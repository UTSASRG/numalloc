#include "pernodesizeclassbag.hh"
#include "numaheap.hh"

void* PerNodeSizeClassBag::allocateOneBagFromNode(void) {
  return NumaHeap::getInstance().allocateOneBagFromNode(_nodeIndex, _classSize, SIZE_ONE_BAG,_bags>0); 
}
