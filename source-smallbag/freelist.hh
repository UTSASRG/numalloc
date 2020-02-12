#ifndef __FREE_LIST_H__
#define __FREE_LIST_H__

#include "slist.hh"

/* We will use the freelist for both perthreadsizeclass and pernodesizeclass */
class FreeList {
protected:
    void*    _list;
    void* _tail;
    uint32_t _length;     // Current length

protected:
    void _PushRangeFromBack(int N, void *start, void *end) {
        if (_length == 0) {
            this->_list = start;
            this->_tail = end;
            this->_length = N;
            return;
        }
        *(reinterpret_cast<void **>(this->_tail)) = start;
        this->_tail = end;
        this->_length += N;
    }

public:
  void Init() {
    _list = NULL;
    _tail = NULL;
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
    return _length > 0;
  }

  void Push(void* ptr) {
      SLL_Push(&_list, ptr);
      if (_length == 0) {
          _tail = ptr;
      }
      _length++;
  }

  void* Pop() {
    assert(_list != NULL);
    _length--;
      if (_length == 0) {
          _tail = NULL;
      }
    return SLL_Pop(&_list);
  }

  void PushRange(int N, void *start, void *end) {
    SLL_PushRange(&_list, start, end);
    if(_length==0){
        _tail=end;
    }
    _length += N;
  }

  void PopRange(int N, void **start, void **end) {
    SLL_PopRange(&_list, N, start, end);
    assert(_length >= N);
    _length -= N;
    if(_length==0){
        _tail=NULL;
    }
  }
};

#endif
