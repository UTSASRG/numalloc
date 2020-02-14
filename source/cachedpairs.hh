#ifndef __CACHED_PAIRS_H__
#define __CACHED_PAIRS_H__

class CachedPairs {
#define TOTAL_PAIRS 1024
    struct ptrPair {
      void * head; 
      void * tail;
    };
   
    struct ptrPair _pairs[TOTAL_PAIRS];
  
    // Whenever a point is coming in, it will be putted to the head, and then update the head pointer. 
    // Whevever we would like to move some objects, we will also  
    unsigned long _head; 
    unsigned long _tail;
    bool _rmFromHead; 

private:
    void initialize(bool moveFromHead = true) {
      _head = 0; 
      _tail = 0;
      _rmFromHead = moveFromHead; 
    }

    bool isEmpty() {
        return _head == _tail;
    }

    bool isFull() {
        return _tail = (_head - 1) % TOTAL_PAIRS;
    }

    void pushPair(void * head, void *tail) {
      _pairs[_head].head = head;
      _pairs[_head].tail = tail;
      _head = (_head + 1) % TOTAL_PAIRS;
      if(isFull()) {
        fprintf(stderr, "Now it is full\n");
        exit(0);
      }
    }

    // If pop is successful, then return true. Otherwise, return false
    bool popPair(void ** head, void ** tail) {
      if(isEmpty()) {
        return false;
      }

      struct ptrPair * pair; 
      if(_rmFromHead == true) {
        pair = &_pairs[_head-1];
        
        // Head will decrement 1;
        _head = (_head - 1) % TOTAL_PAIRS;
      }
      else {
        pair = &_pairs[_tail];

        // Tail will increment 1;
        _tail = (_tail+1)%TOTAL_PAIRS;
      }
       
      *head = pair->head;
      *tail = pair->tail;

      return true;
    }
};

#endif 
