#ifndef __PER_THREAD_HEAP_HH__
#define __PER_THREAD_HEAP_HH__

#include "xdefines.hh"
#include "persizeclasslist.hh"
#include "perthreadsizeclass.hh"

// PerThreadHeap only tracks freed small objects. 
class PerThreadHeap {
private:
  PerThreadSizeClass _sclass[SMALL_SIZE_CLASSES];

public:
  void initialize(int tindex, int nindex) {
    int i;

    unsigned long classSize = 16;
    unsigned long size = 0; 
    for(i = 0; i < SMALL_SIZE_CLASSES; i++) {
      // Try to set perthreadsize classes
      unsigned int objects = SIZE_ONE_MB * 4/classSize;

  //   fprintf(stderr, "sc %d classSize %ld\n", i, classSize);
      _sclass[i].initialize(nindex, classSize, i, objects);

      if(classSize < SIZE_CLASS_TINY_SIZE) {
        classSize += 16;
      }
      else if(classSize < SIZE_CLASS_SMALL_SIZE) {
        classSize += 32;
      }
      else {
        classSize *= 2;
      }
    }
  }

  PerThreadSizeClass * getPerThreadSizeClassFromSize(size_t sz) {
    int sc = size2Class(sz);
   //fprintf(stderr, "sz %ld sc %d\n", sz, sc); 
    return &_sclass[sc];
  } 

  PerThreadSizeClass * getPerThreadSizeClass(int sc) {
    return &_sclass[sc];
  }

  void * allocate(size_t sz) {
    void * ptr = NULL; 

  //  fprintf(stderr, "PERHEA allocate sz %d\n", sz);
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
