#ifndef __PER_NODE_HEAP_HH__
#define __PER_NODE_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodesizeclasslist.hh"
#include "pernodesizeclassbag.hh"
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
    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;   

    // The lock is to protect the updates of _bpSmall
    pthread_spinlock_t _lockSmall;
    pthread_spinlock_t _lockBig;

    int   _nodeindex;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerNodeSizeClassBag   _smallBags[SMALL_SIZE_CLASSES];
    PerNodeSizeClassList  _smallLists[SMALL_SIZE_CLASSES];
    
    // Larger than 1MB will be located in the MB list
    dlistHead _bigList; // the doubly linklist 
    size_t    _bigSize; // total size
    
    // If the total size is larger than this watermark, then we will start to freed some memory
    // Currently, the watermark is set to 128MB
    size_t _bigWatermark; 
    unsigned long  _bigTimestamp; 

    // Store the size info of small objects
    unsigned long * _mbsMapping;

    // TODO: Hash map to store the size info of big objects

 public:
   size_t computeMetadataSize(size_t heapsize) {
      // Comput the size for _mbsMapping
      return sizeof(unsigned long) * (heapsize >> SIZE_ONE_BAG_SHIFT);
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
      
      _bigSize = 0;
      _bigWatermark = BIG_OBJECTS_WATERMARK;
      _bigTimestamp = 0;

      // Compute the metadata size for PerNodeHeap
      // TODO: divide 2
      size_t metasize = computeMetadataSize(heapsize);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
  
      // Set the starting address of _mbsMapping
      _mbsMapping = (unsigned long *)ptr;

      // TODO: initialize the hashmap for big object size info

      // Initialization right now.
      pthread_spin_init(&_lockBig, PTHREAD_PROCESS_PRIVATE);
      pthread_spin_init(&_lockSmall, PTHREAD_PROCESS_PRIVATE);

      listInit(&_bigList);

      _bigSize = 0;
      _bigWatermark = BIG_OBJECTS_WATERMARK;
      _bigTimestamp = 0;
      
      // fprintf(stderr, "pernodeheap.hh: initialize _smallBags(PerNodeSizeClassBag) and _smallLists(PerNodeSizeClassList)`\n");
      // Initialize the _smallLists and _smallBags
      int i = 0;
      size_t size = SIZE_CLASS_START_SIZE;
      for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
        // fprintf(stderr, "i=%d, classSize=%d\n", i, size);
        _smallBags[i].initialize(nodeindex, size);

        _smallLists[i].initialize(size);

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
  void * allocateFromBigFreeList(size_t size, size_t realsize) {
    // fprintf(stderr, "pernodeheap.hh: allocateFromBigFreeList size=%lx, realsize=%lx\n", size, realsize);
    void * ptr = NULL;

    // We only allocate if the total size is larger than the request one
    if(_bigSize < size) {
      return ptr;
    }

    // _bigList is not empty (at least with one element), since _bigSize is not 0
    BigObject * object = NULL;
    bool isFound = false;
    
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

    // Allocate only if the object has been found
    if(isFound) {
      if(object->size > size) {
        object->size -= size;

        // Remove it from the list, since it may be inserted into a different place (due to the changed size)
        listRemoveNode(&_bigList, &object->list);

        // Now insert the first part back to the freelist
        insertFreeBigObject(object);

        // Use the second part as the new object.
        ptr = (char *)object + object->size;
      }
      else {
        // Remove the object from the freed list (with the exact size)
        listRemoveNode(&_bigList, &object->list);
        ptr = (void *)object;
      }

      // Change the total size
      _bigSize -= size;

      // Set the current object to be used
      // TODO use hash map instead of shadow memory
      setUseMbsMapping(ptr, size, realsize);
    }

    unlockBigHeap();

    return ptr;
  }

  int allocateFromSmallFreeList(unsigned int sc, void ** head, void ** tail) {
    // Get freed objects in the freelist at first.
    // If successful, we will just return the numb. 
    int numb = _smallLists[sc].allocateBatch(head, tail);

    if(numb == 0) {
      // fprintf(stderr, "pernodeheap.hh:: allocate size class %d from _smallBags(PerNodeSizeClassBag)\n", sc);
      // If we can't get objects from the freelist, allocate from the bag (never-allocated ones)
      _smallBags[sc].allocate(head, tail);
    } 
    // else {
    //   fprintf(stderr, "pernodeheap.hh:: allocate size class %d from _smallLists(PerNodeSizeClassList)\n", sc);
    // }

    return numb;
  }

  void deallocateBatch(int sc, unsigned long num, void *head, void *tail) {
    // fprintf(stderr, "pernodeheap.hh: deallocate batch from sc=%d\n", sc);
    _smallLists[sc].deallocateBatch(num, head, tail);   
  }

  // Allocate one bag for small objects with the specified classSize.
  // If allocBig is set, we will try to allocate from one of freed big objects (with huge pages)
  // Currently, only the corresponding size class that has allocated one bag before can use big objects 
  void * allocateOneBag(size_t classSize, size_t bagSize, bool allocBig) {
    void * ptr = NULL;

    // Check the freed bigObjects at first, since they may be still hot in cache.
    if(allocBig) {
      // fprintf(stderr, "pernodeheap.hh: allocateOneBag allocBig=true, invoke allocateFromBigFreeList\n");
      //TODO: Be careful!!
      ptr = allocateFromBigFreeList(bagSize, classSize);
    }
    //  fprintf(stderr, "get one bag with size %lx from big object with ptr %p\n", size, ptr);
    // If there is no freed bigObjects, getting one bag from the bump pointer
    if(ptr == NULL) {
      lockSmallHeap();
      // Allocate one bag from the bump pointer
      ptr = (char *)_bpSmall;
      _bpSmall += bagSize;

      unlockSmallHeap();
      // fprintf(stderr, "pernodeheap.hh: allocateOneBag _bpSmall add %lx\n", bagSize);
      // Set the size for the bag, which don't increase the pageHeapSize.
      setUseMbsMapping(ptr, bagSize, classSize);
    }
    return ptr;
  }

  // Allocate a big object
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB); //TODO one bag or one mb?

    // fprintf(stderr, "pernodeheap.hh: allocateBigObject at node %d: size %lx\n", _nodeindex, size);
    // Try to allocate from the freelist at first
    ptr = allocateFromBigFreeList(size, size);
    if(ptr == NULL) {
      // fprintf(stderr, "pernodeheap.hh: allocateBigObject from bumppointer\n");
      lockBigHeap();

      // Allocate from bumppointer
      ptr = (char *)_bpBig;
      _bpBig += size;

      unlockBigHeap();
      // We should not consume all memory. If yes, then we should make the heap bigger.
      // Since we don't check normally  to reduce the overhead, we will use the assertion here
      assert(_bpBig < _bpBigEnd);
      // Mark the size in _mbsMapping
      // TODO use hash map instead of shadow memory
      setUseMbsMapping(ptr, size, size);
    }

    // fprintf(stderr, "pernodeheap.hh:: allocateBigObject size=%lx, ptr=%p\n", size, ptr);
    return ptr;
  }

  size_t getSize(void * ptr) {

    //TODO
    //1. Check if ptr is under big object range
    //2. If so check the hashmap and return

    size_t offset = ((intptr_t)ptr - (intptr_t)_nodeBegin);
    unsigned long mbIndex = offset >> SIZE_ONE_BAG_SHIFT;
    // Check the 1MB information in order to find the size. 
    return getSizeFromMbs(mbIndex); 
  }

  inline size_t getSizeFromMbs(unsigned long index) {
    return (_mbsMapping[index] >> 1);
  }
  
  void deallocate(int nodeindex, void * ptr) {
     // Get the size of this object
     size_t size = getSize(ptr);

     // For a small object, 
     if(size <= BIG_OBJECT_SIZE_THRESHOLD) {
       // If the node index is the same as the current thread, 
       // Return this object to per-thread's cache
       int index = getNodeIndex(); 
       int sc = size2Class(size);
      
       if(index == nodeindex) {
        //  fprintf(stderr, "pernodeheap.hh: deallocate to current per thread heap with ptr=%p and sc=%d\n", ptr, sc);
         // Only return an object to perthread's heap if the object is from the same node
         current->ptheap->deallocate(ptr, sc);
       }
       else {
        //  fprintf(stderr, "pernodeheap.hh: deallocate to _smallLists (node's freelist) with ptr=%p and sc=%d\n", ptr, sc);
        // Return this object to the current node's freelist for different size classes
        // Based on NumaHeap, this address belongs to the current node.
        _smallLists[sc].deallocate(ptr);
       }
     }
     else {
      //  fprintf(stderr, "pernodeheap.hh: bigObjectsDeallocate ptr=%p and sc=%lx\n", ptr, size);
       bigObjectsDeallocate(ptr, size);
     }
   }

  //TODO I will remove setFreeMbsMapping if I use hashmap to store the size info
  inline void setFreeMbsMapping(unsigned long index, unsigned long last, size_t size) {
    _mbsMapping[index] = (size <<1) | 0x1;
    _mbsMapping[last] = (size <<1) | 0x1;
  }

  inline void setFreeMbsMapping(void * ptr, size_t size) {
    unsigned long index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_BAG_SHIFT;
    unsigned long mbs = size >> SIZE_ONE_BAG_SHIFT;
    setFreeMbsMapping(index, index + mbs - 1, size);
  }

  inline void setUseMbsMapping(unsigned long index, unsigned long last, size_t size) {
    _mbsMapping[index] = (size <<1);
    _mbsMapping[last] = (size <<1);
  }

  inline void setUseMbsMapping(void * ptr, size_t size, size_t realsize) {
    unsigned long index = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_BAG_SHIFT;
    unsigned long mbs = size >> SIZE_ONE_BAG_SHIFT;
    setUseMbsMapping(index, index + mbs - 1, realsize);
  }

  //TODO isBigObjectFree is also removed
  inline bool isBigObjectFree(unsigned long index) {
    return ((_mbsMapping[index] & 0x1) != 0);
  }

  //TODO I need to store previous ptr if I use hashmap
  inline unsigned long getPrevFreeMbs(unsigned long mbIndex, size_t * size) {
    unsigned long ret = -1;

    if(isBigObjectFree(mbIndex - 1)) {
      *size = getSizeFromMbs(mbIndex-1);
      ret = mbIndex - ((*size) >> SIZE_ONE_BAG_SHIFT);
      // fprintf(stderr, "mbIndex=%d, size=%lx, ret=%d\n", mbIndex, *size, ret);
    }

    return ret;
  }

  //TODO find the ptr of the big object, I will remove it
  inline void * getBigObject(unsigned long index) {
    return _nodeBegin + (index * SIZE_ONE_BAG);
  }

  //TODO remove
  inline void clearFreeMbsMapping(unsigned long index, unsigned long last) {
    _mbsMapping[index] = 0;
    _mbsMapping[last] = 0;
  }


  void * removeBigObject(unsigned long index, unsigned long last) {
    // fprintf(stderr, "pernodeheap.hh: removeBigObject from _bigList\n");
    //TODO
    //1. Get big object ptr from the argument
    //2. Add the object to the bigList
    //3. Mark the big object is free

    // Turn the index into the address
    void * ptr = getBigObject(index);

    // Remove this object from the freelist
    listRemoveNode(&_bigList, (dlist *)ptr);

    // Cleanup the size information
    clearFreeMbsMapping(index, last);

    return ptr;
  }

  void insertFreeBigObject(BigObject * object) {
    // fprintf(stderr, "pernodeheap.hh: insertFreeBigObject to _bigList\n");
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
    //TODO Update the hash map
    // Set the free status and size of the big object
    setFreeMbsMapping((void *)object, size);
  }

  void bigObjectsDeallocate(void * ptr, unsigned long mysize) {
    // fprintf(stderr, "pernodeheap.hh: bigObjectsDeallocate size=%lx\n", mysize);
    BigObject * object;
    unsigned long mbIndex = ((intptr_t)ptr - (intptr_t)_nodeBegin) >> SIZE_ONE_BAG_SHIFT;;
    unsigned long findex = mbIndex;

    lockBigHeap();

    // We only merge one object if it is larger than one mb.
    if(mysize > SIZE_ONE_MB) {
      unsigned long index;
      size_t size;

      // Try to merger with its previous mbs.
      index = getPrevFreeMbs(mbIndex, &size); //TODO
      if(index != -1) {
      //  fprintf(stderr, "bigobject deallocate ptr %p. mbIndex %d index %d\n", ptr, mbIndex, index);
        // Let's remove the previous object from the freelist.
        object = (BigObject *)removeBigObject(index, mbIndex-1); //TODO pass the pointer
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

      // Try to merge with its next object TODO easy to find the next big object
      index = mbIndex + (mysize >> SIZE_ONE_BAG_SHIFT);
      if(isBigObjectFree(index)) {
        size = getSizeFromMbs(index);
        removeBigObject(index, index + (size >> SIZE_ONE_BAG_SHIFT) -1);
        object->size += size;
      }
    }
    else {
      // Make the current ptr as the starting address
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

    //fprintf(stderr, "_bigSize %lx\n", _bigSize);
    // TODO: return some objects back to the OS if necessary
    unlockBigHeap();
  }

  inline void lockSmallHeap() {
    pthread_spin_lock(&_lockSmall);
  }

  inline void unlockSmallHeap() {
    pthread_spin_unlock(&_lockSmall);
  }
   
  inline void lockBigHeap() {
    pthread_spin_lock(&_lockBig);
  }

  inline void unlockBigHeap() {
    pthread_spin_unlock(&_lockBig);
  }


};
#endif
