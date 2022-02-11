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
    unsigned int      _batch; 
    unsigned int      _items; 
    void             * _listHead; 
    void             * _listTail;

 public:
    void initialize(unsigned int batch) {
      // fprintf(stderr, "pernodesizeclasslist.hh: initialize inside PerNodePerSizeClassList\n");
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
      // fprintf(stderr, "pernodesizeclasslist.hh: allocateAll inside PerNodePerSizeClassList with _items=%d\n", _items);
      int  numb;

      if(_items == 0) {
        return 0;
      }
      else {
        numb = _items;
        *head = _listHead;
        *tail = _listTail;
        _items = 0;

        _listHead = NULL;
        _listTail = NULL;
      }

      return numb;
    }
   
    // We will return true if the _items is equal to _batch
    // No need to acquire the lock, since it is under the protection of the big lock
    bool deallocate(void * ptr) {
      // fprintf(stderr, "pernodesizeclasslist.hh: deallocate inside PerNodePerSizeClassList\n");
      bool retValue = false; 
      
      SLL_Push(&_listHead, ptr);

      if(_items == 0) {
        _listTail = ptr;
      }

      // Update the number of items in the list.
      _items++;

      if(_items == _batch) {
        retValue = true;
      }

      return retValue;
    }

    // Deallocate multiple objects to the pernode's size class. 
    void deallocateBatch(unsigned long numb, void *head, void * tail) {
      // fprintf(stderr, "pernodesizeclasslist.hh: deallocate batch inside PerNodePerSizeClassList with head=%p and tail=%p\n", head, tail);
      if(_items == 0) {
        // Update the tail if no items there.
        _listTail = tail;
      }

      SLL_PushRange(&_listHead, head, tail);
      _items += numb;
      
      return;
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
    else if(classSize < 0x40000){
      batch = 2;
    }
    else{
      batch = 1;
    }

    // Initialize the lock
    pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
    // fprintf(stderr, "pernodesizeclasslist.hh: initialize _lists(PerNodePerSizeClassList)\n");
    for(int i = 0; i < PER_NODE_NUMB_SIZECLASS_LIST; i++) {
      _lists[i].initialize(batch);
    }
    return; 
  }

  // Return the number of items to be allocated. Head and tail is to get the pointers back
  int allocateBatch(void ** head, void ** tail) {
    int numb;
    PerNodePerSizeClassList * list = NULL;
    // fprintf(stderr, "pernodesizeclasslist.hh: allocateBatch from _lists[%d](PerNodePerSizeClassList)\n", _getIndex);
    pthread_spin_lock(&_lock);
    list =  &_lists[_getIndex];
    // Update the _getIndex if there are some objects there.  
    numb = list->allocateAll(head, tail);

    if(numb != 0) {
      _getIndex = (_getIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;
    }
    pthread_spin_unlock(&_lock);

    return numb; 
  }

  void deallocate(void * ptr) {
    PerNodePerSizeClassList * list = NULL;
    // fprintf(stderr, "pernodesizeclasslist.hh: deallocate %p from _lists[%d](PerNodePerSizeClassList)\n", ptr, _putIndex);
    // Acquire the global lock. 
    pthread_spin_lock(&_lock);

    list =  &_lists[_putIndex];

    // Update the _putIndex if freed objects are more than the threshold.
    if(list->deallocate(ptr) == true) {
      // fprintf(stderr, "pernodesizeclasslist.hh: deallocate = true, index ++\n");
      _putIndex = (_putIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;
    }

    pthread_spin_unlock(&_lock);

    return; 
  }

  // Deallocate multiple objects to the pernode's size class. 
  void deallocateBatch(unsigned long numb, void *head, void * tail) {
    PerNodePerSizeClassList * list = NULL;
    // fprintf(stderr, "pernodesizeclasslist.hh: deallocate batch from _lists[%d](PerNodePerSizeClassList)\n", _putIndex);
    // Always update the _putIndex
    pthread_spin_lock(&_lock);
    list =  &_lists[_putIndex];
    _putIndex = (_putIndex + 1)%PER_NODE_NUMB_SIZECLASS_LIST;

    list->deallocateBatch(numb, head, tail);

    pthread_spin_unlock(&_lock);
    return;
  }

};

#endif
