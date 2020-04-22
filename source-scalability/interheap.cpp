#include "interheap.hh"

void * InterHeap::allocate(size_t size) {
  void * ptr = NULL;
  unsigned long address = (unsigned long)&size;
  //fprintf(stderr, "user address %lx\n", *((unsigned long *)(address + MALLOC_SITE_OFFSET)));
  address += *((unsigned long *)(address + MALLOC_SITE_OFFSET));
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

  if(isBigObject(size)) {
    //fprintf(stderr, "Allocate big object with size %lx\n", size);
    ptr = allocateBigObject(size);
  }
  else {
    ptr = allocateSmallObject(size);
  }
  //fprintf(stderr, "size %ld (at %p) instruction %lx, hashvalue %ld ptrhash %ld\n", size, &size, *((unsigned long *)(((unsigned long)&size) + MALLOC_SITE_OFFSET)), HashFuncs::hashStackAddr((void *)address, 0), HashFuncs::hashAllocAddr(ptr, 0));
  //fprintf(stderr, "insidemainheap (%p) instruction address %lx, hashaddress at %lx hashvalue %ld ptrhash %ld\n", &size, *((unsigned long *)(((unsigned long)&size) + MALLOC_SITE_OFFSET)) , address, HashFuncs::hashStackAddr((void *)address, 0), HashFuncs::hashAllocAddr(ptr, 0));

//  fprintf(stderr, "mainheap allocate size %lx ptr %p\n", size, ptr);

  // Now insert this object to the map, so that we could confirm its privateness
  ObjectInfo objInfo(_mhSequence, oinfo);
  _objectsMap.insert((void *)ptr, 0, objInfo);
  return ptr;
}

void InterHeap::deallocate(void * ptr) {
  size_t size = getSize(ptr);

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

  if(!isBigObject(size)) {
    int sc = size2Class(size); 
    
    if(!_sequentialPhase) { 
      pthread_spin_lock(&_lock);
      // Return to the size class
      _smallClasses[sc].deallocate(ptr);
      pthread_spin_unlock(&_lock);
    }
    else {
      _smallClasses[sc].deallocate(ptr);
    }
  }
  else {
    if(!_sequentialPhase) { 
      pthread_spin_lock(&_lock);
      _bigObjects.deallocate(ptr, size);
      pthread_spin_unlock(&_lock);
    }
    else {
      _bigObjects.deallocate(ptr, size);
    }
   }
}
