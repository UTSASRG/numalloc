#ifndef __PER_NODE_HEAP_HH__
#define __PER_NODE_HEAP_HH__

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
   For those allocations, we are careful about the load balance issue.
   However, if an object is larger than PAGE*NUMA_NODES, then we will utilize block-wise 
   allocations, which has been observed by many existing work (such as Xu's PPoPP'14).
  
   Similar to other nodes' allocation, the allocation of main thread will be from two spans. 
   The first half of the perfthreadheap will be completely interleaved, while 
   the second half will be block-wise interleaved method.

   For allocations with the size smaller than 4K, the object will be returned to the node.

   For allocations with the size larger than 4K but smaller than 4K*NUM_NODES, then it will be putted to its owner.

   If the size is larger than 4K*NUM_NODES, then the object will be block-wise interleaved method, which 
   could be placed to the current thread or its original owner (it doesn't matter).

   If the size is larger than 2M*NUM_NODES, we will utilize the large page and the block-wise interleaved method
*/

class MainNodeHeap {
  private:
    char * _begin;
    char * _end;
   
    char * _bumpPointer;

    size_t _scMagicValue; 

    PerNodeBigObjects * _bigObjects;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerNodeSizeClass  ** _sizes;
    int   _numClasses;
    int   _nodeindex;
    char  padding[64]; // Padding with 64bytes to ensure that two node data will no locate in the same cashe line

 public:
   size_t computeMetadataSize(size_t heapsize) {
     size_t size = sizeof(pthread_spinlock_t) + 4; 

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

   void initialize(int nodeindex, char * start, size_t heapsize) {

      // Save the heap related information
      _begin = start; 
      _end = start + heapsize;
      _bumpPointer = start;
      _nodeindex = nodeindex; 
      _numClasses = SMALL_SIZE_CLASSES;

      // Compute the metadata size for PerNodeHeap
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

  // allocate a big object with the specified size
  void * allocateBigObject(size_t size) {
    void * ptr = NULL;
    size = alignup(size, SIZE_ONE_MB_BAG);

    //fprintf(stderr, "allocateBigObject at node %d: size %lx\n", _nodeindex, size);
    ptr = allocateFromFreelist(size);
    if(ptr == NULL) {
      ptr = allocateFromBumppointer(size);
    }

    // Mark the object size
    markPerMBInfo(ptr, size, size);

    return ptr;
  }

  int allocateBatch(int sc, unsigned long num, void ** start) {
    return _sizes[sc]->allocateBatch(num, start);   
  }

  int deallocateBatch(int sc, unsigned long num, void ** start) {
    return _sizes[sc]->deallocateBatch(num, start);   
  }

  // size indicates that this onemb will be used to satisfy object with size
  char * allocateFromFreelist(size_t size) {
    return (char *)_bigObjects->allocate(size); 
  }

  // The size is the actual size for each object. 
  void markPerMBInfo(void * blockStart, size_t blockSize, size_t size) {
    _bigObjects->markPerMBInfo(blockStart, blockSize, size);
  }

  char * allocateFromBumppointer(size_t size) {
    char * ptr = NULL; 

    lock(); 
    ptr = (char *)_bumpPointer;
    _bumpPointer += size; 

    // We should not consume all memory. If yes, then we should make the heap bigger.
    // Since we don't check normally  to reduce the overhead, we will use the assertion here
    assert(_bumpPointer < _end);
    unlock();

    return ptr;
  }

  // Allocate one MB from the current node, and will use it for small objects with size _size
  char * allocateOnemb(size_t size) {
    char * ptr = NULL; 

    // Check the freed bigObjects at first, since they may be still hot in cache. 
    ptr = allocateFromFreelist(SIZE_ONE_MB_BAG); 

    // If there is no freed bigObjects, getting one from the bump pointer
    if(ptr == NULL) {
      ptr = allocateFromBumppointer(SIZE_ONE_MB_BAG);
    } 

    // Mark the size information
    markPerMBInfo(ptr, SIZE_ONE_MB_BAG, size);

    return ptr; 
  }


  size_t getSize(void * ptr) {
    // Check the 1MB information in order to find the size. 
    return _bigObjects->getSize(ptr); 
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
       int sc = getSizeClass(size);

       if(index == nodeindex) {
         // Only return an object to perthread's heap if the object is from the same node
         current->ptheap->deallocate(ptr, sc);
       }
       else {
          // Return this object to the current node's freelist for different size classes
        // Based on NumaHeap, this address belongs to the current node.
        _sizes[sc]->deallocate(ptr);
       }
     }
     else {
  //     _bigObjects->printSize(ptr);   
  //     fprintf(stderr, "Thread %d: Deallocate ptr %p size %lx\n", getThreadIndex(), ptr, size); 
       // Deallocate this object to _bigObjects
       _bigObjects->deallocate(ptr, size);
     }
   }

};
#endif
