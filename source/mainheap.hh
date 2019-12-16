#ifndef __MAIN_HEAP_HH__
#define __MAIN_HEAP_HH__

#include<stdio.h>
#include<stdlib.h>
#include "xdefines.hh"
#include "mm.hh"
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
    void * addr; 
    size_t size;
  } BigObject;

#define MT_MIDDLE_SPAN_THRESHOLD (PAGE_SIZE * NUMA_NODES)
#define MT_BIG_SPAN_THESHOLD BIG_OBJECT_SIZE_THRESHOLD
#define MT_BIG_OBJECTS_NUM 2048

// We will use twice as one bag, which is the unit for the shadow memory
#define BAG_SIZE_SMALL_OBJECTS (2 * MT_MIDDLE_SPAN_THRESHOLD)
#define TIMES 128

  class mtBigObjects {
    private:
      char * _bpBig;
      char * _bpBigEnd;
      unsigned int _next; 
      BigObject _objects[MT_BIG_OBJECTS_NUM];

    public:
    void initialize(void * start, size_t size) {
      _next = 0;
      _bpBig = (char *)start;
      _bpBigEnd = _bpBig + size;
    }

    // We will always utilize the bump pointer to allocate a new object
    void * allocate(size_t sz, int nodeindex) {
      void * ptr = NULL;
      bool isHugePage = false; 

      size_t size = alignup(sz, PAGE_SIZE);
      ptr = _bpBig;
      size_t pages = size/PAGE_SIZE;
      if(size > SIZE_HUGE_PAGE * (NUMA_NODES-1)) {
        // Get the number of huge pages at first
        pages = size/SIZE_HUGE_PAGE; 
        if(size/SIZE_HUGE_PAGE != 0) {
          pages += 1;
        }

        // Compute the blockSize. We will try to be balanced.
        // Overall, we don't want one node has 1 more blocks than the others. 
        size = pages * SIZE_HUGE_PAGE;
        isHugePage = true; 
     }
     _bpBig += size;

      // Allocate the memory from the OS 
      ptr = MM::mmapAllocatePrivate(size, ptr, isHugePage);
      _objects[_next].addr = ptr;
      _objects[_next].size = size;
      _next++;
     
      // Now binding the memory to different nodes. 
      MM::bindMemoryBlockwise((char *)ptr, pages, nodeindex, isHugePage); 
       
      // Adding the object to the list
      return ptr;
    }

    void deallocate(void * ptr) {
      int i;

      for(i = 0; i < _next; i++) {
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

  };

  class PerBagInfo {
    public: 
      unsigned int size;
  };

  private:
    char * _begin;
    char * _end;
   
    char * _bpSmall;
    char * _bpSmallEnd;
    char * _bpMiddle;
    char * _bpMiddleEnd;

    size_t _scMagicValue; 

    int   _numClasses;
    int   _nodeindex;
    int   _bagShiftBits;

    class mtBigObjects  _bigObjects;

    // Currently, size class is from 2^4 to 2^19 (512 KB), with 16 sizes in total.
    PerNodeSizeClass  ** _sizes;

    // The size of bag will be BAG_SIZE_SMALL_OBJECTS 
    PerBagInfo * _info;  

 public:
   size_t computeMetadataSize(void) {
     size_t size = 0;

      // Compute the size for _classes
      size += (sizeof(PerNodeSizeClass) + sizeof(PerNodeSizeClass *)) * SMALL_SIZE_CLASSES;

      // Getting the size for each freeArray. 
      unsigned long classSize = 16; 
      for(int i = 0; i < SMALL_SIZE_CLASSES; i++) { 
        unsigned long numObjects = (SIZE_ONE_MB_BAG * TIMES)/classSize; 
        if(numObjects < 2048) {
          numObjects = 2048;
        }

        size += numObjects * sizeof(void *);
        classSize *= 2;
      }

      // Now reserve the space for the information. 
      size += ((SIZE_PER_SPAN/BAG_SIZE_SMALL_OBJECTS)*sizeof(PerBagInfo))*2;
      return size;
   }

   void initialize(int nodeindex, void * begin) {
      // Save the heap related information
      _begin = (char *)begin;
      _end = _begin + (3 * SIZE_PER_SPAN);

      // Map an interleaved block for the small pages.
      _bpSmall = (char *)MM::mmapPageInterleaved(SIZE_PER_SPAN, (void *)_begin);
      _bpSmallEnd = _bpSmall + SIZE_PER_SPAN;

//      fprintf(stderr, "_bpSmall is %p\n", _bpSmall);

      // MMap the next span at first, and then use mbind to change the binding only
      // Note that we may only need to change it to huge page support (VERY RARE), if the allocation is 
      // larger than HUGE_PAGE_SIZE * NUMA_NODES
      _bpMiddle = (char *)MM::mmapAllocatePrivate(SIZE_PER_SPAN,_bpSmallEnd);
      _bpMiddleEnd = _bpMiddle + SIZE_PER_SPAN;

      // Note that the _bpBig is not initialized at first.
      _bigObjects.initialize(_bpMiddleEnd, SIZE_PER_SPAN);

      _nodeindex = nodeindex; 
      _numClasses = SMALL_SIZE_CLASSES;
      _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE);
     

      //_bagShiftBits = 13 + NUMA_NODES; 

      switch(NUMA_NODES) {
        case 16: 
          _bagShiftBits = 17;
          break;

        case 8:
          _bagShiftBits = 16;
          break;

        case 4: 
          _bagShiftBits = 15;
          break;

        case 2: 
          _bagShiftBits = 14;
          break;

        default:
          fprintf(stderr, "Please provide more support for specified %d nodes\n", NUMA_NODES);
       };
      // Compute the metadata size for the management.
      size_t metasize = computeMetadataSize();

      // Binding the memory to the specified node.
      char * ptr = (char *)MM::mmapFromNode(alignup(metasize, PAGE_SIZE), nodeindex);
      
      fprintf(stderr, "Initialize pernodeheap's metadata from %p to %p\n", ptr, ptr+metasize);

      // Initilize the size classes; 
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
        unsigned long numObjects = (SIZE_ONE_MB_BAG * TIMES)/classSize; 
        if(numObjects < 2048) {
          numObjects = 2048;
        }
        size = numObjects * sizeof(void *);
        // Each element of 
        _sizes[i]->initialize(classSize, numObjects, (void **)ptr);
        ptr += size;

        classSize *= 2;
      }

      // Now initialize the PerBagInfo 
      _info = (PerBagInfo *)ptr;

      fprintf(stderr, "PerBagInfo for the main heap starts at %p. Watch address %p\n", _info, &_info[74]);
   } 
  
  // We will reserve a block of memory to store the size information of each chunk,
  // in the unit of four pages. 
  int getSizeClass(void * ptr) {
    size_t size = getSize(ptr); 
   // fprintf(stderr, "getSizeClass ptr %p size %lx\n", ptr, size); 
    return getSizeClass(size);
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

  inline int getBagIndex(void * ptr) {
    return ((uintptr_t)ptr - (uintptr_t)_begin) >> _bagShiftBits;
  }

  // The size is the actual size for each object. 
  void markPerBagInfo(void * start, size_t blockSize, size_t realsize) {
    int count = blockSize >> _bagShiftBits; 

    unsigned long index = getBagIndex(start);

    PerBagInfo * info = &_info[index];
    // fprintf(stderr, "start %p, index %ld, count %d, info %p (offset %lx), realsize %lx, blocksize %lx\n", start, index, count, info, (&_info[index] - &_info[0]), realsize, blockSize);

    // fprintf(stderr, "start %p, index %ld, count %d, info %p, realsize %ld, blocksize %lx\n", start, index, count, info, realsize, blockSize);
    for(int i = 0; i < count; i++) {
      info->size = realsize;
      info++;
    }
  }

  size_t getSize(void * ptr) {
    // Check the shadow information in order to find the size. 
    unsigned long index = getBagIndex(ptr); 
    return _info[index].size; 
  }

  inline bool isBigObject(size_t size) {
    return (size >= MT_BIG_SPAN_THESHOLD) ? true : false;
  }

  void * allocate(size_t size) {
    void * ptr = NULL; 

    // Allocate from the freelist at first. 
    int sc;

    if(isBigObject(size)) {
      ptr = _bigObjects.allocate(size, _nodeindex);
      return ptr; 
    }
     
    // If it is small object, allocate from the freelist at first. 
    sc = getSizeClass(size);
    ptr = _sizes[sc]->allocate();
      
    // Allocate from the bump pointer right now
    if(ptr == NULL) {
      int classsize = _sizes[sc]->getSize();

      // Now we are using different size requirement for this.
      if(classsize <= MT_MIDDLE_SPAN_THRESHOLD) {
        // Allocate one bag from the first span
        ptr = _bpSmall;
        _bpSmall += BAG_SIZE_SMALL_OBJECTS;
    
        // Adding remainning objects to the free list, except the first one. 
        _sizes[sc]->insertObjectsToFreeList((char *)ptr +classsize, _bpSmall);
      
        // Mark PerBagInfo
        markPerBagInfo(ptr, BAG_SIZE_SMALL_OBJECTS, classsize);
      }
      else if (size <= MT_BIG_SPAN_THESHOLD) {
        ptr = _bpMiddle;
        _bpMiddle += classsize;
        markPerBagInfo(ptr, classsize, classsize);
      }
    }
    
    return ptr;
  }

  void deallocate(void * ptr) {
    if(ptr <= _bpMiddleEnd) {
      int sc = getSizeClass(ptr);
      //fprintf(stderr, "ptr %p with size %lx\n", ptr, getSize(ptr));
      if(sc > 16) {
        unsigned long index = getBagIndex(ptr);
        PerBagInfo * info = &_info[index];
        fprintf(stderr, "ptr %p with index %ld, info %p size %x\n", ptr, index, info, info->size);

        exit(-1);
      }
      _sizes[sc]->deallocate(ptr);
    }
    else { 
      _bigObjects.deallocate(ptr);
    }
  }
 
};
#endif
