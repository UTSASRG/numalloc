
#include "mm.hh"
#include "perthreadsizeclass.hh"
#include "numaheap.hh"

// Allocate reversely. Since _next always points to
// the next availabe slot, we will use the object pointed by _next-1
void * PerThreadSizeClass::allocateOneIfAvailable() {
    FreeMemNode *head = freeMemList.getHead();
    if (head->getNext() == NULL) {
        return NULL;
    }
    head=head->getNext();
    freeMemList.remove(head);
    return (void *) head;

    // Make _next point to the just allocated slot
}

void * PerThreadSizeClass::allocate() {
    void * ptr = NULL;

    FreeMemNode *head = freeMemList.getHead();
    if (head->getNext() != NULL) {
        head = head->getNext();
        freeMemList.remove(head);
        return (void *) head;
    }

    if(_allocs >= _allocsBeforeCheck) {
      _allocs = 0;
      void *dist[_batch];
      // If no available objects, allocate objects from the node's freelist 
      _avails = NumaHeap::getInstance().allocateBatchFromNodeFreelist(getNodeIndex(), _sc, _batch, dist);

      //We should get the allocation from the bumppointers at first, and then get batch from NodeFreelist
      if(_avails > 0) {
          ptr = dist[0];
          for (int i = 1; i < _avails; i++) {
              freeMemList.insertIntoHead(dist[i], _size);
          }
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

      freeMemList.insertIntoHead(ptr, _size);

      // If there is no spot to hold next deallocation, put some objects to PerNodeSizeClass
      if (freeMemList.getLength() < 2 * _max) {
          return;
      }
      void *dist[_batch];
      for (int i = 0; i < _batch; i++) {
          dist[i] = (void *) freeMemList.getTail();
          freeMemList.remove(freeMemList.getTail());
      }
      int items = NumaHeap::getInstance().deallocateBatchToNodeFreelist(getNodeIndex(), _sc, _batch, dist);
      if (items < _batch) {
          fprintf(stderr, "Increase the size for PerNodeSizeClass or PerThreadSizeCalss for _sc %ld\n", _sc);
//          _discard = true;
          //assert(size != 0);
      }
  }
