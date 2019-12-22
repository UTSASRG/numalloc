#ifndef __PER_NODE_BIG_OBJECTS_HH__
#define __PER_NODE_BIG_OBJECTS_HH__

#include <pthread.h>
#include "xdefines.hh"
#include "perthread.hh"
#include "freememlist.hh"

// We should support the transfer between big objects and small objects 
// We will maintain a freelist for big objects, with array 
class PerNodeBigObjects {

  // Each available object will have address and size, and will be inserted into 
  // the freelist that is ordered by the deallocation order.
  class PerBigObject {
  public:
    void *  address;
    size_t  size;
  };

  class PerMBInfo {
  public:
    unsigned int size;
    union {
      unsigned int usedOnes; // Count how many are used for small objects.
      bool usedStatus;           // Point to the starting address of this object.
    } u;
  };

private:
  // A circular array with the maximum of 1024 objects;
  // When an object is used, it will be removed from the list. 
  // Also, the corresponding BigObjectInfo will be erased.
  FreeMemList freeObjectList;
  PerBigObject * _objects;
  void * _heapBegin;

  // TODO: put this in the upper level
  //[PER_NODE_MAX_BIG_OBJECTS];  
  
  unsigned long _next;
  unsigned long _max;

  // The size of total freed objects of this node
  unsigned long _totalSize; 

  // Place the PerMBInfo in this object, since everytime when the information will be changed
  // by each allocation and deallocation. If the information is placed in pernodeheap, although
  // it sounds better, but it is very hard for the class to access it (or invoking unnecessary overhead) 
  // Maintaining a per-mb array for each PerNodeHeap.
  PerMBInfo *   _info;
  unsigned long _mbs;

public:
  void initialize(char * ptr, void * heapBegin, size_t heapsize) {
    _totalSize = 0;
    _next = 0; 
    _max = PER_NODE_MAX_BIG_OBJECTS - 1;
    
    _heapBegin = heapBegin;


    // Initialize the _objects array
    _objects = (PerBigObject *) ptr;
    memset(ptr, 0, PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject));

