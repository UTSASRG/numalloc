#ifndef __PER_NODE_SIZE_CLASS_LIST_HH__
#define __PER_NODE_SIZE_CLASS_LIST_HH__

#include <pthread.h>
#include "mm.hh"
#include "persizeclasslist.hh"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class PerNodeSizeClassList {
  private: 
    pthread_spinlock_t _lock; 
    PerSizeClassList   _list;

 public:
    void initialize(unsigned long size) {
      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      // Initialize the freelist
      _list.initialize();
    }  

    // Allocate multiple objects in a batch from the pernode's size class 
    int allocateBatch(unsigned long requestNumb, void ** head, void ** tail) {
      int  numb;

      lock();
      
      numb = _list.popRange(requestNumb, head, tail);

      unlock();

      return numb;
    }
   
    void * allocate(void) {
      void * ptr = NULL;
     
      lock();
      ptr = _list.pop();
      unlock();

      return ptr;
    }


    void deallocate(void * ptr) {
      lock();
      _list.push(ptr);
      unlock();
    }

    // Deallocate multiple objects to the pernode's size class. 
    void deallocateBatch(unsigned long numb, void *head, void * tail) {
      lock();
     
      //fprintf(stderr, "Donate batch to the pernode lsit.\n"); 
      _list.pushRange(numb, head, tail);
       
      unlock();

      return;
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
