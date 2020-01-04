#ifndef __MAIN_HEAP_HH__
#define __MAIN_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodebigobjects.hh"
#include "real.hh"
#include "perthread.hh"

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
  
  class MainThreadSizeClass {
  private:
    unsigned int _csize;
    unsigned int _sc;
    unsigned int _pages;
    unsigned int _nodeindex;
    char        * _bumpPointer;
    char        * _bumpPointerEnd;
    FreeList     _flist;

  public:
    
    void initialize(unsigned int sc, unsigned int classsize, unsigned int nodeindex) {
      _sc = sc;
      _csize = classsize;
      _nodeindex = nodeindex;
      _pages = classsize >> PAGE_SIZE_SHIFT;
      _bumpPointer = NULL;
      _bumpPointerEnd = NULL;
      _flist.Init();
    }

    bool hasObject() {
      return ((_flist.length() > 0) || (_bumpPointer < _bumpPointerEnd));
    }

    // The current object will be pushed with a new block of memory
    void updateBumpPointer(char * start, size_t size) {
      _bumpPointer = start;
      _bumpPointerEnd = start + size;
    }

    unsigned int getClassSize() {
      return _csize;
    }

    void * allocate(void) {
      void * ptr = NULL; 

      // Allocate from the freelist first.
      if(_flist.hasItems()) { 
        ptr = _flist.Pop(); 
      }
      else { 
        // Allocate from the bump pointer right now. 
        ptr = _bumpPointer;
        _bumpPointer += _csize;

        // Notify the OS to allocate physical memory in the block-wise way
        MM::bindMemoryBlockwise((char *)ptr, _pages, _nodeindex);
      }

      return ptr;
    }

    void deallocate(void * ptr) {
      _flist.Push(ptr);
    }
  }; 
    

  private:
    char * _begin;
    char * _end;
   
    unsigned int  _nodeindex;
    bool    _mainHeapPhase; 
    size_t  _scMagicValue; 

    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpBig;
    char * _bpBigEnd;
    pthread_spinlock_t _lock;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    MainThreadSizeClass _sizes[SMALL_SIZE_CLASSES];

    // _bigObjects will also maintain the PerMBINfo
    PerNodeBigObjects _bigObjects;

 public:

   void initialize(int nodeindex, void * begin) {
      size_t heapsize = 2 * SIZE_PER_SPAN;

     // Save the heap related information
      _begin = (char *)begin;
      _end = _begin + heapsize;

      //_bpSmall = (char *)MM::mmapFromNode(SIZE_PER_SPAN, nodeindex, (void *)begin, false);
    //  _bpSmall = (char *)MM::mmapAllocatePrivate(SIZE_PER_SPAN, (void *)begin);
      _bpSmall = (char *)MM::mmapPageInterleaved(SIZE_PER_SPAN, (void *)begin);
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;

      // Initialize the big heap. Differently, we don't do mmap at first, since 
      // the only way to use huge page is through mmap. Then we will do the allocation 
      // on demand, based on the requirement.
      _bpBig = _bpSmallEnd;
      _bpBigEnd = _bpBig + SIZE_PER_SPAN;  

      _nodeindex = nodeindex; 
      _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE);
     
      // Compute the metadata size for PerNodeHeap
      size_t metasize = _bigObjects.computeImplicitSize(heapsize);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
      _bigObjects.initialize(ptr, _bpSmall, SIZE_PER_SPAN*2);

      // Initialize all size classes. 
      unsigned long classSize = 16;
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
        // Each element of
        _sizes[i].initialize(i, classSize, nodeindex);

        classSize *= 2;
      }

      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
   } 

   inline size_t getSize(void * ptr) {
      return _bigObjects.getSize(ptr);
   } 

  inline int getSizeClass(void * ptr) {
    size_t size = getSize(ptr); 
   // fprintf(stderr, "getSizeClass ptr %p size %lx\n", ptr, size); 
    return getSizeClass(size);
  }


  inline int getSizeClass(size_t size) {
    return _scMagicValue - __builtin_clz(size - 1);
  }

  // allocate a big object with the specified size
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB);

    // Allocate from the free list at first
    ptr = _bigObjects.allocate(size);
    if(ptr == NULL) {
      // Now allocate from the bump pointer
      allocateFromBigBumppointer(size);
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
     MM::bindMemoryBlockwise((char *)ptr, pages, _nodeindex, isHugePage);

     // Mark the object size
     markPerMBInfo(ptr, size, size);
 
     return ptr;
   }

   void markPerMBInfo(void * blockStart, size_t blockSize, size_t size) {
     _bigObjects.markPerMBInfo(blockStart, blockSize, size);
   }

   void * allocate(size_t size) {
     if(isBigObject(size)) {
       //fprintf(stderr, "Allocate big object with size %lx\n", size);
       return allocateBigObject(size);
     }
     else {
       return allocateSmallObject(size);
     }
   }

   inline bool isBigObject(size_t size) {
      return (size >= BIG_OBJECT_SIZE_THRESHOLD) ? true : false;
   }

   void * allocateSmallObject(size_t size) {
    int sc; 

    // If it is small object, allocate from the freelist at first.
    sc = getSizeClass(size);
    if(!_sizes[sc].hasObject()) {
      void * ptr = _bpSmall;
      _bpSmall += SIZE_ONE_MB;
      _sizes[sc].updateBumpPointer((char *)ptr, SIZE_ONE_MB);

      //fprintf(stderr, "allocate small object\n");
      // Update the PerMBInfo as well
      markPerMBInfo(ptr, SIZE_ONE_MB, _sizes[sc].getClassSize()); 
    }

    return _sizes[sc].allocate();
  }

  char * allocateFromSmallBumppointer(size_t size, int sc) {
     char * ptr = NULL;

     ptr = (char *)_bpSmall;
     _bpSmall += size;

     // We should not consume all memory. If yes, then we should make the heap bigger.
     // Since we don't check normally  to reduce the overhead, we will use the assertion here
     assert(_bpSmall < _bpSmallEnd);

     return ptr;
  }

   void deallocate(void * ptr) {
     size_t size = getSize(ptr);
     if(!isBigObject(size)) {
       int sc = getSizeClass(size); 
      
      if(!_mainHeapPhase) { 
        pthread_spin_lock(&_lock);
        // Return to the size class
        _sizes[sc].deallocate(ptr);
        pthread_spin_unlock(&_lock);
      }
      else {
        _sizes[sc].deallocate(ptr);
      }
     }
     else {
      if(!_mainHeapPhase) { 
        pthread_spin_lock(&_lock);
        _bigObjects.deallocate(ptr, size);
        pthread_spin_unlock(&_lock);
      }
      else {
        _bigObjects.deallocate(ptr, size);
      }
     }
   }


 
};
#endif
