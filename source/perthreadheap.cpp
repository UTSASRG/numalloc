#include "xdefines.hh"
#include "perthreadheap.hh"
#include "persizeclass.hh"
#include "numaheap.hh"

void * PerThreadHeap::allocateOneBag(PerSizeClass * sc) {
  void * ptr = NULL; 
  bool allocBig = false; 
  
  allocBig = sc->allocFromBigObjects();

  ptr = NumaHeap::getInstance().allocateOneBagFromNode(_nodeIndex, sc->getClassSize(), sc->getBagSize(), allocBig);
  if(ptr) { 
    sc->updateBags();
  }

  return ptr;
}

void PerThreadHeap::donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail) {
  // Donate objects to the node's freelist
  NumaHeap::getInstance().donateBatchToNodeFreelist(_nodeIndex, classIndex, batch, head, tail);
}
