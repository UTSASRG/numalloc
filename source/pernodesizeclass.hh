#ifndef __PER_NODE_SIZE_CLASS_HH__
#define __PER_NODE_SIZE_CLASS_HH__

#include "mm.hh"
#include "pthread.h"
#include "freememlist.hh"

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

    FreeMemList freeMemList;

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
        FreeMemNode *head = freeMemList.getHead();
        if (head->getNext() != NULL) {
            ptr = (void *) head->getNext();
            freeMemList.remove(head->getNext());
        }
      unlock();
      return ptr;
    }

    // Allocate multiple objects in a batch from the pernode's size class 
    int allocateBatch(unsigned long requestNum, void ** dest) {
      int  num = 0; 

      lock();
        FreeMemNode *head = freeMemList.getHead();
        for(; num < requestNum; num++) {
            head = head->getNext();
            if (head == NULL) {
                break;
            }
            *dest = (void *) head;
            dest++;
            freeMemList.remove(head);
        }
           
      unlock();

      return num;
    }


    // Only invoked by the main thread.
    void insertObjectsToFreeList(char * start, char * stop) {
      char * ptr = start; 

      while(ptr < stop) {
          freeMemList.insertIntoHead(ptr, _size);
        ptr += _size;
      }
    }

    // Deallocate multiple objects to the pernode's size class. 
    int deallocateBatch(unsigned long requestNum, void ** dest) {
      int  num = requestNum; 

    //  if(_size == 0x10000) 
    //      fprintf(stderr, "deallocate batch, requestNum %d. _next %d\n", num, _next);
      
      lock();
      
      // Check whether it is big enough to hold all objects
      
      // Allocate multiple mru entries to the dest starting with the given address
      for(int i = 0; i < num; i++) {
          freeMemList.insertIntoHead(dest[i],_size);
      }

      unlock();

      return num;
   }

    inline size_t getSize() {
      return _size;
    }

    // Putting an entry to the pernode freelist of a size class
    void deallocate(void * ptr) {
      lock();

      freeMemList.insertIntoHead(ptr, _size);
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
