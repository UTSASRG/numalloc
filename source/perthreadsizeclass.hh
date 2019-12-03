#ifndef __PER_THREAD_SIZE_CLASS_HH__
#define __PER_THREAD_SIZE_CLASS_HH__

#include "mm.hh"

class PerThreadSizeClass {
private:
  char        * _bumpPointer; 
  char        * _bumpPointerEnd;
  size_t        _batch; // Allocate or contribute this number if necessary 
  unsigned long _sc; 
  unsigned long _size;  // How big for each object
  unsigned long _max;   // How many entries in the array 
  unsigned long _next;    // Point to the next available slot 
  unsigned long _avails; // Available objects in the array. 
  
  bool _discard;
  // Freelist will be tracked with one circular array.
  void ** _freeArray;

public: 
  void initialize(int nodeindex, int size, int sc, unsigned long numObjects, void * pointer) {
    _next = 0;
    _avails = 0;
    _max = numObjects;
    _size = size;
    _sc = sc;
    _discard = false;

    // Allocate a chunk of memory for the array.
    size_t sz = numObjects * sizeof(void *);

    // Map a chunk of memory in the local node
    _freeArray = (void **)pointer;

    _batch = 0.5 * numObjects;

    _bumpPointer = NULL;
    _bumpPointerEnd = _bumpPointer; 
  }
 
  // Allocate reversely. Since _next always points to 
  // the next availabe slot, we will use the object pointed by _next-1 
  void * allocateOneIfAvailable();

  void * allocate();

  void deallocate(void *ptr);
};
#endif
