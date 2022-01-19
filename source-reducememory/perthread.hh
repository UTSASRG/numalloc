#ifndef __PER_THREAD_HH__
#define __PER_THREAD_HH__

#include "perthreadheap.hh" 
#define LOG_SIZE 4096
typedef void * threadFunction(void*);

typedef struct thread {
  bool inited;

  // Whether the entry is available so that allocThreadIndex can use this one
  bool available;

  bool hasChildrenthreads;

  // Identifications
  pid_t tid;
  pthread_t thread;
  int index;  // Thread index
  int nindex; // Node index

  // Starting parameters
  threadFunction * startRoutine;
  void * startArg;

  // Printing buffer to avoid lock conflict. 
  // fprintf will use a shared buffer and can't be used in signal handler
  char * outputBuf;

  void* startFrame;

  // The freelists of this heap
  PerThreadHeap * ptheap;
} thread_t;

extern "C" {
  extern thread_local thread_t * current;

  extern int getThreadIndex(void);

  extern int getNodeIndex(void);
};

#endif
