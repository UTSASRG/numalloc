#include "interheap.hh"

void * InterHeap::allocate(size_t size) {
  void * ptr = NULL;
#if 0
  unsigned long address = (unsigned long)&size;
  //fprintf(stderr, "user address %lx\n", *((unsigned long *)(address + MALLOC_SITE_OFFSET)));
  address += *((unsigned long *)(address + MALLOC_SITE_OFFSET));

  // No need to check the callsite now
  //fprintf(stderr, "size is at %p address %lx\n", &size, address);
  CallsiteInfo info;
  info.init(); 

  CallsiteInfo * oinfo = _callsiteMap.findOrAdd((void *)address, 0, info);
  // If this is a private callsite, we will use the normal per-thread heap for the
  // allocation, which ensures the allocation will from the local node, instead
  // of using the interleaved heap
  if(oinfo && oinfo->isPrivateCallsite()) {
    return ptr; 
  }
#endif

  if(isBigObject(size)) {
    ptr = allocateBigObject(size);
    // fprintf(stderr, "Allocate big object with size %lx ptr %p\n", size, ptr);
  }
  else {
    ptr = allocateSmallObject(size);
    // fprintf(stderr, "Allocate small object with size %lx ptr %p\n", size, ptr);
  }
  return ptr;
}

void InterHeap::deallocate(void * ptr) {
  size_t size = getSize(ptr);

#if 0  
  if(_sequentialPhase) {
    ObjectInfo * info = _objectsMap.find((void *)ptr, 0);
    if(info) {
      if(info->getSequence() == _mhSequence) {
        // Set the corresponding callsite to private, avoiding subsequent
        // allocations from the special heap.
        CallsiteInfo * csInfo = info->getCallsiteInfo();
        //fprintf(stderr, "csInfo is at %p\n", csInfo);
        csInfo->setPrivateCallsite();
      }
      // If _mhSequence is larger, then the allocation occurs in the last epoch.
      // Then the callsite will be shared (same as the default value), and no need to change
    }

    // Now simply delete this entry
    _objectsMap.erase((void *)ptr, 0);
  }
  else {
    _objectsMap.erase((void *)ptr, 0);
  }
#endif 

  if(!isBigObject(size)) {
    int sc = size2Class(size); 
    
    _smallClasses[sc].deallocate(ptr);
    // fprintf(stderr, "Deallocate small object with size %lx sc %d ptr %p\n", size, sc, ptr);
  }
  else {
    _bigObjects.deallocate(ptr, size);
    // fprintf(stderr, "Deallocate big object with size %lx ptr %p\n", size, ptr);
   }
}
