#ifndef __PER_NODE_SIZE_CLASS_HH__
#define __PER_NODE_SIZE_CLASS_HH__

#include "mm.hh"
#include "freelist.hh"
#include "pthread.h"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class PerNodeSizeClass {
  private: 
    pthread_spinlock_t _lock; 
    unsigned long _size; // Size of this size class
    FreeList _flist;

 public:
    void initialize(unsigned long size) {
      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      _size = size;

      // Map a chunk of memory in the local node
      _flist.Init();
    }  

    // Allocate multiple objects in a batch from the pernode's size class 
    int allocateBatch(unsigned long requestNumb, void ** head, void ** tail) {
      int  numb = 0;

      lock();
      
      numb = _flist.length(); 
      if(numb > requestNumb) {
        numb = requestNumb;
      }
      
      // Pop the specified number of entries; 
      if(numb > 0) 
        _flist.PopRange(numb, head, tail); 
//      fprintf(stderr, "Move batch from the pernode lsit.\n"); 
           
      unlock();

      return numb;
    }

    void deallocate(void * ptr) {
      lock();
      _flist.Push(ptr);
      unlock();
    }


    // Deallocate multiple objects to the pernode's size class. 
    void deallocateBatch(unsigned long numb, void *head, void * tail) {
      lock();
     
      //fprintf(stderr, "Donate batch to the pernode lsit.\n"); 
      _flist.PushRange(numb, head, tail);
       
      unlock();

      return;
   }

    inline size_t getSize() {
      return _size;
    }

 private:
    void lock() {
      pthread_spin_lock(&_lock);
    }

    void unlock() {
      pthread_spin_unlock(&_lock);
    }
};

#endif
