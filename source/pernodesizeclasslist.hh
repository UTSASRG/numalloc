#ifndef __PER_NODE_SIZE_CLASS_LIST_HH__
#define __PER_NODE_SIZE_CLASS_LIST_HH__

#include <pthread.h>
#include "xdefines.hh"
#include "mm.hh"
#include "slist.hh"

// For small objects, each size class will maintain
// one bumppointer and one freelist
class PerNodeSizeClassList {
  class PerNodePerSizeClassList {
    pthread_spinlock_t _lock;
    unsigned int      _batch; 
    unsigned int      _items; 
    void             * _listHead; 
    void             * _listTail;

 public:
    void initialize(unsigned int batch) {
      // Initialize the lock
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);

      // Initialize the freelist
      _items = 0;
      _listHead = NULL;
      _listTail = NULL;
      _batch = batch; 
    }  

    bool hasItems() {
      return (_items != 0) ? true : false;
    }


    // Allocate multiple objects in a batch from the pernode's size class 
    int allocateAll(void ** head, void ** tail) {
      int  numb;

      if(_items == 0) {
        return 0;
      }
      else {
        lock();
        numb = _items;
        *head = _listHead;
        *tail = _listTail;
        _items = 0;

        _listHead = NULL;
        _listTail = NULL;
        unlock();
      }

      return numb;
    }
   
    // We will return true if the _items is equal to _batch
    // No need to acquire the lock, since it is under the protection of the big lock
    bool deallocate(void * ptr) {
      bool retValue = false; 
      //lock();
      SLL_Push(&_listHead, ptr);

      if(_listTail == NULL) {
        _listTail = ptr;
      }

      // Update the number of items in the list.
      _items++;

      if(_items == _batch) {
        retValue = true;
      }

      //unlock();
      return retValue;
    }

    // Deallocate multiple objects to the pernode's size class. 
    void deallocateBatch(unsigned long numb, void *head, void * tail) {
      lock();
     
      if(_items == 0) {
        // Update the tail if no items there.
        _listTail = tail;
      }

      SLL_PushRange(&_listHead, head, tail);
      _items += numb;
      
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

private:
  class PerNodePerSizeClassList _lists[PER_NODE_NUMB_SIZECLASS_LIST];
  unsigned int _putIndex;
  unsigned int _getIndex; 
  size_t _classSize; 
  // Protect the updates of the index
  pthread_spinlock_t _lock; 
  unsigned long _items; 

public:
  void initialize(unsigned long classSize) {
    int batch;
    _putIndex = 0; 
    _getIndex = 0; 
    _classSize = classSize; 
    _items = 0;

    // Use the same setting as perthreasizeclasslist.hh
    if(classSize < PAGE_SIZE) {
      batch = PAGE_SIZE/classSize;
    }
    else {
      batch = 2;
    }

    // Initialize the lock
    pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
    for(int i = 0; i < PER_NODE_NUMB_SIZECLASS_LIST; i++) {
      _lists[i].initialize(batch);
    }
    return; 
  }

#if 0
  // In fact, this functions should not be invoked.
  void * allocate(void) {
    // Always allocate one object from the current index. 
    PerNodePerSizeClassList * list = getAllocOneList(); 

    return list->allocate();
  } 
#endif

  // Return the number of items to be allocated. Head and tail is to get the pointers back
  int allocateBatch(void ** head, void ** tail) {
    int numb;
    PerNodePerSizeClassList * list = NULL;

    pthread_spin_lock(&_lock);
    list =  &_lists[_getIndex];
    // Update the _getIndex if there are some objects there.  
    if(list->hasItems()) {
      _getIndex = (_getIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;
    }
    pthread_spin_unlock(&_lock);

    return list->allocateAll(head, tail);
  }

  void deallocate(void * ptr) {
    PerNodePerSizeClassList * list = NULL;
    
    // Acquire the global lock. 
    pthread_spin_lock(&_lock);

    list =  &_lists[_putIndex];
    // Update the _putIndex if freed objects are more than the threshold.
    if(list->deallocate(ptr) == true) {
      _putIndex = (_putIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;
    }
    pthread_spin_unlock(&_lock);

    return; 
  }

  // Deallocate multiple objects to the pernode's size class. 
  void deallocateBatch(unsigned long numb, void *head, void * tail) {
    PerNodePerSizeClassList * list = NULL;

    // Always update the _putIndex
    pthread_spin_lock(&_lock);
    list =  &_lists[_putIndex];
    _putIndex = (_putIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;
    pthread_spin_unlock(&_lock);

    list->deallocateBatch(numb, head, tail);

    return;
  }

};

#endif
