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
 * @file   liblight.cpp: main file, includes memory interception functions.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 * @author Sam Silvestro <sam.silvestro@utsa.edu>
 */
#include <dlfcn.h>
#include <sys/mman.h>
#include <numa.h>
#include <string.h>
#include "real.hh"
#include "xthread.hh"
#include "numaheap.hh"
#include "mm.hh"
#include "xdefines.hh"
#include "perthread.hh"

// Define a thread local variable--current. Based on the evaluation, 
// http://david-grs.github.io/tls_performance_overhead_cost_linux/, 
// TLS is very efficient. I also have verified this by myself. 
thread_local thread_t * current;

char localBuffer[4096];
char * localPtr = NULL;
char * localPtrEnd;

typedef int (*main_fn_t)(int, char**, char**);

extern "C" int __libc_start_main(main_fn_t, int, char**, void (*)(), void (*)(), void (*)(), void*) __attribute__((weak, alias("light_libc_start_main")));

extern "C" int light_libc_start_main(main_fn_t main_fn, int argc, char** argv, void (*init)(), void (*fini)(), void (*rtld_fini)(), void* stack_end) {
    // real run
  	auto real_libc_start_main = (decltype(__libc_start_main)*)dlsym(RTLD_NEXT, "__libc_start_main");
  	return real_libc_start_main(main_fn, argc, argv, init, fini, rtld_fini, stack_end);
}

// Variables used by our pre-init private allocator
typedef enum {
	E_HEAP_INIT_NOT = 0,
	E_HEAP_INIT_WORKING,
	E_HEAP_INIT_DONE,
} eHeapInitStatus;

eHeapInitStatus heapInitStatus = E_HEAP_INIT_NOT;

extern "C" {
	void   xxfree(void *);
	void * xxmalloc(size_t);
	void * xxcalloc(size_t, size_t);
	void * xxrealloc(void *, size_t);

    void * xxvalloc(size_t);
    void * xxaligned_alloc(size_t, size_t);
    void * xxmemalign(size_t, size_t);
    void * xxpvalloc(size_t);
    void * xxalloca(size_t);
    int    xxposix_memalign(void **, size_t, size_t);

	// Function aliases
	void   free(void *) __attribute__ ((weak, alias("xxfree")));
	void * malloc(size_t) __attribute__ ((weak, alias("xxmalloc")));
	void * calloc(size_t, size_t) __attribute__ ((weak, alias("xxcalloc")));
	void * realloc(void *, size_t) __attribute__ ((weak, alias("xxrealloc")));

    void * valloc(size_t) __attribute__ ((weak, alias("xxvalloc")));
    void * aligned_alloc(size_t, size_t) __attribute__ ((weak,
                                        alias("xxaligned_alloc")));
    void * memalign(size_t, size_t) __attribute__ ((weak, alias("xxmemalign")));
    void * pvalloc(size_t) __attribute__ ((weak, alias("xxpvalloc")));
    void * alloca(size_t) __attribute__ ((weak, alias("xxalloca")));
    int posix_memalign(void **, size_t, size_t) __attribute__ ((weak,
                                        alias("xxposix_memalign")));
}

__attribute__((constructor)) void initializer() {
  heapinitialize();
}


__attribute__((destructor)) void finalizer() {

}

/* Adding the following data structure to 
 * collect numa topology, such as 
 * the number of nodes, and the latency of accessing
 * different nodes.
 * The later one will help the performance by deciding the 
 * collocation in a complicated NUMA environment. 
 * But the first version will not implement it. 
 * Ideally, we could utilize the allocation and deallocation pattern
 * to understand the difference. However, we can't solve this issue
 * when multiple types of threads are accessing the same data, such as 
 * A --> B --> C. How to detect B is inside. 
 */


