#ifndef __PER_SIZE_CLASS_LIST_HH__
#define __PER_SIZE_CLASS_LIST_HH__

#include "xdefines.hh"
#include "slist.hh"

class PerSizeClassList {
private:
  unsigned long _items;  
  void *        _list;

public:
  void initialize(void) {
    _list = NULL;
    _items = 0;
  }

  int popRange(unsigned long requestNumb, void ** head, void ** tail) {
    int  numb = _items < requestNumb ? _items : requestNumb;
    
    // Pop a range of items if its larger than 0
    if(numb > 0) {
      // Pop the specified number of entries;
      SLL_PopRange(&_list, numb, head, tail);
      _items -= numb;
    }

    return numb;
  }

  void * pop(void) {
    void * ptr = NULL;

    if(_items > 0) {
      ptr = SLL_Pop(&_list);
      _items--;
    }

    return ptr;
  }

  void push(void * ptr) {
    assert(ptr != NULL);
    SLL_Push(&_list, ptr);
    _items++;
  }

  void pushRange(unsigned long numb, void *head, void * tail) {
    if(numb == 0) {
      return;
    }

    SLL_PushRange(&_list, head, tail);
    _items += numb;
  }

  bool hasItems(void) {
    return _items > 0 ? true : false;
  }

  int length(void) {
    return _items;
  }
};

#endif
