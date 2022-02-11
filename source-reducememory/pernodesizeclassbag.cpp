#include "pernodesizeclassbag.hh"
#include "numaheap.hh"

void* PerNodeSizeClassBag::allocateOneBagFromNode(int bagSize) {
  return NumaHeap::getInstance().allocateOneBagFromNode(_nodeIndex, _classSize, bagSize, _bags>0); 
}
