#ifndef __PER_THREAD_SIZE_CLASS_LIST_HH__
#define __PER_THREAD_SIZE_CLASS_LIST_HH__

#include <assert.h>
#include <stdio.h>
#include "persizeclasslist.hh"

class PerThreadSizeClassList {
private:
  unsigned int _classIndex;
  unsigned long _classSize;  // How big for each object

  /* Learning from TcMalloc: all objects in the miniBag will be added to the freelist so that
   * we could warm up the corresponding cache.
   * Since we target at the hardware that has at most 16 threads in a node, we could 
   * get 16 objects at most from the pernode heap if the class size is less than 4K. 
   * Otherwise, we could at most get two objects from per-node heap if the size is larger than 4K. 
   * But both parameters will be adjustable from the compilation flag. Therefore, we will have the generality. 
   *
   * Since the modern Linux 4.19.0-8-amd64 are using the huge pages by default, we are trying to reduce the 
   * number of virtual memory that has been used. We can't just use 1M for one thread any more, since that will
   * waste just 1M memory. Therefore, we are designing such minibag machenism in order to reduce the memory consumption. 
   * Due to this reason, we don't maintain the bump pointer for per-thread's size class. Instead, only the per-node heap
   * will maintain the bump pointer.
   */ 
  unsigned long _batchSize;
  unsigned int _batch; 
  
  /* Since everytime the perthreadSizeClass will have to check the per-node list, it doesn't need to maintain 
   * allocs before check any more. Basically, if the number of objects is larger than _miniBagObjects, then we will
   * contribute them to the per-node list. 
   */
  PerSizeClassList _list; 

public: 
  void initialize(int classSize, int classIndex) {
    _classSize = classSize;
    _classIndex = classIndex;

    if(classSize < SIZE_CLASS_TINY_SIZE) {
      int objects = PAGE_SIZE/classSize;
      _batch = objects;
    }
    else if((classSize * BATCH_NUMB_SMALL_OBJECTS) <= PAGE_SIZE) {
      _batch = BATCH_NUMB_SMALL_OBJECTS; 
    }
    else {
      _batch = 2; 
    }

    _batchSize = _batch * classSize;
    // Now initialize the corresponding size
    _list.initialize();
  }

 
  int getBatch(void) {
    return _batch;
  }

  int getBatchSize(void) {
    return _batchSize;
  }

  size_t getClassIndex(void) {
    return _classIndex;
  }

  size_t getClassSize(void) {
    return _classSize;
  }

  bool hasItems(void) {
    return _list.items() > 0;
  }

  // Allocate an object. It will return NULL
  // if no objects left
  void * allocate() {
    return _list.pop();
  }

  void pushRangeList(int numb, void * head, void * tail) {
    if(numb > 0) {
     // fprintf(stderr, "push numb %d head %p tail %p\n", numb, head, tail);
      _list.pushRange(numb, head, tail);
    }
  }

  void pushRange(void * head, void * tail) {
    unsigned int numb; 

    numb = ((uintptr_t)tail - (uintptr_t)head)/_classSize;

    if(numb == 0) {
      return;
    }

    char * iptr = (char *)head;

    void ** iter = (void **)head;
    unsigned int count = 0;
    while (count < numb) {
      iptr += _classSize;
      *iter = iptr;
      iter = reinterpret_cast<void**>(iptr);
      count++;
    }
        
    tail = (void *)(iptr - _classSize);

    pushRangeList(numb, head, tail);
  }


  int getDonateObjects(void ** head, void ** tail) {
    int numb = 0;

    if(_list.items() > _batch) {
      numb = _list.popRange(_batch, head, tail);
    }

    return numb;
  }

  bool toDonate(void) {
    return _list.items() > _batch * 2;
  }

  void deallocate(void *ptr) {
    _list.push(ptr);
  }
};

#endif