    _mbs = heapsize >> SIZE_ONE_MB_SHIFT;
    // Initialize PerMBInfo
    ptr += PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject);

    // confirm why _info has been corrupted. 
    _info = (PerMBInfo *)ptr;
    //memset(ptr, 0, (heapsize>>SIZE_ONE_MB_SHIFT)*sizeof(PerMBInfo));
  
  }

  size_t computeImplicitSize(size_t heapsize) {
    return PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject) + (heapsize>>SIZE_ONE_MB_SHIFT)*sizeof(PerMBInfo);
  }

  void * allocate(size_t size) {
    void * ptr = NULL;

    if(_totalSize < size) {
      return NULL;
    }

    //fprintf(stderr, "Allocate big object size %lx, _totalSize %lx\n", size, _totalSize);

    for(int i = _next - 1; i>= 0; i--) {
      PerBigObject * object = &_objects[i];   
    // Now search the objects to find an entry that satisfies the request
  //  fprintf(stderr, "AllocateBigObject totalSize %lx request size %lx\n",  _totalSize, size);
      // Check whether the current entry can be merged with its neibour to satisfy this request
      if(object->size < size) {
#if 0
        // Merge with its neighbor if possible.
        unsigned long mbIndex = getMBIndex((void *)object->address);
        unsigned long mbs = object->size >> SIZE_ONE_MB_SHIFT;

        PerMBInfo * info = &_info[mbIndex];
        PerMBInfo * prev = info--;
        PerMBInfo * next = &_info[mbIndex+mbs];
        bool mergedWithLeft = false; 
        // Check whether its left neighbour is available.
        if(prev->u.usedStatus == false) {
          mergeBigObjects(prev, info);
          mergedWithLeft = true;
        }
       
        if(next->u.usedStatus == false) {
          // The next object possibly includes the never-allocated ones. But their status will be false as always
          mergeBigObjects(prev);
        }
#endif
        // We should at least merge the last one with never-allocated ones.
      }

      // After merge, confirm whether this entry is big enough to satisfy the current request
      if(object->size >= size) {
        ptr = object->address;
        object->size -= size; 
        _totalSize -= size;

        // Unusual case: the object size is the requested size, then we should change the freelist
        if (object->size == 0) {
            
          object->size = _objects[_next - 1].size;
          object->address = _objects[_next -1].address;

          // Freed an entry
          _next--;
        }
        else {
          // Split this object to two parts, and always use the first part to satisfy the request.
          // For the remaining part, we will update its pointer and size information (already did above).
          // There is no need to update the freelist, since another half is still in the list
          object->address = (void *)((intptr_t)object->address + size);
       
         // fprintf(stderr, "********** dividing original size %lx this size %lx\n", object->size + size, size); 
          // Change the PerMBInfo information for the remaining part.
          // That is, we should change the size to the new size
          size_t newSize = object->size;

          // Change the allocated one
          changePerMBInfoSize(ptr, size, size);

          // Change the remainning one
          changePerMBInfoSize(object->address, newSize, newSize);
        }
        break;
      }
    } 

  //if(ptr)
  //  fprintf(stderr, "GET object ptr %p size %lx _next %ld\n", ptr, size, _next);

    return ptr; 
  }
  
  // Mark the object size in PerMBInfo array
  void changePerMBInfoSize(void * ptr, size_t size, size_t objSize) {
    unsigned long mbIndex = getMBIndex(ptr);
    unsigned long mbs = size >> SIZE_ONE_MB_SHIFT;
  
    assert((size & SIZE_ONE_MB_MASK) == 0);

//   fprintf(stderr, "CHANGE ptr %p size %lx: mbindex %ld\n", ptr, size, mbIndex);
    for(int i = 0; i < mbs; i++) {
      _info[mbIndex+i].size = size;
    }
  }

  // Mark the object size in PerMBInfo array
  void markPerMBInfo(void * ptr, size_t size, size_t objsize) {
    unsigned long mbIndex = getMBIndex(ptr);
    unsigned long mbs = size >> SIZE_ONE_MB_SHIFT;
  
  //  assert(size & SIZE_ONE_MB_MASK == 0);
   if((size & SIZE_ONE_MB_MASK) != 0) {
      fprintf(stderr, "markPerMBInfo markPerMBInfo size is not aligned. size %lx\n", size);
      abort();
    }
 
   // fprintf(stderr, "MARK ptr %p size %lx: mbindex %ld size %lx\n", ptr, size, mbIndex, objsize);
    for(int i = 0; i < mbs; i++) {
      _info[mbIndex+i].size = objsize;
      // Now this object has been used right now
      _info[mbIndex+i].u.usedStatus = true;
    }
  }

  void clearPerMBInfo(void * ptr, size_t size) {
    unsigned long mbIndex = getMBIndex(ptr);
    unsigned long mbs = size >> SIZE_ONE_MB_SHIFT;
  
   // assert(size & SIZE_ONE_MB_MASK == 0);
    if((size & SIZE_ONE_MB_MASK) != 0) {
      fprintf(stderr, "clearPerMBInfo markPerMBInfo size is not aligned. size %lx\n", size);
      abort();
    }
    
    // We don't clear the starting address for the merge operation
    //fprintf(stderr, "clear ptr %p size %lx: mbindex %ld\n", ptr, size, mbIndex);
    for(int i = 0; i < mbs; i++) {
      // Mark all corresponding mbs as free right now
      _info[mbIndex+i].u.usedStatus = false;
    }
  }

  size_t getSize(unsigned long mbIndex) {
    return _info[mbIndex].size;
  } 

  void printSize(void * ptr) {
    unsigned long mbIndex = getMBIndex(ptr); 
    //fprintf(stderr, "printSize with ptr %p, mbIndex %lx, _heapBegin %p, getsize %lx\n", ptr, mbIndex, _heapBegin, getSize(mbIndex));

  }

  unsigned long getMBIndex(void * ptr) {
    return ((uintptr_t)ptr - (uintptr_t)_heapBegin) >> SIZE_ONE_MB_SHIFT;
  }

  size_t getSize(void * ptr) {
    //unsigned long mbIndex = getMBIndex(ptr); 
    unsigned long mbIndex = ((uintptr_t)ptr - (uintptr_t)_heapBegin) >> SIZE_ONE_MB_SHIFT;
  //  fprintf(stderr, "getSize with ptr %p, mbIndex %lx, _heapBegin %p, getsize %lx\n", ptr, mbIndex, _heapBegin, getSize(mbIndex));
    return getSize(mbIndex);
 }

  // Place the objects to the list
  void deallocate(void * ptr, size_t size) {

    PerBigObject * object = &_objects[_next];
    
    object->size = size; 
    object->address = ptr;

    _next++;
    
    _totalSize += size;
  
    assert(_next != _max);
    freeObjectList.insertIntoHead(ptr, size);
  
    // Change PerMBInfo in order to encourage the coalesce TODO:  
    clearPerMBInfo(ptr, size);

  }
};
#endif
