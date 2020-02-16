#include "xdefines.hh"
#include "perthreadheap.hh"
#include "numaheap.hh"

void * PerThreadHeap::allocateOneBag(size_t bagSize, size_t classSize) {
  return NumaHeap::getInstance().allocateOneBagFromNode(_nodeIndex, classSize, bagSize);
}

void PerThreadHeap::donateObjectsToNode(int classIndex, unsigned long batch, void * head, void * tail) {
  // Donate objects to the node's freelist
  NumaHeap::getInstance().donateBatchToNodeFreelist(_nodeIndex, classIndex, batch, head, tail);
}
