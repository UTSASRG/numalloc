#ifndef __PER_THREAD_SIZE_CLASS_LIST_HH__
#define __PER_THREAD_SIZE_CLASS_LIST_HH__

#include <assert.h>
#include <stdio.h>
#include "slist.hh"

class PerThreadSizeClassList {
private:
  unsigned int _classIndex;
  unsigned long _classSize;  // How big for each object

  /* Learning from TcMalloc: all objects in the miniBag will be added to the freelist so that
   * we could warm up the corresponding cache.
   * Since we target at the hardware that has at most 16 threads in a node, we could 
   * get 16 objects at most from the pernode heap if the class size is less than 4K. 
   * Otherwise, we could at most get two objects from per-node heap if the size is larger than 4K. 
   * But both parameters will be adjustable from the compilation flag. Therefore, we will have the generality. 
   *
   * Since the modern Linux 4.19.0-8-amd64 are using the huge pages by default, we are trying to reduce the 
   * number of virtual memory that has been used. We can't just use 1M for one thread any more, since that will
   * waste just 1M memory. Therefore, we are designing such minibag machenism in order to reduce the memory consumption. 
   * Due to this reason, we don't maintain the bump pointer for per-thread's size class. Instead, only the per-node heap
   * will maintain the bump pointer.
   */ 
  // Let's always use the same batch number, which will be more easy to do.
  // Then we could think about how to improve it later. 
  // Freed objects will be donated when there are 2*_batch freed objects. 
  // Upon donation, we will change _items, _listTail, _listNthItem and the end of list (by pointing to NULL).
  unsigned int _batch; 

  /****
   There are multiple situations for donation and borrow. Then we should figure out how to design a good number for them.
   First, when per-thread freelist don't have any objects, then it will allocate _batch number of objects at a time. 
   Except the first one, it will only has at most _batch-1 objects. Then _listNthItem is not set up yet. 
   
   Then there are multiple situations
   (1) If the thread has some deallocations that are allocated from other threads in the same node, then the items may reach the _listNthItem. Then we will only need to set the pointer of _listNthItem by checking each deallocation. 
   (2) If the thread keeps using the objects without the donation, and then it will only invoke another borrow after all objects have been exhausted. Again, there is no need to use _listNthItem in the middle. 
    We don't have a case that the list have some objects, and then get a batch number of objects that making it a necessary to set _listNthItem. 
    
   If one borrow has a large number of objects that is larger than N items (_batch + 1), then we only need to traverse very few objects in the header, which is actually benefiting the performance by the cache warmup. 
 
   Donation only occurs when there are 2*batch+1 objects. Then the donation will utilize  
   *******/

  //unsigned int _lastBatch;

  /* Since everytime the perthreadSizeClass will have to check the per-node list, it doesn't need to maintain 
   * allocs before check any more. Basically, if the number of objects is larger than _miniBagObjects, then we will
   * contribute them to the per-node list. 
   */
  void * _listHead; 
  void * _listTail;
  void * _listNthItem; // N is the same as _batch;
  unsigned long _donationWatermark;

  unsigned long _items; 

public: 
  void initialize(int classSize, int classIndex) {
    _classSize = classSize;
    _classIndex = classIndex;

    if(classSize < PAGE_SIZE) {
      _batch = PAGE_SIZE/classSize;
    }
    else if(classSize < 0x40000){
      _batch = 4; 
    }
    else {
      _batch = 2;
    }

    _donationWatermark = _batch * 2;
    _listHead = NULL;
    _listTail = NULL;
    _listNthItem = NULL;
    _items = 0;
  }

  size_t getClassIndex(void) {
    return _classIndex;
  }

  size_t getClassSize(void) {
    return _classSize;
  }

  bool hasItems(void) {
    return _items > 0;
  }

