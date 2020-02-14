
#include "mm.hh"
#include "perthreadsizeclass.hh"
#include "persizeclasslist.hh"
#include "numaheap.hh"

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
    _list.pushRange(ret, head, tail);

  return ret;
}

void PerThreadSizeClass::donateObjectsToNodeFreelist() {
  void * head, * tail;
  
  _list.popRange(_batch, &head, &tail);

  // Donate objects to the node's freelist
  //fprintf(stderr, "Thread %d :donate batch %d to node list, remain objects %ld\n", getNodeIndex(), _batch, _list.length());
  NumaHeap::getInstance().donateBatchToNodeFreelist(_nodeindex, _sc, _batch, head, tail);
}

void * PerThreadSizeClass::allocate() {
    void * ptr = NULL; 
    
    ptr = _list.pop();
    if(ptr) {
      return ptr;
    } 
     
    if(_allocs >= _allocsBeforeCheck) {
      _allocs = 0;
   
      // If no available objects, allocate objects from the node's freelist 
      int numb = moveObjectsFromNodeFreelist();

      // Now let's place this list to the current list. 
      if(numb > 0) {
        ptr = _list.pop();
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
      if(_bumpPointer == _bumpPointerEnd) {
        // Now get a chunk from the current PerNodeHeap: either from big objects or never allocated ones
        _bumpPointer = (char *)NumaHeap::getInstance().allocateOneBagFromNode(getNodeIndex(), _size, _bagSize); 
        if(_size < SIZE_CLASS_SMALL_SIZE) {
        //  fprintf(stderr, "_size %d, _bagUsableSize %d\n", _size, _bagUsableSize);
          _bumpPointerEnd = _bumpPointer + _bagUsableSize;
        }
        else {
          _bumpPointerEnd = _bumpPointer + _bagSize;
        }
      }

      // Now allocate from the never used ones
      ptr = _bumpPointer;
      if(_size <= SIZE_CLASS_SMALL_SIZE) {
        int numObjects = _miniBagObjects;

        if(_bumpPointerEnd - _bumpPointer < 2*PAGE_SIZE) {
          numObjects = ((_bumpPointerEnd-_bumpPointer)/_size) - 1;
          _bumpPointer = _bumpPointerEnd;
          //fprintf(stderr, "_size %d numObjects %d \n", _size, numObjects);
        }
        else {
          _bumpPointer += _miniBagSize;
          //fprintf(stderr, "_size %d numObjects %d\n", _size, numObjects);
        }

        // Split the block into pieces and add to the free-list
        char * iptr = ((char*)ptr) + _size;
        void * head = iptr;
        void ** tail = (void **)head;
        unsigned int count  = 0;
        while (count < numObjects) {
          iptr += _size;
          *tail = iptr;
          tail = reinterpret_cast<void**>(iptr);
          count++;
        }
       
        tail = (void **)(iptr-_size);
        _list.pushRange(numObjects, head, tail); 
      }
      else {
        _bumpPointer += _size;
      }
    }
    return ptr;
  }
 
  // Return an object starting with ptr to the PerThreadSizeClass
  void PerThreadSizeClass::deallocate(void *ptr) {
    // Put this entry to the list. 
    _list.push(ptr);

    // If there is no spot to hold next deallocation, put some objects to PerNodeSizeClass 
    if(_list.length() >= _watermark) {
      // Donate the _batch items to the pernodefreelist 
      donateObjectsToNodeFreelist();
    }

    return; 
  }
