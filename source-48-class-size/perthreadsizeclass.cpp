
#include "mm.hh"
#include "perthreadsizeclass.hh"
#include "numaheap.hh"

// Allocate reversely. Since _next always points to
// the next availabe slot, we will use the object pointed by _next-1
void * PerThreadSizeClass::allocateFromFreelist() {
  return _flist.Pop();
}

/*
    void *tail, *head;
    src->PopRange(batch_size, &head, &tail);
    central_cache[cl].InsertRange(head, tail, batch_size);
*/
int PerThreadSizeClass::moveObjectsFromNodeFreelist() {
  int ret;
  void * head, * tail;

  // Get the objects from the node's freelist. 
  ret = NumaHeap::getInstance().moveBatchFromNodeFreelist(_nodeindex, _sc, _batch, &head, &tail);

  // Now place these objects to the freelist
  if(ret > 0)
    _flist.PushRange(ret, head, tail);

  return ret;
}

void PerThreadSizeClass::donateObjectsToNodeFreelist() {
  void * head, * tail;
  
  _flist.PopRange(_batch, &head, &tail);

  // Donate objects to the node's freelist
  //fprintf(stderr, "Thread %d :donate batch %d to node list, remain objects %ld\n", getNodeIndex(), _batch, _flist.length());
  NumaHeap::getInstance().donateBatchToNodeFreelist(_nodeindex, _sc, _batch, head, tail);
}

void * PerThreadSizeClass::allocate() {
    void * ptr = NULL; 
    if(_flist.hasItems()) {
      ptr = allocateFromFreelist();
      if(ptr == NULL) {
        fprintf(stderr, "_size is %d _sc %d, ptr %p\n", _size, _sc, ptr);
      }
      return ptr; 
    }
      
    if(_allocs >= _allocsBeforeCheck) {
      _allocs = 0;
   
      // If no available objects, allocate objects from the node's freelist 
      int numb = moveObjectsFromNodeFreelist();

      // Now let's place this list to the current list. 
      if(numb > 0) {
        ptr = allocateFromFreelist(); 
        if(numb == _batch) {
          // Since we could get the objects, let's reduce the threshold for getting from the global list
          _allocsBeforeCheck /= 2;
          if(_allocsBeforeCheck <= _allocsCheckMin) {
            _allocsBeforeCheck = _allocsCheckMin;
          }
        }
      }
      else {
        // Can't find available objects in PerNodeFreeList, then we will 
        // check less frequently. 
        _allocsBeforeCheck *= 2;
        if(_allocsBeforeCheck > _allocsCheckMax) {
          _allocsBeforeCheck = _allocsCheckMax;
        }
      }
    }
    
    if(ptr == NULL) {
      _allocs++;
   
      // Get a new block if necessary 
      //if(_bumpPointer == _bumpPointerEnd) {
      if(_bumpPointer + _size > _bumpPointerEnd) {
        // Now get a chunk from the current PerNodeHeap: either from big objects or never allocated ones
        _bumpPointer = (char *)NumaHeap::getInstance().allocateOneBagFromNode(getNodeIndex(), _size, _bagSize); 
        _bumpPointerEnd = _bumpPointer + _bagSize;

//	if(_size == 48) {
  //        _bumpPointerEnd -= 16;
         // fprintf(stderr, "Getting one bag (size 48) from the node %p\n", _bumpPointer);
   //    }
      }

      // Now allocate from the never used ones
      ptr = _bumpPointer;
      _bumpPointer += _size;
    }
    return ptr;
  }
 
  // Return an object starting with ptr to the PerThreadSizeClass
  void PerThreadSizeClass::deallocate(void *ptr) {
    // Put this entry to the list. 
    _flist.Push(ptr);

    // If there is no spot to hold next deallocation, put some objects to PerNodeSizeClass 
    if(_flist.length() >= _watermark) {
      // Donate the _batch items to the pernodefreelist 
      donateObjectsToNodeFreelist();
    }

    return; 
  }