void heapinitialize() {
	if(heapInitStatus == E_HEAP_INIT_WORKING) {
    
    // Including the number of nodes, and the size of freed memory if possible
    NumaHeap::getInstance().initialize();
    
    // Thread initialization, which can be occurred before or after numap heap initialization
		xthread::getInstance().initialize();

    fprintf(stderr, "in the end of heapinitialize()\n");
    heapInitStatus = E_HEAP_INIT_DONE;
	} 
  else {
			while(heapInitStatus != E_HEAP_INIT_DONE);
	}
}

 void * xxmalloc(size_t size) {

    // If this is the first allocation before the initialization, 
    // then we will try to use a local buffer, since Real::initializer() 
    // will invoke malloc as well.
    if(localPtr == NULL) {
      localPtr = localBuffer;
      localPtrEnd = &localBuffer[4096];

      Real::initializer();
      heapInitStatus = E_HEAP_INIT_WORKING;
    }

    void * ptr = NULL;
    switch(heapInitStatus) {
    case E_HEAP_INIT_NOT:
      ptr = localPtr; 
      localPtr += size;
      if(localPtr > localPtrEnd) {
        fprintf(stderr, "Making the temporary buffer larger\n");
        abort();
      }
    break;

    case E_HEAP_INIT_WORKING:
      ptr = Real::malloc(size);
      break; 

    case E_HEAP_INIT_DONE: 
      ptr = NumaHeap::getInstance().allocate(size); 
      //fprintf(stderr, "malloc size %ld ptr %p\n", size, ptr);
      break;
  
    }
//     fprintf(stderr, "xxmalloc size %ld ptr %p thread-id %d\n", size, ptr);
//     void *tem = (void *) ((long) ptr + size);
//     fprintf(stderr, "xxmalloc ptr check %p %ld %c\n", ptr, size , (char *) tem);
    return ptr;
}

 void xxfree(void * ptr) {
		if(ptr == NULL) {
      return;
		}
//     fprintf(stderr, "xxfree ptr %p thread-id %d\n", ptr, current->index);
   if(heapInitStatus == E_HEAP_INIT_WORKING) { 
     Real::free(ptr);
   }
   else { 
    // fprintf(stderr, "free ptr %p\n", ptr);
     // Perform the free operation
     NumaHeap::getInstance().deallocate(ptr); 
     //fprintf(stderr, "free ptr %p done\n", ptr);
   }
}

void * xxcalloc(size_t nelem, size_t elsize) {
	void * ptr = NULL;
	ptr = malloc(nelem * elsize);
	if(ptr != NULL) {
		memset(ptr, 0, nelem * elsize);
	}
	return ptr;
}

void * xxrealloc(void * ptr, size_t sz) {
    if(heapInitStatus == E_HEAP_INIT_WORKING) { 
      return Real::realloc(ptr, sz);
    }

		// If the pointer is null, call is equivalent to malloc(sz).
		if(ptr == NULL) {
				return xxmalloc(sz);
		}

		// If the pointer is non-null and size is zero, call is equivalent
		// to free(ptr).
		if(sz == 0) {
				xxfree(ptr);
				return NULL;
		}

    // If the object is unknown to us, return NULL to indicate error.
    size_t oldSize = NumaHeap::getInstance().getSize(ptr);

		void * newObject = xxmalloc(sz);
		memcpy(newObject, ptr, oldSize);
		xxfree(ptr);
		return newObject;
}


void * xxalloca(size_t size) {
    PRERR("%s CALLED", __FUNCTION__);
    return NULL;
}

void * xxvalloc(size_t size) {
    PRERR("%s CALLED", __FUNCTION__);
    return NULL;
}

int xxposix_memalign(void **memptr, size_t alignment, size_t size) {
		void * alignedObject = xxmemalign(alignment, size);
		*memptr = alignedObject;
		return 0;
}

void * xxaligned_alloc(size_t alignment, size_t size) {
    PRERR("%s CALLED", __FUNCTION__);
    return NULL;
}

void * xxmemalign(size_t alignment, size_t size) {
  if(size == 0) {
		return NULL;
	}

	size_t allocObjectSize = alignment + size;
	void * object = malloc(allocObjectSize);
	unsigned long residualBytes = (unsigned long)object % alignment;
	void * alignedObject = (void *)((char *)object + residualBytes);

	return alignedObject;
}

void * xxpvalloc(size_t size) {
  PRERR("%s CALLED", __FUNCTION__);
  return NULL;
}

// Intercept thread creation
int pthread_create(pthread_t * tid, const pthread_attr_t * attr,
  void *(*start_routine)(void *), void * arg) {
	if(heapInitStatus != E_HEAP_INIT_DONE) {
			heapinitialize();
	}
  return xthread::getInstance().thread_create(tid, attr, start_routine, arg);
}
int pthread_join(pthread_t tid, void** retval) {
	return xthread::getInstance().thread_join(tid, retval);
}
