#include "pernodesizeclassbag.hh"
#include "numaheap.hh"

void* PerNodeSizeClassBag::allocateBagFromPerNodeHeap(int bagSize) {
  return NumaHeap::getInstance().allocateBagFromPerNodeHeap(_nodeIndex, _classSize, bagSize); 
}
