
#include "perthreadsizeclass.hh"
#include "numaheap.hh"

// Allocate reversely. Since _next always points to
// the next availabe slot, we will use the object pointed by _next-1
void * PerThreadSizeClass::allocateOneIfAvailable() {
    _next--;
    void * ptr = _freeArray[_next];

    // Make _next point to the just allocated slot
    _avails--;
    return ptr;
}


void * PerThreadSizeClass::allocate() {
    void * ptr = NULL; 

    if(_avails > 0) {
      ptr = allocateOneIfAvailable(); 
      return ptr;
    }
      
    if(_allocs >= _allocsBeforeCheck) {
      _allocs = 0;
      // If no available objects, allocate objects from the node's freelist 
      _avails = NumaHeap::getInstance().allocateBatchFromNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[_next]);

      //We should get the allocation from the bumppointers at first, and then get batch from NodeFreelist
      if(_avails > 0) {
        _next += _avails;
        ptr = allocateOneIfAvailable(); 
        _allocsBeforeCheck /= 2;
      }
      else {
      // fprintf(stderr, "Getting from pernode freelist failed. _allocsBeforeCheck %ld\n", _allocsBeforeCheck);
        // Can't find available objects in PerNodeFreeList, then we will 
        // check less frequently. 
        _allocsBeforeCheck *= 2;
        if(_allocsBeforeCheck > _max/2) {
          _allocsBeforeCheck = _max/2;
        }
      }
    }

    if(ptr == NULL) {
      _allocs++;
      // Now allocate from the never used ones
      // We don't need to change _avails and _next any more
      if(_bumpPointer < _bumpPointerEnd) {
        ptr = _bumpPointer;
        _bumpPointer += _size; 
      }
      else {
        // Now get a chunk from the current PerNodeHeap: either from big objects or never allocated ones
        _bumpPointer = NumaHeap::getInstance().allocateOnembFromNode(getNodeIndex(), _size); 
        _bumpPointerEnd = _bumpPointer + SIZE_ONE_MB_BAG;

        // Now perform the allocation
        ptr = _bumpPointer;
        _bumpPointer += _size; 
      }
    }
    return ptr;
  }
 
  // Return an object starting with ptr to the PerThreadSizeClass
  void PerThreadSizeClass::deallocate(void *ptr) {
    if(_discard == true) {
      if(_next < _max) {
        _discard = false;
      }
      else {
        return;
      }
    }

    _freeArray[_next] = ptr;

    _next++;
    _avails++;

    // If there is no spot to hold next deallocation, put some objects to PerNodeSizeClass 
    if(_next >= _max) {
      int items;
      // We would like to maintain the same order. 
      int first = _next - _batch;   
      items = NumaHeap::getInstance().deallocateBatchToNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[first]);
      _avails -= items;
      _next -= items;
      if(items == 0) {
        fprintf(stderr, "Increase the size for PerNodeSizeClass or PerThreadSizeCalss for _sc %ld\n", _sc);
        _discard = true;
        //assert(size != 0);
      }
    }

    return; 
  }
