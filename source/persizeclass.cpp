#include "mm.hh"
#include "persizeclass.hh"
#include "numaheap.hh"

// Get the freed objects from the pernode freelist when necessary
int PerSizeClass::getObjectsFromCentralList(int nodeIndex, void ** head, void ** tail) {
  int numb;

  if(_allocs >= _allocsBeforeCheck) {
    _allocs = 0;

    // Try to allocate objects from the node's freelist 
    numb = NumaHeap::getInstance().moveBatchFromNodeFreelist(nodeIndex, _sc, _batch, head, tail); 

    // If we could get the specified number, let's encourage this by reducing the checking threshold
    if(numb > 0 && (numb == _batch)) {
      _allocsBeforeCheck /= 2;
      if(_allocsBeforeCheck <= _allocsCheckMin) {
        _allocsBeforeCheck = _allocsCheckMin;
      }
    }
    else if (numb == 0) {
      // Can't find available objects, then check less frequently. 
      _allocsBeforeCheck *= 2;

      if(_allocsBeforeCheck > _allocsCheckMax) {
        _allocsBeforeCheck = _allocsCheckMax;
      }
    }
  }

  // Get the objects from the node's freelist. 
  return numb;
}


// Get objects from the bump pointer, and save the list to head and tail.
int PerSizeClass::getObjectsFromBumpPointer(void ** head, void ** tail) {
  int numb = 0;

  if(_bumpPointer < _bumpPointerEnd) {
    if(_size <= SIZE_CLASS_SMALL_SIZE) {
      numb = _miniBagObjects;

      if(_bumpPointerEnd - _bumpPointer < 2*PAGE_SIZE) {
        numb = (_bumpPointerEnd-_bumpPointer)/_size;
        _bumpPointer = _bumpPointerEnd;
      }
      else {
        _bumpPointer += _miniBagSize;
      }

      // Split the block into pieces and connect them together
      char * iptr = _bumpPointer; 
      *head = iptr;
      tail = (void **)iptr;
      unsigned int count = 0;
      while (count < numb) {
        iptr += _size;
        *tail = iptr;
        tail = reinterpret_cast<void**>(iptr);
        count++;
      }
      *tail = (void **)(iptr-_size);
    }
    else {
      numb = 1; 
      *head = _bumpPointer;
      *tail = NULL;

      // Update the object to next object
      _bumpPointer += _size;
    }
  }

  return numb;
}

int PerSizeClass::updateBumpPointerAndGetObjects(void * bptr, void ** head, void ** tail) {
  _bumpPointer = (char *)bptr;
  if(_size < SIZE_CLASS_SMALL_SIZE) {
    //  fprintf(stderr, "_size %d, _bagUsableSize %d\n", _size, _bagUsableSize);
    _bumpPointerEnd = _bumpPointer + _bagUsableSize;
  }
  else {
    _bumpPointerEnd = _bumpPointer + _bagSize;
  }

  // Now get objects from bumppointer
  return getObjectsFromBumpPointer(head, tail);
}

