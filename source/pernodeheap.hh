#ifndef __PER_NODE_HEAP_HH__
#define __PER_NODE_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodesizeclass.hh"
#include "perthread.hh"
#include "dlist.hh"

// Since the array of PerNodeHeap will be allocated from the stack, therefore, 
// we will try to avoid false sharing issue as much as possible. 
// There are two ways to avoid the performance issues: 
// First, we will aligned to 64 bytes. But this really depends on the starting address.  
// Second, we will use all read-only values, so that even there are some writes remotely,
// the cache line won't need to be invalidated. 
class PerNodeHeap {

  // Big objects are those ones with the size larger than 1MB
  class BigObject {
  public:
    list_t  list; // We will use the doubly-link list for the performance reason
    size_t  size;
    unsigned long timestamp; 
  };

  private:
    char * _nodeBegin;
    char * _nodeEnd;
   
    // Pointers to hold never allocated objects. 
    char * _bpSmall; 
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;
    pthread_spinlock_t _smallLock;

    unsigned int _scMagicValue; 
    unsigned int _orderMagicValue; 
    unsigned long _nodeindex;

    // We will have two mappings to track the size information of each object: PerMBSize and PerPageSize. 
    // We will always check the PerMBSize first, and then PerPageSize. 
    // A large object's PerPageSize will be 0, while a small object's PerMBSize will be 0.
    // Since we are using two levels of size informtion, PerPageSize will actually track the actual size, while
    // PerMBSize will only track the MB information
    // Therefore, both mappings will just use the "unsigned short" for each unit.
#if DEBUG 
    unsigned long _allocMBs;
    unsigned long _deallocBig;
    unsigned long _deallocSmall;
#endif    
    // Small objects from 2^4 to 2^11 (2 KB), with 8 classes in total, will be allocated from _smallSizes.
    PerNodeSizeClass  _smallSizes[LIST_SMALL_CLASSES];

    // Objects between 4KB and 512KB will get it from _pageHeap directly (total 8 lists, from order 0 to order 7)
    PerNodeSizeClass  _pageHeap[LIST_PAGE_HEAP];  
    size_t _pageHeapSize;

    // Larger than 1MB will be located in the MB list
    list_t _bigList; // the doubly linklist 
    size_t _bigSize; // total size
    // If the total size is larger than this watermark, then we will start to freed some memory
    // Currently, the watermark is set to 128MB
    size_t _bigWatermark; 
    unsigned long  _bigTimestamp;
    pthread_spinlock_t _bigLock;

    // To save the memory, each page will just use one byte to save the size information and the freed status
    // The size will be just the power (from power 4 to power 19) (only require 5 bits)
    // Particularily, the lowest bit will be status (0--used, 1 -- freed)
    // Small objects's _mbsMapping will be 0. 
    unsigned char  * _pagesMapping;

    // For big objects, their corresponding _pagesMapping will be 0 
    unsigned long  * _mbsMapping;


 public:
   size_t computeMetadataSize(size_t heapsize) {
      size_t size = 0;
      
      // Compute the size for _pagesMapping
      size += sizeof(unsigned char) * (heapsize >> PAGE_SIZE_SHIFT);

      // Comput the size for _mbsMapping
      size += sizeof(unsigned long) * (heapsize >> SIZE_ONE_MB_SHIFT);

      return size;
   }

   void initialize(int nodeindex, char * start) {
      size_t heapsize = 2 * SIZE_PER_SPAN;

      // Save the heap related information
      _nodeBegin = start; 
      _nodeEnd = start + heapsize;
      _nodeindex = nodeindex; 

#if DEBUG
    _allocMBs = 0; 
    _deallocSmall = 0;
    _deallocBig = 0;
#endif
      // Initialize the heap for small objects
      _bpSmall = (char *)MM::mmapFromNode(SIZE_PER_SPAN, nodeindex, (void *)start, false); 
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;

      // Initialize the heap for big objects, which will be allocated using huge pages.
      _bpBig = (char *)MM::mmapFromNode(SIZE_PER_SPAN, nodeindex, _bpSmallEnd, true); 
      _bpBigEnd = _bpBig + SIZE_PER_SPAN;
   
      _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE);
      // For each page, 2^12 (sc will be 8), but the order will be 0, which is 8 less than the sc
      _orderMagicValue = 24 - LOG2(SIZE_CLASS_START_SIZE);

      // Compute the metadata size for PerNodeHeap
      size_t metasize = computeMetadataSize(heapsize);

