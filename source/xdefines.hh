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

/*
 * @file   xdefines.h
 */


extern "C" {

extern char * getThreadBuffer();
extern int outputfd;
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
#define INLINE      		inline __attribute__((always_inline))

#include <sys/mman.h>

#define SIZE_NORMAL_PAGES_PER_NODE_HEP 0x20000000000
#define SIZE_HUGE_PAGES_PER_NODE_HEP 0x20000000000
#define SIZE_PER_NODE_HEAP 0x40000000000 // 40 bits, 4TB
#define SIZE_PER_NODE_HEAP_SHIFT 42
#define SIZE_ONE_MB_BAG 0x100000
#define SIZE_ONE_MB_MASK 0xFFFF
#define SIZE_HUGE_PAGE 0x200000
#define SIZE_ONE_MB_SHIFT 20
#define SMALL_SIZE_CLASSES 16

#define SIZE_CLASS_START_SIZE 16
#define LOG2(x) ((unsigned) (8*sizeof(unsigned long long) - __builtin_clzll((x)) - 1))
}; // extern "C"
#endif
