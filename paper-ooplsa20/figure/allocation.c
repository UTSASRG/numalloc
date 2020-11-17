void * malloc(size_t size) {
  void * ptr = NULL;

  if(size <= BIG_OBJECT_SIZE_THRESHOLD) {
    ptr = perthreadheap->allocate(size);
  }
  else {
    ptr = PerNodeHeap[nindex]->allocateBigObject(size); 
  }
}

