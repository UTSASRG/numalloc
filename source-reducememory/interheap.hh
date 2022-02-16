#ifndef __INTER_HEAP_HH__
#define __INTER_HEAP_HH__

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


class InterHeap {

  // This bag is different from PerNodeSizeClassBag
  // There is no need to use lock, and allocation will be just from one heap.
  class PerSizeClassBag {
    private:
    unsigned int _batchSize; 
    char * _bp;
    char * _bpEnd;

    public: 
    void initialize(unsigned int classSize) {
      _bp = NULL;
      _bpEnd = NULL;

      if(classSize < PAGE_SIZE) {
        _batchSize = (PAGE_SIZE/classSize)*classSize;
      }
      else if(classSize < 0x40000) {
        _batchSize = classSize * 2;
      }
      else {
        _batchSize = classSize;
      }
    }

    bool allocate(void ** head, void ** tail) {
      bool success = false; 

      if(_bp < _bpEnd) {
        *head = _bp;

        if(_bp + 2*_batchSize < _bpEnd) {
          // Update the _bp pointer, but not _bpEnd
          _bp += _batchSize;
          *tail = _bp;
        }
        else {
          *tail = _bpEnd;
          _bp = _bpEnd;
        }
        success = true;
      }

      return success;
    }

    unsigned int getBatchSize() {
      return _batchSize;
    }

    void pushRange(void * head, void * tail) {
      if (head != tail) {
        _bp = (char *)head;
        _bpEnd = (char *)tail;
      }
    }
  };

  private:
    char * _begin;
    char * _end;
   
    unsigned int  _nodeIndex;
    bool    _sequentialPhase; 

    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;
    pthread_spinlock_t _lock;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerThreadSizeClassList _smallClasses[SMALL_SIZE_CLASSES];
    PerSizeClassBag   _smallBags[SMALL_SIZE_CLASSES];

    // _bigObjects will also maintain the PerMBINfo
    PerNodeBigObjects _bigObjects;

    // We should get a big chunk at first and do our allocation. 
    // Typically, there is no need to do the deallocation in the main thread.
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
      _sequentialPhase = true;

      // Compute the metadata size for PerNodeHeap
      size_t metasize = _bigObjects.computeImplicitSize(SIZE_PER_SPAN);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
      _bigObjects.initialize(ptr, _bpSmall);

      // Initialize all size classes. 
      unsigned long classSize = 16;
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
        // Allocate batchSize
        _smallBags[i].initialize(classSize);

        _smallClasses[i].initialize(classSize, i);

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
   } 

   void stopSerialPhase() {
    _sequentialPhase = false;
   }

  void startSerialPhase() {
    _sequentialPhase = true;
    _mhSequence++;
  }

  inline size_t getSize(void * ptr) {
    return _bigObjects.getSize(ptr);
  } 

  // allocate a big object with the specified size
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_BAG);

    // Don't allocate from the freelist since we return the memory back to OS immedidately
    ptr = allocateFromBigBumppointer(size);
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

      ptr = (void *)alignup((uintptr_t)ptr, SIZE_HUGE_PAGE);
      _bpBig = (char *)ptr;
    }
    
    //fprintf(stderr, "allocate size %lx _bpBig %p ptr %p\n", size, _bpBig, ptr); 
    _bpBig += size;

    // Allocate the memory from the OS
    ptr = MM::mmapAllocatePrivate(size, ptr, isHugePage);

    // Now binding the memory to different nodes.
    MM::bindMemoryBlockwise((char *)ptr, pages, _nodeIndex, isHugePage);

    // fprintf(stderr, "interheap: markPerMBInfo from allocateFromBigBumppointer ptr=%p, size=%lx\n", ptr, size);
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
    // fprintf(stderr, "interheap: markPerMBInfo from allocateOneBag ptr=%p, classSize=%lx, bagSize=%lx\n", ptr, classSize, bagSize);
    // Update the PerMBInfo to mark the size class.
    markPerMBInfo(ptr, bagSize, classSize);

    return ptr;
  }

  PerThreadSizeClassList * getPerThreadSizeClassList(size_t size) {
    int scIndex = size2Class(size);
    return &_smallClasses[scIndex];
  }

  void * allocateSmallObject(size_t size) {
    PerThreadSizeClassList * sc;
    void * ptr; 

    // If it is small object, allocate from the freelist at first.
    sc = getPerThreadSizeClassList(size);

    int numb = 0;
    size_t classSize = sc->getClassSize();
    if(sc->hasItems() != true) {
      unsigned int classIndex = sc->getClassIndex(); 

      // Get objects from the corresponding bag.
      void * head = NULL;
      void * tail = NULL;
      bool success = _smallBags[classIndex].allocate(&head, &tail);

      // If success, then the remaining memory in the bag will be at least larger than batchSize.
      if(!success) {
        int bagSize = (classSize < SIZE_ONE_BAG) ? SIZE_ONE_BAG : (classSize / SIZE_ONE_BAG) * SIZE_ONE_BAG;
        // No more space left in the bag, then obtain a new block.
        char * bpPointer = (char *)allocateOneBag(bagSize, classSize);

        head = bpPointer;
        tail = bpPointer + bagSize; //_smallBags[classIndex].getBatchSize(); a bug here previously
        // fprintf(stderr, "Put range (%p~%p) into _smallBags\n", tail, bpPointer + bagSize);

        // Push the remaining memory back to the bag.
        _smallBags[classIndex].pushRange(tail, bpPointer + bagSize);
      }
      // fprintf(stderr, "Put range (%p~%p) into _smallClasses\n", (unsigned long)head + classSize, tail);
      // Now the remaining range to the freelist, except the first one
      sc->pushRange((void *)((unsigned long)head + classSize), tail);

      ptr = head; 
    }
    else { 
      ptr = sc->allocate();
    }
    // fprintf(stderr, "Allocate small object with size %lx ptr %p (sc=%d, size=%lx)\n", size, ptr, sc->getClassIndex(), classSize);
    return ptr;
  }

  void * allocate(size_t size);
  void deallocate(void * ptr);
};
#endif
