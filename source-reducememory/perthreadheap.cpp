#include "perthreadheap.hh"
#include "numaheap.hh"

int PerThreadHeap::allocateFromPerNodeSmallFreeList(unsigned int classIndex, void ** head, void ** tail) {
  return NumaHeap::getInstance().allocateFromPerNodeSmallFreeList(_nodeIndex, classIndex, head, tail);
}

void PerThreadHeap::donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail) {
  // Donate objects to the node's freelist
  NumaHeap::getInstance().donateObjectsToNode(_nodeIndex, classIndex, batch, head, tail);
}
