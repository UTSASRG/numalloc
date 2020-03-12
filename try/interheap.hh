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
  private:
    char * _begin;
    char * _end;
   
    unsigned int  _nodeIndex;
    bool    _mainHeapPhase; 

    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;
    pthread_spinlock_t _bigLock;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerSizeClass _sclass[SMALL_SIZE_CLASSES];
    pthread_spinlock_t _smallLock[SMALL_SIZE_CLASSES];

    // _bigObjects will also maintain the PerMBINfo
    PerNodeBigObjects _bigObjects;

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
        pthread_spin_init(&_smallLock[i], PTHREAD_PROCESS_PRIVATE);
      }

      // Initialize the lock
      pthread_spin_init(&_bigLock, PTHREAD_PROCESS_PRIVATE);

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
    int scindex = size2Class(size);

    lockSmall(scindex);
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
      if(classSize > (PAGE_SIZE * NUMA_NODES/2)) {
        size_t pages = classSize >> PAGE_SIZE_SHIFT;

        // Notify the OS to allocate physical memory in the block-wise way
        MM::bindMemoryBlockwise((char *)ptr, pages, _nodeIndex);
      }
    }
    else { 
      ptr = sc->allocateFromFreeList();
    }
    unlockSmall(scindex);

    //if(size > 32 && size < 64)
    //fprintf(stderr, "allocate an object ptr %p size %ld\n", ptr, size);
    return ptr;
  }

void * allocate(size_t size) {
    void * ptr = NULL;

  if(isBigObject(size)) {
    //fprintf(stderr, "Allocate big object with size %lx\n", size);
    lockBig();
    ptr = allocateBigObject(size);
    unlockBig();
  }
  else {
    ptr = allocateSmallObject(size);
  }

  return ptr;
}

void deallocate(void * ptr) {
  size_t size = getSize(ptr);

  if(!isBigObject(size)) {
//    if(size > 32 && size < 64)
//    fprintf(stderr, "deallocation ptr %p size %ld\n", ptr, size);
    int sc = size2Class(size); 
    
    //if(!_mainHeapPhase) { 
      //pthread_spin_lock(&_lock);
      // Return to the size class
      lockSmall(sc);
      _sclass[sc].deallocate(ptr);
      unlockSmall(sc);
      //pthread_spin_unlock(&_lock);
   // }
   // else {
   //   _sclass[sc].deallocate(ptr);
   // }
  }
  else {
  //  if(!_mainHeapPhase) {
      lockBig(); 
      _bigObjects.deallocate(ptr, size);
      unlockBig(); 
      //pthread_spin_unlock(&_lock);
   // }
   // else {
   //   _bigObjects.deallocate(ptr, size);
   // }
  }
}

  void lockSmall(int sc) {
    pthread_spin_lock(&_smallLock[sc]);
  }

  void unlockSmall(int sc) {
    pthread_spin_unlock(&_smallLock[sc]);
  }

  void lockBig() {
    pthread_spin_lock(&_bigLock);
  }

  void unlockBig() {
    pthread_spin_unlock(&_bigLock);
  }
};
#endif
