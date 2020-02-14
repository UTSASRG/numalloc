#ifndef __PER_NODE_HEAP_HH__
#define __PER_NODE_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodesizeclass.hh"
#include "perthread.hh"

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
    dlist   list; // We will use the doubly-link list for the performance reason
    size_t  size;
    unsigned long timestamp;
  };


  private:
    char * _nodeBegin;
    char * _nodeEnd;
    
    // _bpSmall points to the beginning of available objects. 
    // Each allocation will be aligned to 1 MB (either for big objects and small objects).
    // So that the big objects can be used for small objects, and vice versa.
    // The lock is to protect the updates of _bpSmall
    pthread_spinlock_t _lockSmall;
    pthread_spinlock_t _lockBig;
    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;

    size_t _scMagicValue; 
    int   _nodeindex;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerNodeSizeClass  _smallSizes[SMALL_SIZE_CLASSES];
    
    // Larger than 1MB will be located in the MB list
    dlistHead _bigList; // the doubly linklist 
    size_t    _bigSize; // total size
    
    // If the total size is larger than this watermark, then we will start to freed some memory
    // Currently, the watermark is set to 128MB
    size_t _bigWatermark; 
    unsigned long  _bigTimestamp; 

    unsigned long * _mbsMapping;

 public:
   size_t computeMetadataSize(size_t heapsize) {
      // Comput the size for _mbsMapping
      return sizeof(unsigned long) * (heapsize >> SIZE_ONE_MB_SHIFT);
   }

   void initialize(int nodeindex, char * start) {
      size_t heapsize = 2 * SIZE_PER_SPAN;
      // Save the heap related information
      _nodeBegin = start; 
      _nodeEnd = start + heapsize;
      _nodeindex = nodeindex; 
     
      // Initialize the heap for small objects
      _bpSmall = (char *)MM::mmapFromNode(SIZE_PER_SPAN, nodeindex, (void *)start, false); 
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;

      // Initialize the heap for big objects, which will be allocated using huge pages.
      _bpBig = (char *)MM::mmapFromNode(SIZE_PER_SPAN, nodeindex, _bpSmallEnd, true); 
      _bpBigEnd = _bpBig + SIZE_PER_SPAN;
      _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE);
      
      _bigSize = 0;
      _bigWatermark = BIG_OBJECTS_WATERMARK;
      _bigTimestamp = 0;

      // Compute the metadata size for PerNodeHeap
      size_t metasize = computeMetadataSize(heapsize);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
  
      // Set the starting address of _mbsMapping
      _mbsMapping = (unsigned long *)ptr;

      // Initialization right now.
      pthread_spin_init(&_lockBig, PTHREAD_PROCESS_PRIVATE);
      pthread_spin_init(&_lockSmall, PTHREAD_PROCESS_PRIVATE);

      listInit(&_bigList);
      _bigSize = 0;
      _bigWatermark = BIG_OBJECTS_WATERMARK;
      _bigTimestamp = 0;
      
      // Initialize the _smallSizes 
      int i = 0;
      size_t size = SIZE_CLASS_START_SIZE;
      for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
        _smallSizes[i].initialize(size);

        if(size < SIZE_CLASS_TINY_SIZE) {
          size += 16;
        }
        else if(size < SIZE_CLASS_SMALL_SIZE) {
          size += 32;
        }
        else {
          size *= 2;
        }
      }
  }

  // allocate a big object with the specified size
  void * bigObjectListAllocate(size_t size, size_t realsize) {
    void * ptr = NULL;

    // We only allocate if the total size is larger than the request one
    if(_bigSize < size) {
      return ptr;
    }

    // _bigList is not empty (at least with one element), since _bigSize is not 0
    BigObject * object = NULL;
    bool    isFound = false;
    
    lockBigHeap();

    dlist * entry = _bigList.first;
    // TODO: use skiplist instead of doubly linked list for efficiency
    while(entry != NULL) {
      object = (BigObject *)entry;
      //fprintf(stderr, "search biglist with object %p size %lx request size %lx\n", object, object->size, size);
      if(object->size >= size) {
        isFound = true;
        // Find one available object, which will be allocated from.
        // We already sort the objects based on the size and timestamp
        break;
      }
      entry = entry->next;
    }

    // We will allocate only if the object has been found
    if(isFound) {
      if(object->size > size) {
        //fprintf(stderr, "in bigObjectListAllocate. biglist %p object %p with size %lx request-size %lx\n", &_bigList, object, object->size, size);
        object->size -= size;

        // Remove this object from the list, since it may be inserted into a different place (due to the changed size)
        listRemoveNode(&_bigList, &object->list);

        // Now insert the first part back to the freelist
        insertFreeBigObject(object);

        // Use the second part as the new object.
        ptr = (char *)object + object->size;
      }
      else {
        //fprintf(stderr, "in bigObjectListAllocate. delete object %p size %lx\n", object, size);
        // Remove the object from the freed list
        listRemoveNode(&_bigList, &object->list);
        ptr = (void *)object;
      }

      // Change the total size
      _bigSize -= size;

      // Set the current object to be used
      setUseMbsMapping(ptr, size, realsize);
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

  // Allocate one MB from the current node, and will use it for small objects with size _size
  void * allocateOneBag(size_t size, size_t bagSize) {
    void * ptr = NULL;

    // Check the freed bigObjects at first, since they may be still hot in cache.
    ptr = bigObjectListAllocate(bagSize, size);
      //fprintf(stderr, "get object from BIG OBJECT ptr %p\n", ptr);
    // If there is no freed bigObjects, getting one MB from the bump pointer
    if(ptr == NULL) {
      lockSmallHeap();
      // Allocate one bag from the bump pointer
      ptr = (char *)_bpSmall;
      _bpSmall += bagSize;

      unlockSmallHeap();

      // Set the size for the bag, which don't increase the pageHeapSize.
      setUseMbsMapping(ptr, bagSize, size);
    }
    return ptr;
  }

  // Allocate a big object
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB);

    //fprintf(stderr, "allocateBigObject at node %d: size %lx\n", _nodeindex, size);
    // Try to allocate from the freelist at first
    ptr = bigObjectListAllocate(size, size);
    if(ptr == NULL) {
      //fprintf(stderr, "allocateBigObject at node %d: size %lx\n", _nodeindex, size);
      lockBigHeap();

      // Allocate from bumppointer
      ptr = (char *)_bpBig;
      _bpBig += size;

      unlockBigHeap();
      // We should not consume all memory. If yes, then we should make the heap bigger.
      // Since we don't check normally  to reduce the overhead, we will use the assertion here
      assert(_bpBig < _bpBigEnd);
      // Mark the size in _mbsMapping
      // Mark the size in _mbsMapping
      setUseMbsMapping(ptr, size, size);
    }

    //fprintf(stderr, "allocateBigObject %p: size %lx\n", ptr, size);
    return ptr;
  }

  size_t getSize(void * ptr) {
    size_t offset = ((intptr_t)ptr - (intptr_t)_nodeBegin);
    unsigned long mbIndex = offset >> SIZE_ONE_MB_SHIFT;
    // Check the 1MB information in order to find the size. 
    return getSizeFromMbs(mbIndex); 
  }

  inline size_t getSizeFromMbs(unsigned long index) {
    return (_mbsMapping[index] >> 1);
  }

  bool isSmallObject(size_t size) {
    return (size <= BIG_OBJECT_SIZE_THRESHOLD) ? true : false;
  }
  
  void deallocate(int nodeindex, void * ptr) {
     // Get the size of this object
     size_t size = getSize(ptr);

     // For a small object, 
     if(isSmallObject(size)) {
       // If the node index is the same as the current thread, 
       // Return this object to per-thread's cache
       int index = getNodeIndex(); 
       int sc = size2Class(size);

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
     else {
       bigObjectsDeallocate(ptr, size);
     }
   }

    inline void setFreeMbsMapping(unsigned long index, unsigned long last, size_t size) {
    _mbsMapping[index] = (size <<1) | 0x1;
    _mbsMapping[last] = (size <<1) | 0x1;
  }

  inline void setFreeMbsMapping(void * ptr, size_t size) {
    unsigned long index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;
    unsigned long mbs = size >> SIZE_ONE_MB_SHIFT;
    setFreeMbsMapping(index, index + mbs - 1, size);
  }

  inline void setUseMbsMapping(unsigned long index, unsigned long last, size_t size) {
    _mbsMapping[index] = (size <<1);
    _mbsMapping[last] = (size <<1);
  }

  inline void setUseMbsMapping(void * ptr, size_t size, size_t realsize) {
    unsigned long index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;
    unsigned long mbs = size >> SIZE_ONE_MB_SHIFT;
    setUseMbsMapping(index, index + mbs - 1, realsize);
  }

  inline void markFreeMbsMapping(unsigned long index) {
    // When setting the index, we will set the first one and the last one
    _mbsMapping[index] |= 0x1;
  }

  inline bool isBigObjectFree(unsigned long index) {
    return ((_mbsMapping[index] & 0x1) != 0);
  }

  inline unsigned long getPrevFreeMbs(unsigned long mbIndex, size_t * size) {
    unsigned long ret = -1;

    if(isBigObjectFree(mbIndex - 1)) {
      *size = getSizeFromMbs(mbIndex-1);
      ret = mbIndex - ((*size) >> SIZE_ONE_MB_SHIFT);
      //fprintf(stderr, "size is %lx ret %d\n", *size, ret);
    }

    return ret;
  }

  inline void * getBigObject(unsigned long index) {
    return _nodeBegin + (index * SIZE_ONE_MB);
  }

  inline void clearFreeMbsMapping(unsigned long index, unsigned long last) {
    _mbsMapping[index] = 0;
    _mbsMapping[last] = 0;
  }


  void * removeBigObject(unsigned long index, unsigned long last) {
    // Turn the index into the address
    void * ptr = getBigObject(index);

    // Remove this object from the freelist
    listRemoveNode(&_bigList, (dlist *)ptr);

    // Cleanup the size information
    clearFreeMbsMapping(index, last);

    return ptr;
  }

  void insertFreeBigObject(BigObject * object) {
    size_t size = object->size;

    // Insert this object to the freelist based on the size and timestamp
    // If there are multiple objects in the freelist, we will use the timestamp.
    // The one with the larger timestamp (which indicates the newer one) will be placed in the header
    dlist * node = _bigList.first;
    bool isFound = false;
    // Find a good placement to insert this object
    while(node != NULL) {
      BigObject * lobject = (BigObject *)node;
      if((lobject->size > size) || ((lobject->size == size) && (lobject->timestamp < object->timestamp)))  {
        isFound = true;
        break;
      }

      node = node->next;
    }

    if(isFound == true) {
      listInsertBefore(&_bigList, node, (dlist *)object);
    }
    else {
      insertListEnd(&_bigList, (dlist *)object);
    }
    // Set the free status and size of the big object
    setFreeMbsMapping((void *)object, size);
  }

  void bigObjectsDeallocate(void * ptr, unsigned long mysize) {
    BigObject * object;
    unsigned long mbIndex = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_MB_SHIFT;;
    unsigned long findex = mbIndex;

    lockBigHeap();

    // We only merge one object if it is larger than one mb.
    if(mysize > SIZE_ONE_MB) {
      unsigned long index;
      size_t size;

      // Try to merger with its previous mbs.
      index = getPrevFreeMbs(mbIndex, &size);
      if(index != -1) {
      //  fprintf(stderr, "bigobject deallocate ptr %p. mbIndex %d index %d\n", ptr, mbIndex, index);
        // Let's remove the previous object from the freelist.
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

      // Should we compute the timestamp based on the ratio
      object->timestamp = _bigTimestamp++;

      // Try to merge with its next object
      index = mbIndex + (mysize >> SIZE_ONE_MB_SHIFT);
      if(isBigObjectFree(index)) {
        size = getSizeFromMbs(index);
        removeBigObject(index, index + (size >> SIZE_ONE_MB_SHIFT) -1);
        object->size += size;
      }
    }
    else {
      // Make the current ptt as the starting address
      object = (BigObject *)ptr;
      object->size = mysize;
      object->timestamp = _bigTimestamp++;
    }

    // Insert the object to the freelist.
    // The freelist will be sorted first as the size, and then timestamp
    // Therefore, we will always get the first one with the specified size
    insertFreeBigObject(object);

    // Update the size information right now
    _bigSize += mysize;

    unlockBigHeap();
  }

   void lockSmallHeap() {
      pthread_spin_lock(&_lockSmall);
   }

    void unlockSmallHeap() {
      pthread_spin_unlock(&_lockSmall);
    }
   
    void lockBigHeap() {
      pthread_spin_lock(&_lockBig);
   }

    void unlockBigHeap() {
      pthread_spin_unlock(&_lockBig);
    }


};
#endif
