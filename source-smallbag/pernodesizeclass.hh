#ifndef __PER_NODE_SIZE_CLASS_HH__
#define __PER_NODE_SIZE_CLASS_HH__

#include "mm.hh"
#include "freelist.hh"
#include "perthread.hh"
#include "jumpfreelist.hh"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class PerNodeSizeClass {
  private: 
    pthread_spinlock_t _lock; 
    unsigned long _size; // Size of this size class
    JumpFreeList _flist;

 public:
    void initialize(unsigned long size) {
      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      _size = size;

      // Initialize the freelist
      _flist.Init(FREE_LIST_INTERVAL(size));
    }  

    // Allocate multiple objects in a batch from the pernode's size class 
    void allocateBatch(unsigned long requestNumb, JumpFreeList* list) {
      int  numb = 0;

      if(_flist.length()<=0){
          return;
      }

      lock();
      
      numb = _flist.length(); 
      if(numb > requestNumb) {
        numb = requestNumb;
      }
      
      //#fprintf(stderr, "size %ld: Move batch from the pernode list.\n", _size); 
      // Pop the specified number of entries;
      if(numb > 0)
      _flist.PopRange(numb, list);
      //fprintf(stderr, "size %ld: Move batch %d from the pernode list.\n", _size, numb); 
           
      unlock();

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
    void deallocateBatch(JumpFreeList* list) {
      lock();
     
      //fprintf(stderr, "Donate batch to the pernode lsit.\n"); 
      _flist.PushRange(list);
       
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
