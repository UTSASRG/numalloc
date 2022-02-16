#include "interheap.hh"

void * InterHeap::allocate(size_t size) {
  void * ptr = NULL;
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
  // Check the address range of this ptr
  size_t offset = (intptr_t)ptr - (intptr_t)_begin;

  if(offset < SIZE_PER_SPAN) {
    size_t size = getSize(ptr);
    int sc = size2Class(size); 
    
    _smallClasses[sc].deallocate(ptr);
    // fprintf(stderr, "Deallocate small object with size %lx sc %d ptr %p\n", size, sc, ptr);
  }
  else {
    _bigObjects.deallocate(ptr);
    // fprintf(stderr, "Deallocate big object with ptr %p\n", ptr);
   }
}