      // Allocate the memory from the current node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);

      fprintf(stderr, "metasize is %lx\n", metasize);
      // Initilize the mappings
      _pagesMapping = (unsigned char *)ptr;
      ptr += (sizeof(unsigned char) * (heapsize >> PAGE_SIZE_SHIFT));

      // Set the starting address of _mbsMapping
      _mbsMapping = (unsigned long *)ptr;

      // Initialize the _smallSizes 
      int i = 0; 
      size_t size = SIZE_CLASS_START_SIZE;
      for(i = 0; i < LIST_SMALL_CLASSES; i++) {
        _smallSizes[i].initialize(size);
        size *= 2;
      } 

      // Initialize the _pageHeap
      size = PAGE_SIZE;
      for(i = 0; i < LIST_PAGE_HEAP; i++) {
        _pageHeap[i].initialize(size);
        size *= 2;
      } 
   
      // Initialize the management for big objects. 
      listInit(&_bigList);   
      _bigSize = 0; 
      _bigWatermark = BIG_OBJECTS_WATERMARK;
      _bigTimestamp = 0; 
      pthread_spin_init(&_bigLock, PTHREAD_PROCESS_PRIVATE);
      pthread_spin_init(&_smallLock, PTHREAD_PROCESS_PRIVATE);
   } 

  int getSizeClass(size_t size) {
    int sc;

    if(size <= SIZE_CLASS_START_SIZE) {
      sc = 0;
    }
    else {
      sc = _scMagicValue - __builtin_clz(size - 1);
    }
 
    return sc;
  }

  inline int getOrder(size_t size) {
    // Here, size will be not 0
    return _orderMagicValue - __builtin_clz(size - 1);
  }

  // Allocate one mb from big objects
  void * bigObjectAllocateOneMb(void) {
    void * ptr = bigObjectListAllocate(SIZE_ONE_MB);

    // Clear the mapping of this block. Now it will hold small objects only
    clearFreeMbsMapping(ptr);

    return ptr;
  }

  // allocate a big object with the specified size
  void * bigObjectListAllocate(size_t size) {
    void * ptr = NULL;

    // We only allocate if the total size is larger than the request one
    if(_bigSize < size) {
      return ptr;
    }

    lockBigHeap();

    // _bigList is not empty (at least with one element), since _bigSize is not 0
    BigObject * object = NULL;
    list_t * entry = _bigList.next;

    // TODO: use skiplist instead of doubly linked list
    do {
      object = (BigObject *)entry; 
      if(object->size >= size) {
        // Find one available object, which will be allocated from.
        // We already sort the objects based on the size and timestamp
        break; 
      }

      entry = entry->next;
    } while(!isListTail(entry)); 

    // We will allocate only if the object has been found
    if(object->size >= size) {
      if(object->size > size) {
        // Set free for the first part
        setFreeMbsMapping((void *)object, object->size - size);
 
        // Use the second part as the new object, avoiding the change of the link list
        ptr = (char *)object + size;
      }
      else {
        // Remove the object from the freed list
        listRemoveNode(&object->list);
        
        ptr = (void *)object;
      }

      // Change the total size
      _bigSize -= size;

      // Set the current object to be used
      setUseMbsMapping(ptr, size);
    }

    unlockBigHeap();
        
    return ptr;
  }

  int allocateBatch(int sc, unsigned long num, void ** head, void ** tail) {
    return _smallSizes[sc].allocateBatch(num, head, tail);   
  }

  void deallocateBatch(int sc, unsigned long num, void *head, void *tail) {
    _smallSizes[sc].deallocateBatch(num, head, tail);   
  }

  inline unsigned int getPageIndex(void * ptr) {
    size_t offset = ((intptr_t)ptr - (intptr_t)_nodeBegin);
    return offset >> PAGE_SIZE_SHIFT;
  }

  // Mark the size for page heap. Inside the same page, we will  hold 
  // objects with the same size (never changed).
  // Since we don't care the invalid frees right now, we will only set the size for
  // the first page of each PageHeap object 
  inline void setUsePageHeap(void * ptr, unsigned int power) {
    unsigned int pageIndex = getPageIndex(ptr);

    // We will save the power with one bit shift
    _pagesMapping[pageIndex] = power << 1;
  }

  inline void setFreePageHeap(void * ptr, unsigned int power) {
    unsigned int pageIndex = getPageIndex(ptr);

    // We will save the power with one bit shift
    _pagesMapping[pageIndex] = ((power << 1) | 0x1);
  }

  void * splitPageHeapObject(void * ptr, unsigned int order, unsigned int reqOrder, unsigned int power) {
    char * addr = NULL;

    assert(order <= ORDER_ONE_MB);

    while(--order >= reqOrder) {
      unsigned long newPower = order + PAGE_HEAP_START_POWER;

      // Adding the second part to the free list. 
      addr = (char *)ptr + (2 << newPower);
      
      // Set freed status for the second part
      setFreePageHeap(addr, newPower);

      // Add the object to the freelist (lock will be acquired inside)
      _pageHeap[order].deallocate(addr);
    }

    // The first part will be used for the allocation
    setUsePageHeap(ptr, power);
   
    return ptr; 
  }

  // Allocate an object from the page heap directly
  void * pageHeapAllocate(unsigned int order, unsigned int power) {
    unsigned int reqOrder = order; 

    void * ptr = NULL; 

    // Get one bag from the page heap at first
    while(order <= MAX_ORDER) {
      if(_pageHeap[order].getSize() > 0) {
        // Allocate one object from the specified page heap list
        ptr = _pageHeap[order].allocate(); 
        break;
      }

      // Otherwise, try to get one object from the higher order
      order++;
    }

    // Get one object from the higher order list
    if(ptr != NULL) {
      if(order == reqOrder) {
        setUsePageHeap(ptr, power);
      }
      else {
        splitPageHeapObject(ptr, order, reqOrder, power);
      }
    }
    
    return ptr;
  }

  inline unsigned int getPower(size_t size) {
    return getSizeClass(size) + SIZE_CLASS_START_POWER;
  }

  inline unsigned long getPageHeapSize() {
    return __atomic_load_n(&_pageHeapSize, __ATOMIC_RELAXED);
  }
  
  inline void increasePageHeapSize(size_t size) {
    __atomic_fetch_add(&_pageHeapSize, size, __ATOMIC_RELAXED);
  }
  
  inline void decreasePageHeapSize(size_t size) {
    __atomic_sub_fetch(&_pageHeapSize, size, __ATOMIC_RELAXED);
  }

  // Allocate one MB from the current node, and will use it for small objects with size _size
  void * allocateOneBag(size_t size, size_t bagSize) {
    void * ptr = NULL; 

    // Get one bag from the _pageHeap first
    unsigned int order = getOrder(bagSize);
    unsigned int power = getPower(size);

    // If the _pageHeap has some objects, try to allocate from it first.
    if(bagSize <= getPageHeapSize()) {
      ptr = pageHeapAllocate(order, power); 
    }
    
    // If _pageHeap can't allocate it, try other cases.
    if(ptr == NULL) {
      // Check the freed bigObjects at first, since they may be still hot in cache. 
      ptr = bigObjectAllocateOneMb(); 
    
      // If there is no freed bigObjects, getting one MB from the bump pointer
      if(ptr == NULL) {
        lockSmallHeap();

        // Allocate one bag from the bump pointer
        ptr = (char *)_bpSmall;
        _bpSmall += bagSize;

        unlockSmallHeap();

        // Set the size for the bag, which don't increase the pageHeapSize. 
        setUsePageHeap(ptr, power);
      }
      else {
        // Adding the remaining one to the PageHeap, which should return ptr.
        splitPageHeapObject(ptr, ORDER_ONE_MB, order, power);
        increasePageHeapSize(SIZE_ONE_MB - bagSize);
      }
    }
    else {
      // If allocated from _pageHeap, decrease the size
      decreasePageHeapSize(bagSize);
    } 

    return ptr; 
  }

  // Allocate a big object
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB);

    //fprintf(stderr, "allocateBigObject at node %d: size %lx\n", _nodeindex, size);
    // Try to allocate from the freelist at first
    ptr = bigObjectListAllocate(size);
    if(ptr == NULL) {
      //fprintf(stderr, "allocateBigObject at node %d: size %lx\n", _nodeindex, size);
      lockBigHeap();

      // Allocate from bumppointer
      ptr = (char *)_bpBig;
      _bpBig += size;

      unlockBigHeap();
#if DEBUG
      //_allocMBs += size/SIZE_ONE_MB_BAG;
#endif
      // We should not consume all memory. If yes, then we should make the heap bigger.
      // Since we don't check normally  to reduce the overhead, we will use the assertion here
      assert(_bpBig < _bpBigEnd);
      // Mark the size in _mbsMapping
      setUseMbsMapping(ptr, size);
    }

    return ptr;
  }

  unsigned int getPowerPagesMapping(unsigned int index) {
    return (_pagesMapping[index] >> 1);
  }


  size_t getSize(void * ptr) {
    size_t size; 
    size_t offset = ((intptr_t)ptr - (intptr_t)_nodeBegin);
    unsigned long mbIndex = offset >> SIZE_ONE_MB_SHIFT;
    if(_mbsMapping[mbIndex] == 0) {
      size = 2 << getPowerPagesMapping(offset >> PAGE_SIZE_SHIFT);
    }
    else {
      size = getSizeFromMbs(mbIndex); 
    }

    return size;
  }

  void deallocate(int nodeindex, void * ptr) {
    // Get the size of this object
    size_t size;

    size_t offset = ((intptr_t)ptr - (intptr_t)_nodeBegin);
    unsigned long mbIndex = offset >> SIZE_ONE_MB_SHIFT;
    if(_mbsMapping[mbIndex] == 0) {
      // Now it is a small object
      unsigned long pgIndex = offset >> PAGE_SIZE_SHIFT;
      unsigned int  power; 

      power = getPowerPagesMapping(pgIndex);
      size = 2 << power; 

      // Check whether the perthreadsizeclass is full or not
      if(size <= MAX_SMALL_CLASSES) {
        // If the node index is the same as the current thread, 
        // Return this object to per-thread's cache
        int index = getNodeIndex(); 
        int sc = getSizeClass(size);

        if(index == nodeindex) {
          // Only return an object to perthread's heap if the object is from the same node
          current->ptheap->deallocate(ptr, sc);
        }
        else {
          // Return this object to the current node's freelist for different size classes
          // Based on NumaHeap, this address belongs to the current node.
          _smallSizes[sc].deallocate(ptr);
        }
      }
      else if(size <= MAX_PAGE_HEAP) {
        unsigned int order = power - PAGE_HEAP_START_POWER; 

        // Now let's put this object to the common _pageHeap
        pageHeapDeallocate(ptr, order, pgIndex);

        // increase the size.
        increasePageHeapSize(size); 
      }
    }
    else {
      // Deallocate this big object to _bigList
      bigObjectsDeallocate(ptr, mbIndex);
    }
  }

  // Deallocate this object to the page heap, which hold the list of "power of 2" pages
  void pageHeapDeallocate(void * ptr, unsigned int order, int pageIndex) {
#if 0
    // Implement the buddy mechanism as Linux 
    unsigned long index, page_idx, mask, flags;
    //struct page *base;

    mask = (~0UL) << order;

    index = page_idx >> (1 + order);
    while (mask + (1 << (MAX_ORDER-1))) {
      struct page *buddy1, *buddy2;

      if (!__test_and_change_bit(index, area->map))
      /*
       * the buddy page is still allocated.
       */
      break;
    /*
     * Move the buddy up one level.
     * This code is taking advantage of the identity:
     *  -mask = 1+~mask
     */
    buddy1 = base + (page_idx ^ -mask);
      buddy2 = base + page_idx;

          list_del(&buddy1->list);
    mask <<= 1;
    area++;
    index >>= 1;
    page_idx &= mask;
  } 
  list_add(&(base + page_idx)->list, &area->free_list);
#endif
    _pageHeap[order].deallocate(ptr);

    // Mark this object as freed. 
    markFreePageObject(pageIndex);  
  }

  inline void markFreePageObject(unsigned int index) {
    _pagesMapping[index] |= 0x1;
  }

  inline void clearFreeMbsMapping(void * ptr) {
    unsigned int index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;
    _mbsMapping[index] = 0;
  }

  inline void clearFreeMbsMapping(unsigned int index, unsigned int last) {
    _mbsMapping[index] = 0;
    _mbsMapping[last] = 0;
  }

  inline void setFreeMbsMapping(unsigned int index, unsigned int last, size_t size) {
    _mbsMapping[index] = (size <<1) | 0x1;
    _mbsMapping[last] = (size <<1) | 0x1;
  }

  inline void setFreeMbsMapping(void * ptr, size_t size) {
    unsigned int index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;
    unsigned int mbs = size >> SIZE_ONE_MB_SHIFT;
    setFreeMbsMapping(index, index + mbs, size);
  }
  
  inline void setUseMbsMapping(unsigned int index, unsigned int last, size_t size) {
    _mbsMapping[index] = (size <<1);
    _mbsMapping[last] = (size <<1);
  }

  inline void setUseMbsMapping(void * ptr, size_t size) {
    unsigned int index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;
    unsigned int mbs = size >> SIZE_ONE_MB_SHIFT;
    setUseMbsMapping(index, index + mbs, size);
  }

  inline void markFreeMbsMapping(unsigned int index) {
    // When setting the index, we will set the first one and the last one
    _mbsMapping[index] |= 0x1;
  }

  inline bool isBigObjectFree(unsigned int index) {
    return ((_mbsMapping[index - 1] & 0x1) != 0);
  }

  inline int getPrevFreeMbs(unsigned int mbIndex, size_t * size) {
    int ret = -1; 

    if(isBigObjectFree(mbIndex - 1)) {
      *size = getSizeFromMbs(mbIndex-1);
      ret = mbIndex - ((*size) >> SIZE_ONE_MB_SHIFT);
    }

    return ret; 
  }

  inline size_t getSizeFromMbs(unsigned int index) {
    return (_mbsMapping[index] >> 1);
  }

  inline void * getBigObject(unsigned int index) {
    return _nodeBegin + (index << SIZE_ONE_MB_SHIFT);
  }

  void * removeBigObject(unsigned int index, unsigned long last) {
    // Turn the index into the address 
    void * ptr = getBigObject(index);

    // Remove this object from the freelist
    listRemoveNode((list_t *)ptr);

    // Cleanup the size information
    clearFreeMbsMapping(index, last);     

    return ptr;
  }

  void insertBigObject(BigObject * object, unsigned int findex) {
    size_t size = object->size;
    unsigned int mbs = size >> SIZE_ONE_MB_SHIFT;

    // TODO: check it carefully
    // Insert this object to the freelist based on the size
    // If there are multiple objects in the freelist, then the current one
    // will be inserted into the beginning
    // Therefore, we will find the first object that satisfy the condition
    list_t * entry = _bigList.next;
    while(!isListTail(entry)) {
      BigObject * lobject = (BigObject *)entry; 
      if(lobject->size >= size) {
        break; 
      } 

      entry = entry->next;
    }
    listInsertNode(&object->list, entry); 

    // Mark the setting of big objects
    markFreeMbsMapping(findex);

    // Also mark the last one
    if(size > SIZE_ONE_MB) {
      markFreeMbsMapping(findex+ (size>> SIZE_ONE_MB_SHIFT));
    }
  }

  void bigObjectsDeallocate(void * ptr, unsigned int mbIndex) {
    BigObject * object; 
    unsigned int findex = mbIndex;
    size_t mysize = getSizeFromMbs(mbIndex);

    lockBigHeap();

    // We only merge one object if it is larger than one mb.
    if(mysize > SIZE_ONE_MB) {
      unsigned int index;
      size_t size; 

      // Try to merger with its previous mbs. 
      index = getPrevFreeMbs(mbIndex, &size);
      if(index != -1) {
        // Let's remove the previous object. 
        object = (BigObject *)removeBigObject(index, mbIndex-1);

        // Now let's combine with the current object. 
        object->size = size + mysize;
        findex = index;
      }
      else {
        // Make the current prt as the starting address
        object = (BigObject *)ptr;
        object->size = mysize;
      }
        
      object->timestamp = _bigTimestamp++;

      // Try to merge with its next object
      index = mbIndex + (mysize >> SIZE_ONE_MB_SHIFT) + 1;
      if(isBigObjectFree(index)) {
        size = getSizeFromMbs(index);
        removeBigObject(index, index + (size >> SIZE_ONE_MB_SHIFT)); 
        object->size += size;
      }
    }
    else {
      // Make the current prt as the starting address
      object = (BigObject *)ptr;
      object->size = mysize;
    }
      
    // Insert the object to the freelist. 
    // The freelist will be sorted first as the size, and then timestamp
    // Therefore, we will always get the first one with the specified size
    insertBigObject(object, findex);
  
    // Update the size information right now
    _bigSize += mysize;
 
    unlockBigHeap();
  }

 private:
    void lockBigHeap() {
      pthread_spin_lock(&_bigLock);
    }

    void unlockBigHeap() {
      pthread_spin_unlock(&_bigLock);
    }
    
    void lockSmallHeap() {
      pthread_spin_lock(&_smallLock);
    }

    void unlockSmallHeap() {
      pthread_spin_unlock(&_smallLock);
    }
};
#endif
