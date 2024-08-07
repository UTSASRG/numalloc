/*
 * FreeGuard: A Faster Secure Heap Allocator
 * Copyright (C) 2017 Sam Silvestro, Hongyu Liu, Corey Crosser, 
 *                    Zhiqiang Lin, and Tongping Liu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * 
 * @file   xthread.hh: thread-related functions implementation.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 * @author Sam Silvestro <sam.silvestro@utsa.edu>
 */
#ifndef __XTHREAD_HH__
#define __XTHREAD_HH__

#include <unistd.h>
#include <new>
#include <sys/syscall.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include "log.hh"
#include "perthread.hh"
#include "real.hh"
#include "xdefines.hh"
#include "mm.hh"

class xthread {
#define MAX_ALIVE_THREADS 1024

	public:
  static inline xthread& getInstance() {
    static char buf[sizeof(xthread)];
    static xthread* xthreadObject = new (buf) xthread();
    return *xthreadObject;
  }

	void initialize() {
		  _alives = 0;
      _next = 0;
      _max = MAX_ALIVE_THREADS - 1;

	    // Initialize the spin_lock
	    pthread_spin_init(&_spin_lock, PTHREAD_PROCESS_PRIVATE);


      // Shared the threads information.
      memset(&_threads, 0, sizeof(_threads));

      _nodeIndex = getRealNodeIndex();
      _nodeMax = NUMA_NODES;

      fprintf(stderr, "Main thread is at node %d\n", _nodeIndex);
      int totalCpus = get_nprocs();

      cpu_set_t *cpusetp;
      size_t size;

      cpusetp = CPU_ALLOC(totalCpus);
      if (cpusetp == NULL) {
        perror("CPU_ALLOC");
        exit(EXIT_FAILURE);
      }

      size = CPU_ALLOC_SIZE(totalCpus);

      // Making sure that the configuration is the same as the results of lscpu
      // Initialize the CPU set!!
 #if 0
      for (int i = 0; i < NUMA_NODES; i++) {
        struct logic_cpu cpus = Numa::get_logic_cpu(i);
            CPU_ZERO_S(size, cpusetp);
            for (int j = 0; j < cpus.size; j++) {
                fprintf(stderr, "Node %d: setcpu %d\n", i, cpus.cpus[j]);
                CPU_SET_S(cpus.cpus[j], size, cpusetp);
            }
            pthread_attr_setaffinity_np(&_tattrs[i], size, cpusetp);
      }
 #else
      struct bitmask *bitmask = numa_bitmask_alloc(totalCpus);
      // Making sure that the configuration is the same as the results of lscpu
      // Initialize the CPU set!!
      for(int i = 0; i < NUMA_NODES; i++) {
        numa_node_to_cpus(i, bitmask);

        CPU_ZERO_S(size, cpusetp);
        for (int cpu = 0; cpu < totalCpus; cpu++) {
//          fprintf(stderr, "Node %d: setcpu %d\n", i, cpu);
          if(numa_bitmask_isbitset(bitmask, cpu)) {
      //      fprintf(stderr, "Node %d: setcpu %d\n", i, cpu);
            CPU_SET_S(cpu, size, cpusetp);
          }
        }
        pthread_attr_setaffinity_np(&_tattrs[i], size, cpusetp);
      }

 #endif
        int max_use_node = _nodeMax;
        int start_thread_node = _nodeIndex;
        int node_foot_print=0;
#ifdef MAX_USE_NODE
        max_use_node = MAX_USE_NODE;
#endif
        fprintf(stderr, "totally use %d node \n", max_use_node);

        // Initialize all threads's structure at once
      thread_t * thread;
      for(int i = 0; i < MAX_ALIVE_THREADS; i++) {
        thread = &_threads[i];

			  // Those information that are only initialized once.
     	  thread->available = true;
        thread->hasChildrenthreads = false;
        thread->index = i;

        // We will assign the thread to each node during the initialization.
        // There is no need to re-assign later 
        thread->nindex = _nodeIndex;
        thread->inited = false;

        // Always makes the first thread to be the same node as the main thread
        if(i != 0) {
          _nodeIndex++;
          if(_nodeIndex == _nodeMax) {
            _nodeIndex = 0;
          }
            node_foot_print++;
            if(node_foot_print>=max_use_node){
                _nodeIndex=start_thread_node;
                node_foot_print=0;
            }
        }
//fprintf(stderr, "initialize i %d _nodeIndex %d\n", i, _nodeIndex);
      }

		  // Now we will intialize the initial thread
		  initializeInitialThread();
  }


