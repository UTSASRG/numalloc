
#include "perthreadsizeclass.hh"
#include "numaheap.hh"

// Allocate reversely. Since _next always points to
// the next availabe slot, we will use the object pointed by _next-1
void * PerThreadSizeClass::allocateOneIfAvailable() {
    void * ptr = _freeArray[_next-1];

    // Make _next point to the just allocated slot
    _next--;
    _avails--;
#if 0
    if(_size == 0x10000 && (getThreadIndex() == 17)) {
      fprintf(stderr, "allocateObject ptr %p from _size %lx.  _next %d freeArray(next)[%d]: %p (at %p). freeArray(next-1)[%d]: %p\n", ptr, _size, _next, _next, _freeArray[_next], &_freeArray[_next], _next-1, _freeArray[_next-1]);
      fprintf(stderr, "allocateObject ptr %p from _size %lx, _next %d. freeArray[%d]: %p\n", ptr, _size, _next, _next-1, _freeArray[_next-1]);
    }
#endif
    return ptr;
}


void * PerThreadSizeClass::allocate() {
    void * ptr = NULL; 

    if(_avails > 0) {
      ptr = allocateOneIfAvailable(); 
      if(ptr == NULL) { 
        fprintf(stderr, "Thread%d: ptr %p when _avails is larger than 0 (%ld). FreeArray %p\n", getThreadIndex(), ptr, _avails, _freeArray[_avails-1]); 
        abort();
      } 
      return ptr;
    }
    else {
      
      // If no available objects, allocate objects from the node's freelist 
      _avails = NumaHeap::getInstance().allocateBatchFromNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[_next]);

      if(_avails > 0) {
        _next += _avails;
        ptr = allocateOneIfAvailable(); 
      }
    }

    if(ptr == NULL) {
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
    _freeArray[_next] = ptr;

    _next++;
    _avails++;

#if 0
    if(_size == 0x10000 && (getThreadIndex() == 17) ) {
      //&& (_next==7 ||  _next == 6)) {
      fprintf(stderr, "deallocation: adding ptr %p to the freelist with _next %ld. _avails %ld, _max %ld\n", ptr, _next-1, _avails, _max);
      fprintf(stderr, "freeArray[6]: %p (at %p), freeArray[7]: %p (at %p), freeArray[8]:%p\n", _freeArray[6], &_freeArray[6], _freeArray[7], &_freeArray[7], _freeArray[8]);
   }
#endif    
    // If there is no spot to hold next deallocation, contribute some objects to
    // PerNodeSizeClass 
    if(_next >= _max) {
      int size;
      // We would like to maintain the same order. 
      int first = _next - _batch;   
      size = NumaHeap::getInstance().deallocateBatchToNodeFreelist(getNodeIndex(), _sc, _batch, &_freeArray[first]);
      _avails -= size;
      _next -= size;
    }

    return; 
  }
