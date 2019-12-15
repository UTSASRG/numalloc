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
 * @file   xthread.cpp: thread-related function implementation.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 */

#include "xthread.hh"
#include "numaheap.hh"

char * getThreadBuffer() {
	return current->outputBuf;
}

int getThreadIndex(void) {
  return current->index;
}

int getNodeIndex(void) {
   return current->nindex;
}
	
int xthread::thread_create(pthread_t * thread, const pthread_attr_t * attr, threadFunction * fn, void * arg) {
  // Since we will create the children threads, set this flag to be true. 
  // This flag will change the allocation of the main thread, but should not affect
  // other threads.
  if(current->hasChildrenthreads == false)
    current->hasChildrenthreads = true;

  int tindex = allocThreadIndex();

	// Acquire the thread structure.
	thread_t* children = getThread(tindex);	
	children->index = tindex;
	children->startArg = arg;
	children->startRoutine = fn;

  // If there are two threads right now, we will stop the main heap phase.
  if(_alives == 2) {
    NumaHeap::getInstance().stopMainHeapPhase();
  }

  // Create the thread and bind the thread to the specific node (passed by _tattrs[nodeindex])
	int result = Real::pthread_create(thread, &_tattrs[children->nindex], xthread::startThread, (void *)children);

	if(result) {
		FATAL("thread_create failure");
	}

	// Setting up this in the creater thread so that
	// pthread_join can always find its pthread_t. 
	// Otherwise, it may fail if the child haven't set this value before the join
	children->thread = *thread;
	return result;
}

int xthread::thread_join(pthread_t thread, void ** retval) {
	int ret;

	if((ret = Real::pthread_join(thread, retval)) == 0) {
    int joinee = -1;

    // Clean up the thread entry so that the future threads 
    // can utilize the same entry, and also the 
    lock();
    joinee = findJoineeIndex(thread);
    _threads[joinee].available = true;
    _alives--;
	  unlock();
	}
  
  if(_alives == 1) {
    NumaHeap::getInstance().startMainHeapPhase();
  }
	return ret;
}

