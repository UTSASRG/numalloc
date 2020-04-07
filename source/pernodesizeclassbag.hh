#ifndef __PER_NODE_SIZE_CLASS_BAG__
#define __PER_NODE_SIZE_CLASS_BAG__

#include <pthread.h>
#include "xdefines.hh"

// Each node's small size class will have a bag to hold objects that 
// are never allocated. 
class PerNodeSizeClassBag {
  public:
    char        * _bp;
    char        * _bpEnd;
    unsigned int  _bags; // How many bags are allocated in this size class. 
    unsigned int  _classSize;
    unsigned int  _nodeIndex;
    pthread_spinlock_t _lock;
    unsigned int _batchSize;

  public:
    void initialize(unsigned int nodeindex, unsigned int classSize) {
      _bp = NULL;
      _bpEnd = NULL;
      _nodeIndex = nodeindex;
      _classSize = classSize;

      if(classSize < PAGE_SIZE) {
        int objects = PAGE_SIZE/classSize;
        _batchSize = objects*classSize;
      }
      else {
        _batchSize = classSize * 4;
      }

      _bags = 0;
      pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
    }

    // This function should be always successful. Therefore, there is no
    // need for the return values. 
    void allocate(void ** head, void ** tail) {
      pthread_spin_lock(&_lock);
      if(_bp == _bpEnd) {
        // If the allocation is not successful, then allocateOneBag()
        // should handle the failure.
        _bp = (char *)allocateOneBagFromNode();
        _bpEnd = _bp + SIZE_ONE_BAG;
        _bags++;  
      }

      if((_bp + 2*_batchSize) < _bpEnd) {
        *head = _bp;

        // Update the _bp pointer after the allocation
        _bp += _batchSize;
        *tail = _bp;
      }
      else {
        // In order to avoid the unalignment issue that may have less than one object to return in the last 
        // allocation, we will combine the last two together in one allocation.
        *head = _bp;
        *tail = _bpEnd;

        // Set _bp to _bpEnd so that it will allocate a new block if more allocations
        _bp = _bpEnd;
      }

      pthread_spin_unlock(&_lock);

      return;
    }

    // Implementation is in the cpp file.
    void *allocateOneBagFromNode();  
};

#endif
