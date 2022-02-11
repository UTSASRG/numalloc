#ifndef __PER_THREAD_HEAP_HH__
#define __PER_THREAD_HEAP_HH__

#include "xdefines.hh"
#include "perthreadsizeclasslist.hh"

// PerThreadHeap only tracks freed small objects. 
class PerThreadHeap {
private:
  unsigned long _nodeIndex;
  unsigned long _threadIndex;
  PerThreadSizeClassList _sclass[SMALL_SIZE_CLASSES];

public:
  void initialize(int tindex, int nindex) {
    // Save the node index
    _nodeIndex = nindex; 
    _threadIndex = tindex;

    // fprintf(stderr, "perthreadheap.hh: initialize _sclass(PerThreadSizeClassList )\n");
    unsigned long classSize = SIZE_CLASS_START_SIZE;
    for(int i = 0; i < SMALL_SIZE_CLASSES; i++) {

      _sclass[i].initialize(classSize, i);

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

  PerThreadSizeClassList * getPerThreadSizeClassList(size_t size) {
    int classIndex = size2Class(size);
    return &_sclass[classIndex];
  }
  
  PerThreadSizeClassList * getPerThreadSizeClassListByIndex(int classIndex) {
    return &_sclass[classIndex];
  }

  void * allocate(size_t size) {
    void * ptr = NULL;
    PerThreadSizeClassList * sc;

    // Allocate from the freelist at first.
    sc = getPerThreadSizeClassList(size);

    // We will allocate from the per-thread size class. 
    void * head = NULL;
    int numb = 0;
    if(sc->hasItems() != true) {
      void * tail = NULL;
     
      // Get objects from Node. 
      // fprintf(stderr, "perthreadheap.hh:: allocate size %lx (sc=%d) from PerNodeHeap\n", size, sc->getClassIndex());
      numb = getObjectsFromNode(sc->getClassIndex(), &head, &tail);

      //fprintf(stderr, "Getting size %ld objects %d\n", sc->getClassSize(), numb);
      if(numb == 0) {

        // We don't have freed objects in PerNode's freelist. Instead, 
        // all objects will be returned by a range of memory.
        // Therefore, we need to add all objects in the specified range into the freelist. 
        unsigned int classSize = sc->getClassSize();
       
        // Only push the remaining objects in the continuous range to the freelist
        sc->pushRange((void *)((unsigned long)head + classSize), tail);
      }
      else {
        if(numb > 1) {
          // Push the remaining objects in the list (pointed by head and tail) into the freelist.
          sc->pushRangeList(numb-1, *((void **)head), tail);
        }
      }
      ptr = head; 
    }
    else {
      // fprintf(stderr, "perthreadheap.hh:: allocate size %lx (sc=%d) from PerThreadSizeClassList(_sclass)\n", size, sc->getClassIndex());
      ptr = sc->allocate();
    }
    // fprintf(stderr, "perthreadheap.hh:: givenSize=%lx, allocatedSize=%lx, sizeClass=%d, ptr=%p\n\n", size, sc->getClassSize(), sc->getClassIndex(), ptr);
    return ptr;
  }

  // Deallocate an object to this thread's freelist 
  void deallocate(void * ptr, int classIndex) {
    // fprintf(stderr, "perthreadheap.hh: deallocate ptr %p with sc=%d \n", ptr, classIndex);
    PerThreadSizeClassList * sc = getPerThreadSizeClassListByIndex(classIndex);

    bool toDonate = sc->deallocate(ptr);

    // Check if there are too many freed objects in the freelist, if yes,
    // then donateObjects to the pernode freelist if necessary
    if(toDonate) {
      void * head, * tail;
     
      int numb = sc->getDonateObjects(&head, &tail);
      // fprintf(stderr, "perthreadheap.hh: donating Size %ld objects numb %d\n", sc->getClassSize(), numb);
      donateObjectsToNode(classIndex, numb, head, tail);
    }

    return;
  }
  
  /* This is an important function to communicate with the Node.
   * If the Node has freed objects, then we will utilize head and tail to bring the list of objects. 
   * Also, the return number will be the number of objects in the freelist. 
   * However, if Node does not have freed objects in its freelist, it will simply return a range of memory, which is also brought back by the head and tail pointer. At the same time, the returned number will be 0. 
   * If Node do not have freed objects, and it also cannot allocate, it will fail. There is no need to handle in this function.
   */
  int getObjectsFromNode(unsigned int classIndex, void ** head, void ** tail);

  void donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail); 
};

#endif
