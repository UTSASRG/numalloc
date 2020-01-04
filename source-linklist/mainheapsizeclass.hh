#ifndef __MAIN_HEAP_SIZE_CLASS_HH__
#define __MAIN_HEAP_SIZE_CLASS_HH__

#include "mm.hh"
#include "pthread.h"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class MainHeapSizeClass {
  private: 
    pthread_spinlock_t _lock; 
    unsigned long _max;     // How many entries in the array 
    unsigned long _next;    // Point to the next available slot 
    unsigned long _avails;  // Available objects in the array. 
    unsigned long _size; // Size of this size class

    // Freelist will be tracked with one circular array.
    void ** _freeArray;

 public:
    void initialize(unsigned long size, unsigned long numObjects, void ** ptr) {
      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      _next = 0;
      _avails = 0;
      _max = numObjects - 1;
      _size = size;

      // Allocate a chunk of memory for the array. 
      //size_t size = numObjects * sizeof(void *);

      // Map a chunk of memory in the local node
      _freeArray = (void **)ptr;
    }  

    // Note: this function is only invoked by the main thread's heap. 
    // Therefore, there is no need to utilize the lock protect, which will be only 
    // invoked when there is just one thread.
    void * allocate( ) {
      void * ptr = NULL;
      lock();

      if(_avails > 0) {
        _next--;
        _avails--;
        ptr = _freeArray[_next];
      }

      unlock();
      return ptr;
    }


    // Only invoked by the main thread.
    void insertObjectsToFreeList(char * start, char * stop) {
      char * ptr = start; 

      while(ptr < stop) {
        _freeArray[_next] = ptr;
        ptr += _size; 
        _avails++;
        _next++;
      }

      return;
    }

    inline size_t getSize() {
      return _size;
    }

    // Putting an entry to the pernode freelist of a size class
    void deallocate(void * ptr) {
      lock(); 

      if(_next < _max) {
        _freeArray[_next] = ptr;
        
        _next++;
        _avails++;
      
        // Check whether _avails objects are more than enough
        if(_next == _max || _avails == _max) {
          fprintf(stderr, "The pernode freelist for size class %ld with %ld objects is too small.\n", _size, _max);
          assert(_avails == _max);
        }
      }
      unlock();
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
