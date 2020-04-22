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

  static void* mmapAllocatePrivate(size_t sz, void* startaddr = NULL, bool isHugePage = false, int fd = -1) {
    return allocate(false, isHugePage, sz, fd, startaddr);
  }
  
  static void bindMemoryToNode(void * ptr, size_t size, int nodeindex) {
    unsigned long mask = 1 << nodeindex;
    // Binding the memory to a specific node before the actual access. 
    if(mbind(ptr, size, MPOL_BIND, &mask, NUMA_NODES+1, 0) == -1) {
      fprintf(stderr, "Binding failure for address ptr %p size %ld, with error %s\n", ptr, size, strerror(errno));
      exit(-1);
    }
    return; 
  }

    static void *mmapPageInterleaved(size_t sz, void *startaddr, int mainThreadNodeIndex) {
    void * ptr;

    // Mmap a block from the OS. 
    ptr = mmapAllocatePrivate(sz, startaddr);
    unsigned long mask = (1 << NUMA_NODES)-1;

#ifdef MAX_USE_NODE
        mask = 0;
        int currentNodeIndex = mainThreadNodeIndex;
        for (int i = 0; i < MAX_USE_NODE; i++) {
            mask = mask | 1 << currentNodeIndex;
            currentNodeIndex = ++currentNodeIndex % NUMA_NODES;
        }
#endif

    // Set the memory to be interleaved, and the nodemax has to be set to 1 more than the NUMA_NODES
    if(mbind(ptr, sz, MPOL_INTERLEAVE, &mask, NUMA_NODES+1, 0) == -1) {
      fprintf(stderr, "Binding failure for address ptr %p size %ld, with error %s\n", ptr, sz, strerror(errno));
      exit(-1);
    }

    return ptr;
  }

  static void bindMemoryBlockwise(char * pointer, size_t pages, int startNodeIndex, bool isHugePage = false) {
#if 1
      int total_use_node_num=NUMA_NODES;
#ifdef MAX_USE_NODE
      total_use_node_num = MAX_USE_NODE;
#endif

    int pagesPerNode = pages / total_use_node_num;



      double totalStrides = (double)(pages - pagesPerNode * total_use_node_num) / total_use_node_num;
    double stride = (double)1 / total_use_node_num;

    int nindex = startNodeIndex; 
    double strides = 0.0;
    size_t size;
    for(int i = 0; i < total_use_node_num; i++) {
      int pages = pagesPerNode;
      strides += stride; 
      if(strides <= totalStrides) {
        pages += 1;
      } 
    
      // Now we will compute the actual size 
      size = isHugePage ? (pages * SIZE_HUGE_PAGE) : (pages * PAGE_SIZE);

      bindMemoryToNode(pointer, size, nindex);

      pointer += size;
      nindex++;
      if(nindex == NUMA_NODES) {
        nindex = 0;
      }
    }
#endif   
  }

  static void * mmapFromNode(size_t sz, int nodeindex, void * startaddr = NULL, bool isHugePage = false) {
    size_t size; 
    void * ptr; 

    // Check whether a block should be allocated from the big heap
    if(sz >= SIZE_ONE_MB) {
      size = alignup(sz, SIZE_HUGE_PAGE);
      ptr = mmapAllocatePrivate(size, startaddr, isHugePage);
      //ptr = mmapAllocatePrivate(sz);
    }
    else {
      size = sz;
      ptr = mmapAllocatePrivate(size, startaddr, isHugePage);
    }

    // Binding mmap from the node
    bindMemoryToNode(ptr, size, nodeindex);
    
    return ptr; 
  }

private:
  static void* allocate(bool isShared, bool isHugePage, size_t sz, int fd, void* startaddr) {
    int protInfo = PROT_READ | PROT_WRITE;
    int sharedInfo = isShared ? MAP_SHARED : MAP_PRIVATE;
    sharedInfo |= ((fd == -1) ? MAP_ANONYMOUS : 0);
    sharedInfo |= ((startaddr != (void*)0) ? MAP_FIXED : 0);
    sharedInfo |= MAP_NORESERVE;

#ifdef USE_HUGE_PAGE
    if(isHugePage) {
      sharedInfo |= MAP_HUGETLB;
    }
#endif

    void* ptr = mmap(startaddr, sz, protInfo, sharedInfo, fd, 0);
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Couldn't do mmap (%s) : startaddr %p, sz %lx, protInfo=%d, sharedInfo=%d\n",
            strerror(errno), startaddr, sz, protInfo, sharedInfo);
			exit(-1);
    }

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
