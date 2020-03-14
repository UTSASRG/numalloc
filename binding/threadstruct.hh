#ifndef __PER_THREAD_HH__
#define __PER_THREAD_HH__

typedef void * threadFunction(void*);

typedef struct thread {
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
} thread_t;

extern "C" {
  extern thread_local thread_t * current;
  extern int getThreadIndex(void);
  extern int getNodeIndex(void);
};

#endif
