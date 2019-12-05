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
 * @file   mm.hh: memory mapping functions.
 * @author Tongping Liu <http://www.cs.utsa.edu/~tongpingliu/>
 * @author Sam Silvestro <sam.silvestro@utsa.edu>
 */
#ifndef __MM_HH__
#define __MM_HH__

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <numaif.h>
#include <numa.h>
#include "xdefines.hh"

class MM {
public:
#define ALIGN_TO_CACHELINE(size) (size % 64 == 0 ? size : (size + 64) / 64 * 64)

  static void mmapDeallocate(void* ptr, size_t sz) { munmap(ptr, sz); }

  static void* mmapAllocateShared(size_t sz, int fd = -1, void* startaddr = NULL) {
    return allocate(true, false, sz, fd, startaddr);
  }

  static void* mmapAllocatePrivate(size_t sz, void* startaddr = NULL, int fd = -1) {
    return allocate(false, false, sz, fd, startaddr);
  }
  
  static void* mmapPrivateHugepages(size_t sz, void* startaddr = NULL, int fd = -1) {
    void * ptr = allocate(false, true, sz, fd, startaddr);
		//madvise(ptr, sz, MADV_HUGEPAGE);
		return ptr;
  }

  static void * mmapFromNode(size_t sz, int nodeindex) {
    size_t size; 
    void * ptr; 

    // Check whether a block should be allocated from the big heap
    if(sz >= SIZE_ONE_MB_BAG) {
      size = alignup(sz, SIZE_HUGE_PAGE);
      ptr = mmapPrivateHugepages(size);
      //ptr = mmapAllocatePrivate(sz);
    }
    else {
      size = sz;
      ptr = mmapAllocatePrivate(sz);
    }

  //  fprintf(stderr, "mmap from node %d size %lx\n", nodeindex, sz);
    unsigned long mask = 1 << nodeindex;
    // Currently, we only support up to 32 nodes. 
    if(mbind(ptr, size, MPOL_BIND, &mask, 32, 0) == -1) {
      fprintf(stderr, "Binding failure for address ptr %p, with error %s\n", ptr, strerror(errno));
    }
    
    return ptr; 
  }

private:
  static void* allocate(bool isShared, bool isHugePage, size_t sz, int fd, void* startaddr) {
    int protInfo = PROT_READ | PROT_WRITE;
    int sharedInfo = isShared ? MAP_SHARED : MAP_PRIVATE;
    sharedInfo |= ((fd == -1) ? MAP_ANONYMOUS : 0);
    sharedInfo |= ((startaddr != (void*)0) ? MAP_FIXED : 0);
    sharedInfo |= MAP_NORESERVE;

    if(isHugePage) {
     sharedInfo |= MAP_HUGETLB;
      //sharedInfo |= MAP_HUGETLB | MAP_HUGE_2MB;
    }

    void* ptr = mmap(startaddr, sz, protInfo, sharedInfo, fd, 0);
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Couldn't do mmap (%s) : startaddr %p, sz %lx, protInfo=%d, sharedInfo=%d\n",
            strerror(errno), startaddr, sz, protInfo, sharedInfo);
			exit(-1);
    }
   // else {
   //    fprintf (stderr, "Successful map 0x%lx.\n", sz);
   // }

    return ptr;
  }
};

class InternalHeapAllocator {
public:
  static void* malloc(size_t sz);
  static void free(void* ptr);
  static void* allocate(size_t sz);
  static void deallocate(void* ptr);
};
#endif
