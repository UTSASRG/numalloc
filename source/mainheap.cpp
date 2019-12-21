
#include "mainheap.hh"

  void * MainHeap::allocate(size_t size) {
    void * ptr = NULL;

    // If the callstack is shared or unknown, then we will allocated from the interleaved heap.
    // Allocate from the freelist at first. 
    int sc;

    if(isBigObject(size)) {
      ptr = _bigObjects.allocate(size, _nodeindex);
      return ptr; 
    }
     
    // If it is small object, allocate from the freelist at first. 
    sc = getSizeClass(size);
    ptr = _sizes[sc]->allocate();
      
    // Allocate from the bump pointer right now
    if(ptr == NULL) {
      int classsize = _sizes[sc]->getSize();

      // Now we are using different size requirement for this.
      if(classsize <= MT_MIDDLE_SPAN_THRESHOLD) {
        // Allocate one bag from the first span
        ptr = _bpSmall;
        _bpSmall += BAG_SIZE_SMALL_OBJECTS;
    
        // Adding remainning objects to the free list, except the first one. 
        _sizes[sc]->insertObjectsToFreeList((char *)ptr +classsize, _bpSmall);
      
        // Mark PerBagInfo
        markPerBagInfo(ptr, BAG_SIZE_SMALL_OBJECTS, classsize);
      }
      else if (size <= MT_BIG_SPAN_THESHOLD) {
        ptr = _bpMiddle;
        _bpMiddle += classsize;
        markPerBagInfo(ptr, classsize, classsize);
      }
    }
    return ptr;
  }

void MainHeap::deallocate(void * ptr) {
    if(ptr <= _bpMiddleEnd) {
      int sc = getSizeClass(ptr);
      //fprintf(stderr, "free ptr %p with size %lx\n", ptr, getSize(ptr));
      if(sc > 16) {
        unsigned long index = getBagIndex(ptr);
        PerBagInfo * info = &_info[index];
        fprintf(stderr, "ptr %p with index %ld, info %p size %x\n", ptr, index, info, info->size);

        exit(-1);
      }
      _sizes[sc]->deallocate(ptr);
    }
    else { 
      _bigObjects.deallocate(ptr);
    }
  }
