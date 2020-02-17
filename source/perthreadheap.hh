#ifndef __PER_THREAD_HEAP_HH__
#define __PER_THREAD_HEAP_HH__

#include "xdefines.hh"
#include "persizeclasslist.hh"
#include "persizeclass.hh"

// PerThreadHeap only tracks freed small objects. 
class PerThreadHeap {
private:
  unsigned long _nodeIndex;
  PerSizeClass _sclass[SMALL_SIZE_CLASSES];

public:
  void initialize(int tindex, int nindex) {
    // Save the node index
    _nodeIndex = nindex; 

    unsigned long classSize = 16;
    unsigned long size = 0; 
    for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {
      // Try to set perthreadsize classes
      unsigned int objects = SIZE_ONE_MB * 4/classSize;

      _sclass[i].initialize(classSize, i, objects);

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

  PerSizeClass * getPerSizeClass(size_t size) {
    int scIndex = size2Class(size);
    return &_sclass[scIndex];
  }

  void * allocateOneBag(size_t bagSize, size_t classSize);
  void donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail);

  void * allocate(size_t size) {
    PerSizeClass * sc;

    // If it is small object, allocate from the freelist at first.
    sc = getPerSizeClass(size);

    // We will allocate from the per-thread size class. 
    if(sc->hasItems() != true) {
      void * head = NULL;
      void * tail = NULL;

      // Get objects from the central list. 
      int numb = sc->getObjectsFromCentralList(_nodeIndex, &head, &tail);

      if(numb == 0) {
        // Get objects from the bump pointer (with the cache warmup mechanism) 
        numb = sc->getObjectsFromBumpPointer(&head, &tail);
      
       //fprintf(stderr, "line %d: numb %ld\n", __LINE__, numb); 
        // Now we should get a new block and update the bumppointer 
        if(numb == 0) {
          void * bPtr = allocateOneBag(sc->getBagSize(), sc->getClassSize());

          // Update bumppointers and get objects
          numb = sc->updateBumpPointerAndGetObjects(bPtr, &head, &tail);
       //fprintf(stderr, "line %d: numb %ld\n", __LINE__, numb); 
          assert(numb >= 0);
        }
      }

       //fprintf(stderr, "line %d: numb %ld\n", __LINE__, numb); 
      // Push these objects into the freelist.
      sc->pushRangeToList(numb, head, tail);
    }
      
    // Get one object from the list.
    void * ptr = sc->allocateFromFreeList();

    assert(ptr != NULL);

    return ptr;
  }

  // Deallocate an object to this thread's freelist 
  void deallocate(void * ptr, int classIndex) {
    PerSizeClass * sc = getPerSizeClass(classIndex);

    sc->deallocate(ptr);

    // Check if there are too many freed objects in the freelist, if yes,
    // then donateObjects to the pernode freelist if necessary
    if(sc->checkDonation() == true) {
      void * head, * tail;
     
      int numb = sc->getDonateObjects(&head, &tail);

      donateObjectsToNode(classIndex, numb, head, tail);
    }

    return;
  } 
};

#endif