  void finalize() {}

  
	void initializeInitialThread(void) {
		int tindex = allocThreadIndex();
   	assert(tindex == 0);

    //getThread(tindex);
    initializeCurrentThread(getThread(tindex));
    
    fprintf(stderr, "Initialization, Main thread is at node %d\n", current->nindex);
		// Adding the thread's pthreadt.
		current->thread = pthread_self();

    // Binding the current thread to the current node.    
    if(numa_run_on_node(current->nindex) != 0) {
      fprintf(stderr, "Binding initial task to node %d failed with error %s\n", current->nindex, strerror(errno));
      exit(-1);
    }
  }

	// This function is only called in the current thread before the real thread function 
	void initializeCurrentThread(thread_t * thread) {
    current = thread;
		current->tid = syscall(__NR_gettid);
    int realNodeIndex = getRealNodeIndex();
  // fprintf(stderr, "Thread index %d nodeindex %d actual node %d\n", current->index, current->nindex, getRealNodeIndex());
    if(current->nindex != realNodeIndex) {
      fprintf(stderr, "Thread index %d nodeindex %d actual node %d\n", current->index, current->nindex, getRealNodeIndex());
      current->nindex = realNodeIndex;
    }

    // Initialize the current heap
    if(current->inited == false) 
      initCurrentHeap(current->index, current->nindex); 

    //current->startFrame = (char *)__builtin_frame_address(0);
  }

	/// @ internal function: allocation a thread index when spawning.
  int allocThreadIndex() {
    int index = -1;
    thread_t* thread;

    lock();

    if(_alives == _next) {
      // All entries before _next has been utilized -- common case
			index = _next;
      _next++;
      thread = getThread(index);
			thread->available = false;
		} 
    else {
      // Some threads have exited before. 
			for(int i = 0; i <= _next; i++) {
      	thread = getThread(i);
      	if(thread->available) {
        	thread->available = false;
					index = i;
					break;
				}
			}
		}
    // Now we will create one more thread
    _alives++;

   if(_alives >= _max) {
    fprintf(stderr, "alives is %d _max is %d\n", _alives, _max); 
    assert(_alives < _max);
   }

		unlock();
  //  fprintf(stderr, "allocate THREAD %d\n", index);
    return index;
  }

  inline thread_t* getThread(int index) { return &_threads[index]; }


  static void * startThread(void * arg) {
    thread_t * current = (thread_t *) arg;

    xthread::getInstance().initializeCurrentThread(current);

    //fprintf(stderr, "staring thread %d\n", current->index);
    return current->startRoutine(current->startArg);
  }

  int findJoineeIndex(pthread_t thread) {
    int index = -1; 
    for(int i = 0; i < _max; i++) {
      // Exit if we find the entry in _threads array
      if(_threads[i].thread == thread) {
        index = i; 
        break;
      }
    }

    if(index == -1) {
      fprintf(stderr, "Can't find the thread?? \n");
      abort();      
    }
    return index; 
  }

	int thread_create(pthread_t * thread, const pthread_attr_t * attr, threadFunction * fn, void * arg);
	int thread_join(pthread_t thread, void ** retval);

	void lock() {
		spin_lock();
	}
	
	void unlock() {
		spin_unlock();
	}
	
private:

  void initCurrentHeap(int tindex, int nindex) {
    // Allocate a big chunk of memory from the local node
    size_t size = LOG_SIZE + sizeof(PerThreadHeap); 
    size = alignup(size, PAGE_SIZE);

    //fprintf(stderr, "InitCurrentHeap with Size %lx", size);
    char * ptr = (char *)MM::mmapFromNode(size, current->nindex);
    if(ptr == 0) { 
      fprintf(stderr, "InitCurrentHeap thread %d: with ptr %p\n", tindex, ptr);
      assert(ptr != 0);
    }

    current->outputBuf = (char *)ptr;
    ptr += LOG_SIZE;

    current->ptheap = (PerThreadHeap *)ptr;

    // Initialize the current thread's heap
    current->ptheap->initialize(tindex, nindex);
    current->inited = true;
  }

	inline void spin_lock() {
		pthread_spin_lock(&_spin_lock);
	} 

	inline void spin_unlock() { 
		pthread_spin_unlock(&_spin_lock); 
	}

	pthread_spinlock_t _spin_lock;

  // The next available thread index 
  int _next;
  int _max;
	int _alives;

  // Bind different threads to different nodes in a circular way
  int  _nodeMax; 
  int  _nodeIndex;
  pthread_attr_t _tattrs[NUMA_NODES];
  thread_t _threads[MAX_ALIVE_THREADS];
};
#endif
