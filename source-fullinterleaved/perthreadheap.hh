#ifndef __PER_THREAD_HEAP_HH__
#define __PER_THREAD_HEAP_HH__

#include "xdefines.hh"
#include "perthreadsizeclass.hh"

// PerThreadHeap only tracks freed small objects. 
class PerThreadHeap {
private:
  size_t _scMagicValue;
  PerThreadSizeClass _sclass[SMALL_SIZE_CLASSES];

public:
  void initialize(int tindex, int nindex) {
    int i;

    unsigned long classSize = 16;
    unsigned long size = 0; 
    for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
      // Size is the total size of array for each size class. 
      // Each perthreadsizeclass will hold 2M/classSize number of objects
      // When perthreadsizeclass has too many freed objects, the objects will be putted into pernodesizeclass
      size += (SIZE_ONE_MB_BAG * 2 * sizeof(void *))/classSize;
      classSize *= 2;
    }

    char * ptr = (char *)MM::mmapFromNode(alignup(size, PAGE_SIZE), nindex); 
    if(tindex == 0)
    fprintf(stderr, "PerThreadHeap %d initialization nindex %d from %p to %p\n", tindex, nindex, ptr, ptr + size ); 
    classSize = 16; 
    for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
      unsigned int objects = SIZE_ONE_MB_BAG * 2/classSize;;
      size = objects * sizeof(void *);
      _sclass[i].initialize(nindex, classSize, i, objects, (void *)ptr); 

      ptr += size;
      classSize *= 2;
    }

    _scMagicValue = 32 - LOG2(SIZE_CLASS_START_SIZE); 
    //fprintf(stderr, "********_scMagivValue %ld\n", _scMagicValue);
  }

  PerThreadSizeClass * getPerThreadSizeClassFromSize(size_t sz) {
    int sc;

    if(sz <= SIZE_CLASS_START_SIZE) {
      sc = 0;
    }
    else {
      sc = _scMagicValue - __builtin_clz(sz - 1);
      //fprintf(stderr, "sz %lx __builin %d _scMagivValue %ld sc %d\n", sz, __builtin_clz(sz - 1), _scMagicValue, sc); 
    }
    
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