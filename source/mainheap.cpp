
#include "mainheap.hh"
#include "selfmap.hh"

#define CALLSTACK_SKIP_LEVEL 2 
#define MY_DEBUG 0
int getCallStack(int depth, callstack *cs) {

  // Fetch the frame address of the topmost stack frame
  struct stack_frame * current_frame = NULL;

  // Loop condition tests the validity of the frame address given for the
  // previous frame by ensuring it actually points to a location located
  // on the stack
  void * caller_addr = NULL;

  current_frame = (struct stack_frame *)(__builtin_frame_address(CALLSTACK_SKIP_LEVEL));

  // Initialize the prev_frame pointer to equal the current_frame. This
  // simply ensures that the while loop below will be entered and
  // executed and least once
  struct stack_frame * prev_frame = current_frame;

  int level = 0;
  int loop_counter = 0;
  while(level < depth &&  
      ((void *)prev_frame <= current->startFrame) && 
      (prev_frame >= current_frame) &&
      (loop_counter++ < MAX_SEARCH_CALLSTACK_DEPTH)) {

    caller_addr = prev_frame->caller_address;

    if(!selfmap::getInstance().isThisLibrary(caller_addr)){
      cs->stack[level++] = caller_addr;
      //fprintf(stderr, "calleraddr %p\n", caller_addr);
    }

    if(prev_frame == prev_frame->prev){
      break;
    }
    // Walk the prev_frame pointer backward in preparation for the
    // next iteration of the loop
    prev_frame = prev_frame->prev;
  }

//  fprintf(stderr, "level is %d depth is %d\n", level, depth);

  if(level < depth) {
    for(int i=level; i<depth; i++) {
      cs->stack[i] = NULL;
    }
  }

  return level;
}
      

  void * MainHeap::allocate(size_t size) {
    void * ptr = NULL;

#if MY_DEBUG      
    // Get the callstack right now.
    callstack cs;
    struct CallsiteInfo info;

    // Collect the callstack
    getCallStack(2, &cs); 
    cs.hashcode = hash_range(cs.stack, 0, 2);
    
    // Check whether the callstack is in the hashmap or not.
    CallsiteInfo * oldinfo = _callsiteMap.findOrAdd(cs, sizeof(cs), info);
      
    // Check whether this callstack is shared or not
    // If not, then we will use the normal allocation, instead of the shared heap.
    if(oldinfo) {
      if(oldinfo->isPrivateCallsite()) {
        //while(1) { ; }
        //  fprintf(stderr, "***********a private callsite*********\n");
        return NULL;
      }
      else {
        oldinfo->updateAlloc();
      }
    }
 #endif
    //_specialMalloc++;

    //if(_specialMalloc % 10000 == 0) {
    //  fprintf(stderr, "specialMalloc %ld\n", _specialMalloc);
   // }
    // If the callstack is shared or unknown, then we will allocated from the interleaved heap.
    // Allocate from the freelist at first. 
    int sc;

    if(isBigObject(size)) {
      ptr = _bigObjects.allocate(size, _nodeindex);
      return ptr; 
    }
     
    // If it is small object, allocate from the freelist at first. 
    sc = getSizeClass(size);
    ptr = _sizes[sc]->allocate();
      
    // Allocate from the bump pointer right now
    if(ptr == NULL) {
      int classsize = _sizes[sc]->getSize();

      // Now we are using different size requirement for this.
      if(classsize <= MT_MIDDLE_SPAN_THRESHOLD) {
        // Allocate one bag from the first span
        ptr = _bpSmall;
        _bpSmall += BAG_SIZE_SMALL_OBJECTS;
    
        // Adding remainning objects to the free list, except the first one. 
        _sizes[sc]->insertObjectsToFreeList((char *)ptr +classsize, _bpSmall);
      
        // Mark PerBagInfo
        markPerBagInfo(ptr, BAG_SIZE_SMALL_OBJECTS, classsize);
      }
      else if (size <= MT_BIG_SPAN_THESHOLD) {
        ptr = _bpMiddle;
        _bpMiddle += classsize;
        markPerBagInfo(ptr, classsize, classsize);
      }
    }
  
#if MY_DEBUG
    // We will only add the first object into the objectsHashMap to speedup the search later. 
    if(!oldinfo) {
      CallsiteInfo* info = _callsiteMap.find(cs, sizeof(cs));
      ObjectInfo objInfo(_mhpSequence, info);

      // For the performance reason, we will save the entry of another hashmap there. 
      _objectsMap.insert((void *)ptr, sizeof(void *), objInfo); 
    }
#endif    
    return ptr;
  }

void MainHeap::deallocate(void * ptr) {
#if MY_DEBUG
  // Check the phase information of this deallocation. 
  // If the current phase is the same as the allocation phase, 
  // then we will mark the corresponding callsite as private
  if(_mainHeapPhase) {
    ObjectInfo * info = _objectsMap.find((void *)ptr, sizeof(void *)); 

    if(info) {
  //    fprintf(stderr, "in deallocate, with object phase %d\n", info->getSequence());
      if(info->getSequence() == _mhpSequence) {
        // Set the corresponding callsite to private, avoiding subsequent 
        // allocations from the special heap.
        CallsiteInfo * csInfo = info->getCallsiteInfo();
        //fprintf(stderr, "csInfo is at %p\n", csInfo);
        csInfo->setPrivateCallsite();
      }
    }

    // Now simply delete this entry
    _objectsMap.erase((void *)ptr, sizeof(void *));
  } 
#endif
    if(ptr <= _bpMiddleEnd) {
      int sc = getSizeClass(ptr);
      //fprintf(stderr, "free ptr %p with size %lx\n", ptr, getSize(ptr));
      if(sc > 16) {
        unsigned long index = getBagIndex(ptr);
        PerBagInfo * info = &_info[index];
        fprintf(stderr, "ptr %p with index %ld, info %p size %x\n", ptr, index, info, info->size);

        exit(-1);
      }
      _sizes[sc]->deallocate(ptr);
    }
    else { 
      _bigObjects.deallocate(ptr);
    }
  }
