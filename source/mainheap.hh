#ifndef __MAIN_HEAP_HH__
#define __MAIN_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodebigobjects.hh"
#include "real.hh"
#include "hashmap.hh"
#include "hashfuncs.hh"
#include "perthread.hh"
#include "spinlock.hh"

/* Basic memory mechanism for main thread.
 
   The memory management of the main thread is different from other threads,
   mainly because the main thread typically prepare the data for its children 
   threads. Therefore, the memory allocation of the main thread will come from two 
   separate spans that are not always allocated locally. 
   In fact, we are more careful about the load balance issue, due to the fact that all children threads
   are sharing with it.

   However, if an object is larger than PAGE*NUMA_NODES, then we will utilize block-wise 
   allocations, which has been observed by many existing work (such as Xu's PPoPP'14).
  
   Similar to other nodes' allocation, the allocation of main thread will be from two spans. 
   The first half of the perfthreadheap will be completely interleaved, while 
   the second half will be block-wise interleaved method.

   For simplicity, all these objects will be returned to this heap, but will be returned to 
   the os if it is larger than the bag size after the creation of threads.
   We don't want to create unnecessary remote accesses. 

   The allocation of this heap can be only used if there is one thread. In fact, it is important to 
   support this for some openmp program
*/

class MainHeap {
  private:
    char * _begin;
    char * _end;
   
    unsigned int  _nodeIndex;
    bool    _mainHeapPhase; 

    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;
    pthread_spinlock_t _lock;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerSizeClass _sclass[SMALL_SIZE_CLASSES];

    // _bigObjects will also maintain the PerMBINfo
    PerNodeBigObjects _bigObjects;

    class CallsiteInfo {
      bool _isPrivate; // Is known to be private or not.
      int  _allocNum;  // How many allocations in this callsite?

      public:
        CallsiteInfo() {
        }

        void init() {
          _isPrivate = false;
          _allocNum = 1;
        }

        inline bool isPrivateCallsite() {
          return _isPrivate;
        }

        inline void updateAlloc() {
          _allocNum++;
        }

        inline void setPrivateCallsite() {
          _isPrivate = true;
        }
    };

    // We should get a big chunk at first and do our allocation. 
    // Typically, there is no need to do the deallocation in the main thread.
    class localAllocator {
      public:
      static void * allocate(size_t size) {
        return Real::malloc(size);
      }

      static void free(void *ptr) {
        return Real::free(ptr);
      }
    };

    // CallsiteMap is used to save the callsite and corresponding information (whether it is
    // private or not).
    typedef HashMap<void *, CallsiteInfo, spinlock, localAllocator> CallsiteMap;
    CallsiteMap _callsiteMap;

    class ObjectInfo {
    public:
      int            _allocSequence;
      CallsiteInfo * _callsiteInfo;

      ObjectInfo(int sequence, CallsiteInfo * info) {
        _allocSequence = sequence;
        _callsiteInfo = info;
      }

      int getSequence() {
        return _allocSequence;
      }

      CallsiteInfo * getCallsiteInfo() {
        return _callsiteInfo;
      }
    };
    // ObjectsMap will save the mapping between the object address and its callsite
    typedef HashMap<void *, ObjectInfo, spinlock, localAllocator> ObjectsHashMap;
    ObjectsHashMap _objectsMap;

    int   _mhSequence;     // main heap phase sequence number
 public:

