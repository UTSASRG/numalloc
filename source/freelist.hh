#ifndef __FREE_LIST_H__
#define __FREE_LIST_H__

#include "slist.hh"

/* We will use the freelist for both perthreadsizeclass and pernodesizeclass */
class FreeList {
private:
    void*    _list;
    uint32_t _length;     // Current length

public:
  void Init() {
    _list = NULL;
    _length = 0;
  }

  // Return current length of list
  size_t length() const {
    return _length;
  }

  // Is list empty?
  bool empty() const {
    return _list == NULL;
  }
  
  bool hasItems() const {
    return _list != NULL;
  }

  void Push(void* ptr) {
    SLL_Push(&_list, ptr);
    _length++;
  }

  void* Pop() {
    assert(_list != NULL);
    _length--;
    return SLL_Pop(&_list);
  }

  void PushRange(int N, void *start, void *end) {
    SLL_PushRange(&_list, start, end);
    _length += N;
  }

  void PopRange(int N, void **start, void **end) {
    SLL_PopRange(&_list, N, start, end);
    assert(_length >= N);
    _length -= N;
  }
};

#endif
