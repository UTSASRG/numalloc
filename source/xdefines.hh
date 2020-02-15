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
 * @file   xdefines.hh: global constants, enums, definitions, and more.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 * @author Sam Silvestro <sam.silvestro@utsa.edu>
 */
#ifndef __XDEFINES_HH__
#define __XDEFINES_HH__

#include <stddef.h>
#include <stdint.h>
#include <ucontext.h>
#include <assert.h>
#include <string.h>

/*
 * @file   xdefines.h
 */


extern "C" {

extern char * getThreadBuffer();
extern int outputfd;
extern unsigned int mainNodeIndex;
extern struct bitmask memBitmasks[NUMA_NODES+1];
#define PTHREADEXIT_CODE 2230

inline const char * boolToStr(bool value) {
    return (value ? "true" : "false");
}

#ifdef LOGTOFILE
inline int getOutputFD() {
  return outputfd;
}
#endif

inline size_t alignup(size_t size, size_t alignto) {
  return (size % alignto == 0) ? size : ((size + (alignto - 1)) & ~(alignto - 1));
}

inline void * alignupPointer(void * ptr, size_t alignto) {
  return ((intptr_t)ptr%alignto == 0) ? ptr : (void *)(((intptr_t)ptr + (alignto - 1)) & ~(alignto - 1));
}

inline size_t aligndown(size_t addr, size_t alignto) { return (addr & ~(alignto - 1)); }

#define MAX(a,b) \
		({ __typeof__ (a) _a = (a); \
		 __typeof__ (b) _b = (b); \
		 _a > _b ? _a : _b; })
#define MIN(a,b) \
		({ __typeof__ (a) _a = (a); \
		 __typeof__ (b) _b = (b); \
		 _a < _b ? _a : _b; })

#ifdef LOGTOFILE
#define OUTFD getOutputFD()
#else 
#define OUTFD 2
#endif

#define PER_NODE_MAX_BIG_OBJECTS 1024

// 512K will be the threshold for small objects
// If an object is larger than this, it will be treated as large objects. 
#define BIG_OBJECT_SIZE_THRESHOLD 0x80000 

// Since each huge page will be 2MB, which will hold up to 8 objects
// For objects less than this threshold, we will allocate from normal pages in order to
// reduce the memory consumption (and memory management overhead)
#define SIZE_THRESHOLD_FROM_HUGE_PAGES 0x400000

// Normal page size is still 4K
#define PAGE_SIZE 0x1000
#define PAGE_SIZE_SHIFT 12
#define INLINE      		inline __attribute__((always_inline))

#include <sys/mman.h>

#define SIZE_PER_TB 0x10000000000  // 0x100 8*0 
#define SIZE_PER_SPAN (2*SIZE_PER_TB)
#define SIZE_PER_NODE (2*SIZE_PER_SPAN)
#define MAX_SEARCH_CALLSTACK_DEPTH 5
//#define MAX_CALLSTACK_SKIP_BOTTOM = 0 };
#define MAX_CALLSTACK_DEPTH 5

// Since there are 64TB continuous memory space, let's support up to 
// 16 node machines. That is, each node will have 4TB memory, and the first 
// thread will have the 4TB memory as well.
//#define SIZE_PER_NODE_HEAP (4*SIZE_PER_TB) // 40 bits, 4TB

// It utilizes rdtscp to get the node id, which should take much less time than invoking a system call.
inline int getRealNodeIndex(void)
{
  unsigned long a,d,c;
  __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
  return (c & 0xFFF000)>>12;
}

struct stack_frame {
  struct stack_frame * prev;  // pointing to previous stack_frame
  void * caller_address;    // the address of caller
};

struct callstack {
  size_t hashcode;
  void*  stack[MAX_CALLSTACK_DEPTH];

  /*   assign operator */
  callstack& operator = (const callstack& cs) {
    if (this != &cs) {
      hashcode = cs.hashcode;
      memcpy(&stack, &cs.stack, MAX_CALLSTACK_DEPTH * sizeof(void*));
    }
    return *this;
  }

  /* comparison */
  bool operator == (const callstack& other) const {
    bool ret = true;
    int level = 2;
    for(int i=0; i<level; i++){
      ret &= stack[i]==other.stack[i];
    }
    return ret;
  }
};


#define SIZE_PER_NODE_SHIFT 42
#define SIZE_ONE_MB 0x100000
#define SIZE_ONE_MB_MASK 0xFFFF
#define SIZE_ONE_MB_SHIFT 20
#define SIZE_HUGE_PAGE      0x200000
#define SIZE_HUGE_PAGE_MASK 0x1FFFFF
#define SIZE_HUGE_PAGE_SHIFT 21
#define SMALL_SIZE_CLASSES (16+7)
#define BIG_OBJECTS_WATERMARK (128 * SIZE_ONE_MB)

//#define MALLOC_SITE_OFFSET 0x20
#define MALLOC_SITE_OFFSET 0x48
#define SIZE_CALL_SITE_MAP 16384
#define SIZE_HASHED_OBJECTS_MAP 65536
#define SIZE_MINI_BAG 8192
#define SIZE_CLASS_TINY_SIZE 128
#define SIZE_CLASS_BASE_SMALL_SIZE 7
#define SIZE_CLASS_SMALL_SIZE 256
#define SIZE_CLASS_START_SIZE 16
#define LOG2(x) ((unsigned) (8*sizeof(unsigned long long) - __builtin_clzll((x)) - 1))
  

  inline int size2Class(size_t sz) {
    int sc;
    unsigned long offset; 
    bool aligned = true; 

    if(sz <= SIZE_CLASS_START_SIZE) {
      sc = 0;
    }
    else if (sz <= SIZE_CLASS_TINY_SIZE) {
      offset = sz - SIZE_CLASS_START_SIZE;
      sc = offset >> 4;
      if(offset > (16*sc)) {
        sc += 1;
      }
    }
    else if (sz <= SIZE_CLASS_SMALL_SIZE) {
      offset = sz - SIZE_CLASS_TINY_SIZE;
      sc = offset >> 5; 
      if(offset > (32* sc)) {
        sc += 1 + SIZE_CLASS_BASE_SMALL_SIZE;
      }
      else {
        sc += SIZE_CLASS_BASE_SMALL_SIZE;
      } 
    }
    else {
      // Note that 35 is the magic number, but it is only used here.
      sc = 35 - __builtin_clz(sz - 1);
    }

    return sc;
  }
}; // extern "C"
#endif
