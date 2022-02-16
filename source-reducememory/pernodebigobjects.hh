#ifndef __PER_NODE_BIG_OBJECTS_HH__
#define __PER_NODE_BIG_OBJECTS_HH__

#include <pthread.h>
#include "xdefines.hh"
#include "perthread.hh"

class PerNodeBigObjects {

  // Each big object will have address and size
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
  PerBigObject * _objects;
  void * _heapBegin;

  unsigned long _next;
  unsigned long _max;

  // Place the PerMBInfo in this object, since everytime when the information will be changed
  // by each allocation and deallocation. If the information is placed in pernodeheap, although
  // it sounds better, but it is very hard for the class to access it (or invoking unnecessary overhead) 
  // Maintaining a per-mb array for each PerNodeHeap.
  PerMBInfo *   _info;

public:
  void initialize(char * ptr, void * heapBegin) {
    _next = 0; 
    _max = PER_NODE_MAX_BIG_OBJECTS - 1;
    
    _heapBegin = heapBegin;

    // Initialize the _objects array
    _objects = (PerBigObject *) ptr;
    memset(ptr, 0, PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject));

    // Initialize PerMBInfo
    ptr += PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject);

    // confirm why _info has been corrupted. 
    _info = (PerMBInfo *)ptr;
  }

  size_t computeImplicitSize(size_t heapsize) {
    return PER_NODE_MAX_BIG_OBJECTS * sizeof(PerBigObject) + (heapsize>>SIZE_ONE_BAG_SHIFT)*sizeof(PerMBInfo);
  }

  // Mark the object size in PerMBInfo array for small objects; store the size info in the PerBigObject array for big objects
  void markPerMBInfo(void * ptr, size_t size, size_t objsize) {
    // Big objects
    if (objsize >= BIG_OBJECT_SIZE_THRESHOLD && size == objsize) {
      PerBigObject * object = &_objects[_next];
      
      object->size = size;
      object->address = ptr;

      _next++;
    
      assert(_next != _max);
      return;      
    }


    // Small objects
    unsigned long mbIndex = ((uintptr_t)ptr - (uintptr_t)_heapBegin) >> SIZE_ONE_BAG_SHIFT;
    unsigned long mbs = size >> SIZE_ONE_BAG_SHIFT;
 
   // fprintf(stderr, "ptr %p size %lx objsize %lx\n", ptr, size, objsize);  
   if((size & SIZE_ONE_BAG_MASK) != 0) {
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

  inline size_t getSize(unsigned long mbIndex) {
    return _info[mbIndex].size;
  }

  // Only for small object (checked before invoked)
  size_t getSize(void * ptr) {
    unsigned long mbIndex = ((uintptr_t)ptr - (uintptr_t)_heapBegin) >> SIZE_ONE_BAG_SHIFT;
 // fprintf(stderr, "getSize with ptr %p, mbIndex %lx, _heapBegin %p, getsize %lx\n", ptr, mbIndex, _heapBegin, getSize(mbIndex));
    return getSize(mbIndex);
  }

  // Deallocate big object and return the memory back to OS
  void deallocate(void * ptr) {
    PerBigObject * object;
    size_t size = -1;
    for(int i = _next - 1; i >= 0; i--) {
      object = &_objects[i];
      // fprintf(stderr, "addr=%p, size=%lx\n", object->address, object->size);
      if(object->address == ptr) {
        size = object->size;
        break;
      }
    }
    assert(size != -1);
    if (_next > 1) {
      object->address = _objects[_next-1].address;
      object->size = _objects[_next-1].size;
    }
    _next--;
    MM::returnMemoryBackToOS(ptr, size);
  }
};
#endif
