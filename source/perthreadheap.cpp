#include "perthreadheap.hh"
#include "numaheap.hh"

int PerThreadHeap::getObjectsFromNode(unsigned int classIndex, void ** head, void ** tail) {
  return NumaHeap::getInstance().getObjectsFromNode(_nodeIndex, classIndex, head, tail);
}

void PerThreadHeap::donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail) {
  // Donate objects to the node's freelist
  NumaHeap::getInstance().donateObjectsToNode(_nodeIndex, classIndex, batch, head, tail);
}