   void initialize(int nodeindex, void * begin) {
      size_t heapsize = 2 * SIZE_PER_SPAN;

     // Save the heap related information
      _begin = (char *)begin;
      _end = _begin + heapsize;

      _bpSmall = (char *)MM::mmapPageInterleaved(SIZE_PER_SPAN, (void *)begin);
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;

      // Initialize the big heap. Differently, we don't do mmap at first, since 
      // the only way to use huge page is through mmap. Then we will do the allocation 
      // on demand, based on the requirement.
      _bpBig = _bpSmallEnd;
      _bpBigEnd = _bpBig + SIZE_PER_SPAN;  

      _nodeIndex = nodeindex; 
      _mainHeapPhase = true;
      _mhSequence = 0;

      // Compute the metadata size for PerNodeHeap
      size_t metasize = _bigObjects.computeImplicitSize(heapsize);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
      _bigObjects.initialize(ptr, _bpSmall, SIZE_PER_SPAN*2);

      // Initialize all size classes. 
      unsigned long classSize = 16;
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
        _sclass[i].initialize(classSize, i, 0);
      
        if(classSize < SIZE_CLASS_TINY_SIZE) {
          classSize += 16;
        }
        else if(classSize < SIZE_CLASS_SMALL_SIZE) {
          classSize += 32;
        }
        else {
          classSize *= 2;
       }
      }

      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      // Call stack map
      _callsiteMap.initialize(HashFuncs::hashStackAddr, HashFuncs::compareAddr, SIZE_CALL_SITE_MAP);
      _objectsMap.initialize(HashFuncs::hashAllocAddr, HashFuncs::compareAddr, SIZE_HASHED_OBJECTS_MAP);
   } 

   void stopPhase() {
    _mainHeapPhase = false;
   }

  void updatePhase() {
    _mainHeapPhase = true;
    _mhSequence++;
  }

  inline size_t getSize(void * ptr) {
    return _bigObjects.getSize(ptr);
  } 

  // allocate a big object with the specified size
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB);

    // Allocate from the free list at first
    ptr = _bigObjects.allocate(size);
    if(ptr == NULL) {
      // Now allocate from the bump pointer
      ptr = allocateFromBigBumppointer(size);
    }
    return ptr;
  }

  void * allocateFromBigBumppointer(size_t sz) {
    void * ptr = NULL;
    bool isHugePage = false;

    size_t size = sz;
    ptr = _bpBig;
    size_t pages = size/PAGE_SIZE;

    // If it is possible to use huge pages
    if(size > SIZE_HUGE_PAGE * (NUMA_NODES-1)) {
      // Get the number of huge pages at first
      pages = size>>SIZE_HUGE_PAGE_SHIFT;
      if((size & SIZE_HUGE_PAGE_MASK) != 0) {
        pages += 1;
      }

      // Compute the blockSize. We will try to be balanced.
      // Overall, we don't want one node that has 1 more blocks than the others.
      size = pages * SIZE_HUGE_PAGE;
      isHugePage = true;

     // fprintf(stderr, "Size is %lx _bpBig %p ptr %p\n", size, _bpBig, ptr);
      ptr = (void *)alignup((uintptr_t)ptr, SIZE_HUGE_PAGE);
      //fprintf(stderr, "BigObject allocate: size %lx, pages %lx, ptr %p\n", sz, pages, ptr);
      _bpBig = (char *)ptr;
    }
    
    //fprintf(stderr, "allocate size %lx _bpBig %p ptr %p\n", size, _bpBig, ptr); 
    _bpBig += size;

    // Allocate the memory from the OS
    ptr = MM::mmapAllocatePrivate(size, ptr, isHugePage);

    // Now binding the memory to different nodes.
    MM::bindMemoryBlockwise((char *)ptr, pages, _nodeIndex, isHugePage);

    // Mark the object size
    markPerMBInfo(ptr, size, size);
 
    return ptr;
  }

  void markPerMBInfo(void * blockStart, size_t blockSize, size_t size) {
    _bigObjects.markPerMBInfo(blockStart, blockSize, size);
  }


  inline bool isBigObject(size_t size) {
     return (size >= BIG_OBJECT_SIZE_THRESHOLD) ? true : false;
  }

  inline void * allocateOneBag(size_t bagSize, size_t classSize) {
    // There is no need to use the lock, since there is just one thread currently.
    void * ptr = _bpSmall;
    _bpSmall += bagSize;

    // Update the PerMBInfo to mark the size class.
    markPerMBInfo(ptr, bagSize, classSize);

    return ptr;
  }

  PerSizeClass * getPerSizeClass(size_t size) {
    int scIndex = size2Class(size);
    return &_sclass[scIndex];
  }

  void * allocateSmallObject(size_t size) {
    PerSizeClass * sc;

    // If it is small object, allocate from the freelist at first.
    sc = getPerSizeClass(size);

    int numb = 0;
    void * head = NULL;
    size_t classSize = sc->getClassSize();
    if(sc->hasItems() != true) {
      void * tail = NULL;

      // Get objects from the bump pointer (with the cache warmup mechanism)
      numb = sc->getObjectsFromBumpPointer(&head, &tail);
      if(numb == 0) {
        // Get objects from the bumppointer 
        void * bPtr = allocateOneBag(sc->getBagSize(), classSize);

        // Update the bumppointer
        numb = sc->updateBumpPointerAndGetObjects(bPtr, &head, &tail);
        //fprintf(stderr, "allocate one bag with bPtr %p head %p tail %p\n", bPtr, head, tail);
        assert(numb >= 0);
      }

      if(numb > 1) { 
        // Push these objects into the freelist.
        sc->pushRangeToList(numb, head, tail);
      }
    }

    // Get one object from the list.
    void * ptr = NULL;
    if(numb == 1) {
      ptr = head;
      // Check size. If possible, we will bind the memory in the block-wise. 
      if(classSize > (PAGE_SIZE * 4 * NUMA_NODES)) {
        size_t pages = classSize >> PAGE_SIZE_SHIFT;

        // Notify the OS to allocate physical memory in the block-wise way
        MM::bindMemoryBlockwise((char *)ptr, pages, _nodeIndex);
      }
    }
    else { 
      ptr = sc->allocateFromFreeList();
     // fprintf(stderr, "allocate small object with ptr %p\n", ptr);
    }

    return ptr;
  }

  void * allocate(size_t size);
  void deallocate(void * ptr);
};
#endif
