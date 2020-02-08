#ifndef __PER_THREAD_HEAP_HH__
#define __PER_THREAD_HEAP_HH__

#include "xdefines.hh"
#include "perthreadsizeclass.hh"

// PerThreadHeap only tracks freed small objects. 
class PerThreadHeap {
private:
  size_t _scMagicValue;

  PerThreadSizeClass _sclass[SMALL_SIZE_CLASSES + 1];

public:
  void initialize(int tindex, int nindex) {
    int i;

    unsigned long classSize = 16;
    unsigned long size = 0; 
    for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
      // Try to set perthreadsize classes
      unsigned int objects = SIZE_ONE_MB * 4/classSize;
      _sclass[i].initialize(nindex, classSize, i, objects);
      classSize *= 2;
    }

    // Assigned for 48 only
    _sclass[SMALL_SIZE_CLASSES].initialize(nindex, 48, SMALL_SIZE_CLASSES, 4096);

    _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE); 
  }

  PerThreadSizeClass * getPerThreadSizeClassFromSize(size_t sz) {
    int sc;

    if(sz <= SIZE_CLASS_START_SIZE) {
      sc = 0;
    }
   else if (sz == 48) {
      sc = SMALL_SIZE_CLASSES;
    }
    else {
      sc = _scMagicValue - __builtin_clz(sz - 1);
      //fprintf(stderr, "sz %lx __builin %d _scMagivValue %ld sc %d\n", sz, __builtin_clz(sz - 1), _scMagicValue, sc); 
    }
   
    //fprintf(stderr, "sz %ld sc %d\n", sz, sc); 
    return &_sclass[sc];
  } 

  PerThreadSizeClass * getPerThreadSizeClass(int sc) {
    return &_sclass[sc];
  }

  void * allocate(size_t sz) {
    void * ptr = NULL; 

    PerThreadSizeClass * ptclass = getPerThreadSizeClassFromSize(sz);

    // First, we will allocate from the per-thread size class. 
    // If succeed, then this allocation will not need lock (best speed).
    ptr = ptclass->allocate(); 
    if(ptr == NULL) {
      fprintf(stderr, "An object with sz %lx can't be allocated in perthreadsizclass\n", sz);
      assert(0); 
    }

    return ptr;
  }

  // Deallocate an object to this thread's freelist with the size class sc  
  void deallocate(void * ptr, int sc) {
    PerThreadSizeClass * ptclass = getPerThreadSizeClass(sc);

    ptclass->deallocate(ptr);
    return;
  } 

};

#endif
