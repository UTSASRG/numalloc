
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

    //if(_sc == 2) {
    //  fprintf(stderr, "_avails at %p, _size %p, _pointer at %p, _pointerEnd at %p\n", &_avails, &_size, &_bumpPointer, &_bumpPointerEnd);
   // }
    if(_avails > 0) {
      ptr = allocateOneIfAvailable(); 
      return ptr;
    }
 #if 1
    // TODO: switch with the bumppointer
    else {
      
      // If no available objects, allocate objects from the node's freelist 
      _avails = NumaHeap::getInstance().allocateBatchFromNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[_next]);

      //TODO
      //We should get the allocation from the bumppointers at first, and then get batch from NodeFreelist
      //if(getThreadIndex() == 0)
      //fprintf(stderr, "thread %d get batches from node %d\n", _avails);

      if(_avails > 0) {
        _next += _avails;
        ptr = allocateOneIfAvailable(); 
      }
    }
#endif

    if(ptr == NULL) {
      // Now allocate from the never used ones
      // We don't need to change _avails and _next any more
      if(_bumpPointer < _bumpPointerEnd) {
//        fprintf(stderr, "ALLOCATE from bump pointer\n");
        ptr = _bumpPointer;
        _bumpPointer += _size; 
      }
      else {
        // Now get a chunk from the current PerNodeHeap: either from big objects or never allocated ones
        _bumpPointer = NumaHeap::getInstance().allocateOnembFromNode(getNodeIndex(), _size); 
        _bumpPointerEnd = _bumpPointer + SIZE_ONE_MB_BAG;

      //  fprintf(stderr, "Getting one mb now\n");
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
      int size;
      // We would like to maintain the same order. 
      int first = _next - _batch;   
      size = NumaHeap::getInstance().deallocateBatchToNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[first]);
      _avails -= size;
      _next -= size;
      if(size == 0) {
        fprintf(stderr, "Increase the size for PerNodeSizeClass or PerThreadSizeCalss for _sc %ld\n", _sc);
        _discard = true;
        //assert(size != 0);
      }
    }

    return; 
  }
