#include "mainheap.hh"

   void * MainHeap::allocate(size_t size) {
     void * ptr = NULL;
     unsigned long address = (unsigned long)&size;
     //fprintf(stderr, "size is at %p address %lx\n", &size, address);
     address += *((unsigned long *)(address + MALLOC_SITE_OFFSET));
     CallsiteInfo info;
     info.init(); 
     //fprintf(stderr, "size %ld with address %lx\n", size, address);
     //fprintf(stderr, "size %ld (%p) instruction %lx\n", size, &size, *((unsigned long *)(((unsigned long)&size) + MALLOC_SITE_OFFSET)));

     if(size == 16) { 
       //while(1) { ; }
     }

     CallsiteInfo * oinfo = _callsiteMap.findOrAdd((void *)address, 0, info);
     // Check whether this is private callsite?
     if(oinfo && oinfo->isPrivateCallsite()) {
      //fprintf(stderr, "size %ld instruction %lx address %lx is private\n", size, *((unsigned long *)(((unsigned long)&size) + MALLOC_SITE_OFFSET)), address);
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

     // Now insert this object to the map, so that we could confirm its privateness
     ObjectInfo objInfo(_mhSequence, oinfo);
     _objectsMap.insert((void *)ptr, 0, objInfo);
     //fprintf(stderr, "allocate ptr %p\n", ptr); 
     return ptr;
   }



  void MainHeap::deallocate(void * ptr) {
     size_t size = getSize(ptr);

     if(_mainHeapPhase) {
      ObjectInfo * info = _objectsMap.find((void *)ptr, 0);
      if(info) {
        if(info->getSequence() == _mhSequence) {
          // Set the corresponding callsite to private, avoiding subsequent
          // allocations from the special heap.
          CallsiteInfo * csInfo = info->getCallsiteInfo();
          //fprintf(stderr, "csInfo is at %p\n", csInfo);
          csInfo->setPrivateCallsite();
        }
        // If current sequence number is larger, then the callsite will still be shared.
        // There is no need to change
      }

      // Now simply delete this entry
      _objectsMap.erase((void *)ptr, 0);
    }
    else {
      _objectsMap.erase((void *)ptr, 0);
    }

     if(!isBigObject(size)) {
       int sc = size2Class(size); 
      
      if(!_mainHeapPhase) { 
        pthread_spin_lock(&_lock);
        // Return to the size class
        _sizes[sc].deallocate(ptr);
        pthread_spin_unlock(&_lock);
      }
      else {
        _sizes[sc].deallocate(ptr);
      }
     }
     else {
      if(!_mainHeapPhase) { 
        pthread_spin_lock(&_lock);
        _bigObjects.deallocate(ptr, size);
        pthread_spin_unlock(&_lock);
      }
      else {
        _bigObjects.deallocate(ptr, size);
      }
     }
   }
