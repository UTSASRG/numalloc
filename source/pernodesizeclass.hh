#ifndef __PER_NODE_SIZE_CLASS_HH__
#define __PER_NODE_SIZE_CLASS_HH__

#include "mm.hh"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class PerNodeSizeClass {
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

    // Allocate multiple objects in a batch from the pernode's size class 
    int allocateBatch(unsigned long requestNum, void ** dest) {
      int  num = 0; 

      lock();
      
      if(_avails > 0) {
        num = requestNum >= _avails ? _avails : requestNum;

        int start = _next-num; 

        //assert(_next >= 1);
        if(_next == 0) {
          fprintf(stderr, "_next is 0!!!_size %ld _avails %ld requestNum %ld\n", _size, _avails, requestNum);
          abort();
        }

        // Allocate multiple mru entries to the dest starting with the given address
        for(int i = start; i < _next; i++) {
          *dest = _freeArray[i];
          assert(_freeArray[i] != NULL);

          dest++;
        }
        _next = start;
        _avails -= num;
      }
           
      unlock();

      return num;
    }

    // Deallocate multiple objects to the pernode's size class. 
    int deallocateBatch(unsigned long requestNum, void ** dest) {
      int  num = requestNum; 

    //  if(_size == 0x10000) 
    //      fprintf(stderr, "deallocate batch, requestNum %d. _next %d\n", num, _next);
      
      lock();
      
      // Check whether it is big enough to hold all objects
      if(requestNum + _avails > _max) {
        num = _max - _avails; 
      }
      
      // Allocate multiple mru entries to the dest starting with the given address
      for(int i = 0; i < num; i++) {
         _freeArray[_next] = *dest;
         dest++;
         _next++;
      }

      _avails += num;
      
      unlock();

      return num;
   }

    // Putting an entry to the pernode freelist of a size class
    void deallocate(void * ptr) {
      lock(); 

      _freeArray[_next] = ptr;
        
      _next++;
      _avails++;
      // Check whether _avails objects are more than enough
      if(_next == _max || _avails == _max) {
        fprintf(stderr, "The pernode freelist for size class %ld with %ld objects is too small.\n", _size, _max);
        assert(_avails == _max);
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