  // Allocate an object. It will return NULL
  // if no objects left. 
  // No need to set _listNthItem during the allocation for the performance reason, since it will only 
  // consume freed objects and will not cause the donation itself. 
  // That is, it is possible to make _listNthItem point to 
  // an invalid object (not the nth object) during allocations. 
  // However, we will correct the pointer during deallocations, since _items (available objects) is the one 
  // to control the donation.
  void * allocate() {
    void * ptr = NULL;

    //fprintf(stderr, "allocate in perthreadsizeclass _items %d\n", _items);
    if(_items > 0) {
      if(_classSize == 0x800000)
      fprintf(stderr, "size %lx: last pointer %p\n", _classSize, _listHead);
      ptr = SLL_Pop(&_listHead);
      _items--;
    }

    return ptr;
  }

  // pushRangeList will be invoked when there is no freed objects in the list. 
  // Therefore, we should change all pointers if necessary.  
  void pushRangeList(int numb, void * head, void * tail) {
    assert(_items == 0); 

    if(numb > 0) {
    //  fprintf(stderr, "push numb %d head %p tail %p\n", numb, head, tail);
      SLL_PushRange(&_listHead, head, tail);
      _items = numb;
      _listTail = tail; 

      // Set the Nth item
      if(numb >= _batch + 1) {
        SLL_getItem(&_listHead, numb - _batch - 1, &_listNthItem);   
      }
      // Otherwise, there is no need to store _listNthItem.
      // Since the deallocation will always set this pointer correctly
    }
  }

  // pushRange will be invoked when there is no freed objects in the list. 
  // Therefore, we should change all pointers if necessary.  
  void pushRange(void * head, void * tail) {
    unsigned int numb; 

    numb = ((uintptr_t)tail - (uintptr_t)head)/_classSize;

    if(numb == 0) {
      return;
    }

//    if(_classSize == 0x80000)
//    fprintf(stderr, "push range head %p tail %p\n", head, tail);
    // Before calling push range, there are no items in the freelist.
    assert(_items == 0);

    char * iptr = (char *)head;

    void ** iter = (void **)head;
    unsigned int count = 0;

    if(numb >= _batch + 1) {
      // If number is larger than _batch+1, then we need to update the Nth item, since 
      // it is possible that we only have a lot of deallocations after now. 
      // Then the Nth item is not pointing to the correct position.
      while (count < numb) {
        // We will need _listNthItem points to the (n+1)th item from the tail.
        if(count == numb - _batch - 1) {
          _listNthItem = iptr;
        }

        iptr += _classSize;
        *iter = iptr;
        iter = reinterpret_cast<void**>(iptr);
        count++;
      }
    }
    else {
      while (count < numb) {
        iptr += _classSize;
        *iter = iptr;
        iter = reinterpret_cast<void**>(iptr);
        count++;
      }
    }
        
    tail = (void *)(iptr - _classSize);

    SLL_PushRange(&_listHead, head, tail);
    //if(_classSize == 0x80000)
    //fprintf(stderr, "size %ld: push range head %p tail %p to the list. Items %ld\n", _classSize, head, tail, _items);

    // Update other items
    _items = numb;
    _listTail = tail; 

    //fprintf(stderr, "pushRange _items %d\n", _items);
    return;
  }


  // Always donate _batch objects at a time. 
  int getDonateObjects(void ** head, void ** tail) {
    assert(_items == _donationWatermark);

    // If _listNthItem is not set, which is a bug since we will always set this
    // pointer during the deallocation.
    assert(_listNthItem != NULL);

    // Donate the older objects to the per-node heap, which helps the locality
    *head = *((void **)_listNthItem);
    *tail = _listTail;
    _items -= _batch; 
 
    // Update the _listTail
    _listTail = _listNthItem;
    *((void **)_listTail) = NULL; 
    // NO need to set _listNthItem, since the next deallocation
    // will set it anyway.
    //_listNthItem = NULL;

    return _batch;
  }

  // The function will return true, when the number freed objects reaches the _donationWatermark
  bool deallocate(void *ptr) {
    assert(ptr != NULL);

    SLL_Push(&_listHead, ptr);

    // Update the tail if necessary
    if(_items == 0) {
      _listTail = ptr;
    }

    // Update the NthItem if necessary.
    // This should be udpated before updating _items. 
    if(_items == _batch) {
      _listNthItem = ptr;
    }

    _items++;

    return (_items == _donationWatermark) ? true : false;
  }
};

#endif
