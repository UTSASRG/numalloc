#ifndef __MAIN_HEAP_HH__
#define __MAIN_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include "xdefines.hh"
#include "mm.hh"
#include "pernodebigobjects.hh"
#include "pernodesizeclass.hh"
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

  typedef struct t_bigObject {
    char * addr; 
    size_t size;
    // For some cases, we will have to mmap twice with one allocation
    char * addr2; 
    size_t size;
  } BigObject;

#define MT_MIDDLE_SPAN_THRESHOLD (PAGE_SIZE * (NUMA_NODES-1))
#define MT_BIG_SPAN_THESHOLD BIG_OBJECT_SIZE_THRESHOLD
#define MT_BIG_OBJECTS_NUM 2048

  class mtBigObjects {
    private:
      char * _bpBig;
      char * _bpBigEnd;
      unsigned int _next; 
      BigObject _objects[MT_BIG_OBJECTS_NUM];

    void initialize(void * start, size_t size) {
      _next = 0;
      _bpBig = (char *)start;
      _bpBigEnd = _bpBig + size;
    }

    // We will always utilize the bump pointer to allocate a new object
    void * allocate(size_t size) {
      void * ptr;

      size_t size = (); 
      if(size <= SIZE_HUGE_PAGE * (NUMA_NODES-1)) {
        // Then use the normal pages, but with the block-interleaved way for the allocation
        ptr = _bpBig;
        
        ptr = MM:mmapPageInterleaved(

        _bpBig += size;
      }
      else {

      } 
      return ptr;
    }

    void deallocate(void * ptr) {
      for(int i = 0; i < _next; i++) {
        // Now we have found the object
        if(ptr == _objects[i].addr) {
          // Simply return this object to the OS right now
          munmap(ptr, _objects[i].size);

          // Moving the last one to the current one.
          if(i != _next-1) { 
            _objects[i].addr = _objects[_next-1].addr;
            _objects[i].size = _objects[_next-1].size;
          } 
          _next--;
          break;  
        }
      }
    }
  }

  private:
    char * _begin;
    char * _end;
   
    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpMiddle;
    char * _bpMiddleEnd;

    size_t _scMagicValue; 

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerNodeSizeClass  ** _sizes;
    int   _numClasses;
    int   _nodeindex;
    char  padding[64]; // Padding with 64bytes to ensure that two node data will no locate in the same cashe line


 public:
   size_t computeMetadataSize(void) {
     size_t size = 0;

      // Compute the size for _bigObjects. 
      size += sizeof(PerNodeBigObjects) + _bigObjects->computeImplicitSize(heapsize);

      // Compute the size for _classes
      size += (sizeof(PerNodeSizeClass) + sizeof(PerNodeSizeClass *)) * SMALL_SIZE_CLASSES;

      // Getting the size for each freeArray. 
      unsigned long classSize = 16; 
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) { 
        unsigned long numObjects = (SIZE_ONE_MB_BAG * TIMES_MB)/classSize; 
        if(numObjects < 2048) {
          numObjects = 2048;
        }

        size += numObjects * sizeof(void *);
        classSize *= 2;
      }

      return size;
   }

   void initialize(int nodeindex, char * end) {
      // Save the heap related information
      _end = end;
      _begin = _end - 3 * SIZE_PER_SPAN;

      // Map an interleaved block for the small pages.
      _bpSmall = (char *)MM::mmapPageInterleaved(SIZE_PER_SPAN, (void *)_begin);
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;


      // MMap the next span at first, and then use mbind to change the binding only
      // Note that we may only need to change it to huge page support (VERY RARE), if the allocation is 
      // larger than HUGE_PAGE_SIZE * NUMA_NODES
      _bpMiddle = (char *)MM::mmapAllocatePrivate(SIZE_PER_SPAN,_bpSmallEnd);
      _bpMiddleEnd = _bpMiddle + SIZE_PER_SPAN;

      // Note that the _bpBig is not initialized at first.
      _bpBig = _bpMiddleEnd;
      _bpBigEnd = _bpBig + SIZE_PER_SPAN; 

      _nodeindex = nodeindex; 
      _numClasses = SMALL_SIZE_CLASSES;

      // Compute the metadata size for the management.
      size_t metasize = computeMetadataSize(heapsize);
      _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE);

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
      
      fprintf(stderr, "Initialize pernodeheap's metadata from %p to %p\n", ptr, ptr+metasize);

      // Initialization right now.
      _lock = (pthread_spinlock_t *)ptr; 
      pthread_spin_init(_lock, PTHREAD_PROCESS_PRIVATE);
      
      ptr += sizeof(pthread_spinlock_t) +4; 

      // Initilize the PerNodeMBInfo
      // Initialize the _bigObjects
      _bigObjects = (PerNodeBigObjects *)ptr; 
      ptr += sizeof(PerNodeBigObjects); 
      _bigObjects->initialize(ptr, start, heapsize); 
      ptr += _bigObjects->computeImplicitSize(heapsize);

      // Initilize the size classes; 
      //size += sizeof(PerNodeSizeClass) * SMALL_SIZE_CLASSES;
      _sizes = (PerNodeSizeClass **)ptr;
      ptr += sizeof(PerNodeSizeClass *) * SMALL_SIZE_CLASSES;

      // Initialize the _sizes array, since every element points to an actual PerNodeSizeClass object
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
        _sizes[i] = (PerNodeSizeClass *)ptr;
        ptr += sizeof(PerNodeSizeClass);
      } 
      
      unsigned long classSize = 16; 
      unsigned long size; 
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
        unsigned long numObjects = (SIZE_ONE_MB_BAG * TIMES_MB)/classSize; 
        if(numObjects < 2048) {
          numObjects = 2048;
        }
        size = numObjects * sizeof(void *);
        // Each element of 
        _sizes[i]->initialize(classSize, numObjects, (void **)ptr);
        ptr += size;

        classSize *= 2;
      }
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

  // The size is the actual size for each object. 
  void markPerMBInfo(void * blockStart, size_t blockSize, size_t size) {
  }

  size_t getSize(void * ptr) {
    // Check the shadow information in order to find the size. 
    // TODO:
    //
  }

  bool isSmallObject(size_t size) {
    return (size <= BIG_OBJECT_SIZE_THRESHOLD) ? true : false;
  }

  void * allocateFromSecondSpan(size_t size) {


  }

  void * allocate(size_t size) {
    void * ptr = NULL; 

    // Allocate from the freelist at first. 
    int sc;
    if(isSmallObject(size)) {
      sc = getSizeClass(size);
      ptr = _sizes[sc]->allocate();
    }
    else {
      ptr = _bigObjects->allocate(size);
    }
      
    // Allocate from the bump pointer right now
    if(ptr == NULL) {
      // Now we are using different size requirement for this.
      if(size <= MT_MIDDLE_SPAN_THRESHOLD) {
        size_t sc = (); 

        // Allocate one bag from the first span
        // Then add all objects to the free list, except the first one. 
         
        ptr = (char *)_bpSmall;
        _bpSmall += size;
      }
      else if () {
        // Allocate from the second span
        ptr = allocateFromSecondSpan(size);
      }
      else {
        // Allocate from the big span
      }
    }
  }

  void deallocateBigObject(void *ptr) {
    // Traverse the list of big objects, and then invokde 
    // munmap to return this object back to the OS.
    
  }

  void deallocate(void * ptr) {
    if(ptr <= _bpMiddleEnd) {
      int sc = getSizeClass(ptr);
      _sizes[sc]->deallocate(ptr);
    }
    else { 
      deallocateBigObject(ptr);
    }
};
#endif
