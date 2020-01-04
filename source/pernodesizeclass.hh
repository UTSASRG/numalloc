#ifndef __PER_NODE_SIZE_CLASS_HH__
#define __PER_NODE_SIZE_CLASS_HH__

#include "mm.hh"
#include "freelist.hh"
#include "perthread.hh"

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

      // Initialize the freelist
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
      
      fprintf(stderr, "size %ld: Move batch from the pernode list.\n", _size); 
      // Pop the specified number of entries;
      if(numb > 0)  
      _flist.PopRange(numb, head, tail); 
      fprintf(stderr, "size %ld: Move batch %d from the pernode list.\n", _size, numb); 
           
      unlock();

      return numb;
    }
    
    void * allocate(void) {
      void * ptr = NULL;
     
      lock();
      if(_flist.hasItems()) 
        ptr = _flist.Pop();
      unlock();

      return ptr;
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

    inline bool hasItems() {
      return _flist.hasItems();
    }

    inline size_t getSizeClass() {
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
