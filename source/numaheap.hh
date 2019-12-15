/*
 * FreeGuard: A Faster Secure Heap Allocator
 * Copyright (C) 2017 Sam Silvestro, Hongyu Liu, Corey Crosser, 
 *                    Zhiqiang Lin, and Tongping Liu
 *
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
 * @file   numaheap.hh: 
  *        Manage the numaheap.
 * @author Tongping Liu <http://people.umass.edu/tongping/>
 */
#ifndef __NUMAHEAP_HH__
#define __NUMAHEAP_HH__

#include <new>
#include "mm.hh"
#include "xdefines.hh"
#include "dlist.h"
#include "pernodeheap.hh"
#include "perthread.hh"
//#ifdef SPEC_MAINTHREAD_SUPPORT
#include "mainheap.hh"
//#endif

/* How to deal with the heap allocation. If one thread continuously allocates 
 * large chunk of data, such as over the number of allocations and the total size.
 * Should we place it on the same node. In such cases, 
 * some threads will be forced to read from remotely. 
 * Should we allocate some objects in a remote node, and then putting the threads into 
 * the remote node. 
 * However, how to deal with the situation that multiple threads are computing on the same data,
 * based on the protection of locks. 
 */

/**************************************
  Overall relationship of different classes. 
  NumaHeap --> PerNodeHeap --> PerNode --> PerNodeBigObjects
                                                    
                                       --> PerNodeSizeClass
*/
 
class NumaHeap {
public:
  static NumaHeap& getInstance() {
    static char buf[sizeof(NumaHeap)];
    static NumaHeap* NumaHeapObject = new (buf) NumaHeap();
    return *NumaHeapObject;
  }  

  // Initialization of the NUMA Heap
  void initialize(void) {
    //unsigned long heapSize = (NUMA_NODES + 1) * SIZE_PER_NODE;
#ifndef SPEC_MAINTHREAD_SUPPORT
    unsigned long heapSize = NUMA_NODES * SIZE_PER_NODE;
    _heapBegin = 0x100000000000; 

  // fprintf(stderr, "_heapBegin is %lx\n", _heapBegin);
    // Setting the _heapEnd address
    _heapEnd = _heapBegin + heapSize;

    char * pnheapPointer =(char *)_heapBegin;

    // Initialize the memory for the main thread
    // The main thread's memory is always located in the beginning of the heap
    // For it, we will maintain the memory location for each page, so that the memory can be re-utilized.
    //initMainThreadMemory(_heapBegin); 
    
    // Initialize for each PerNodeHeap right now. 
    for(int i = 0; i < NUMA_NODES; i++) {
      _nodes[i].initialize(i, pnheapPointer, SIZE_PER_NODE); 

      // Binding the memory to the specific node
      unsigned long mask = 1 << i;
      if(mbind(pnheapPointer, SIZE_PER_NODE, MPOL_PREFERRED, &mask, 32, 0) == -1) {
        fprintf(stderr, "Binding failure for address ptr %p, with error %s\n", pnheapPointer, strerror(errno));
      }

      pnheapPointer += SIZE_PER_NODE;
    }
#else


#endif
 
  }
 

  void * allocate(size_t size) {
    void * ptr = NULL; 

#ifndef SPEC_MAINTHREAD_SUPPORT
//   fprintf(stderr, "Thread %d at node %d: allocation size %ld\n", current->index, current->nindex, size);

    // Check the size information. 
    if(size <= BIG_OBJECT_SIZE_THRESHOLD) {
      // Small objects will be always allocated via PerThreadSizeClass
      // although PerThreadSizeClass may get objects from PerNodeHeap as well 
      ptr = current->ptheap->allocate(size);
    }
    else { 
      // Getting the node index that the current thread is running on
      int index = getNodeIndex();

      // Always allocate a large object from PerNodeHeap directly 
      ptr = _nodes[index].allocateBigObject(size);
    }
#else

#endif
   
    return ptr;
  } 

  // Allocate the specified number of freed objects from the specified node(nindex)'s size class (sc).
  int allocateBatchFromNodeFreelist(int nindex, int sc, unsigned long num, void ** start) {
    return _nodes[nindex].allocateBatch(sc, num, start);
  }

  char * allocateOnembFromNode(int nindex, size_t size) {
    return _nodes[nindex].allocateOnemb(size);
  }
  
  // Contribure some objects to the node's freelist
  int deallocateBatchToNodeFreelist(int nindex, int sc, unsigned long num, void ** start) {
    return _nodes[nindex].deallocateBatch(sc, num, start);
  }

  void deallocate(void * ptr) {
    if((uintptr_t)ptr < _heapBegin || (uintptr_t)ptr > _heapEnd) {
      return; 
    }
    
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _heapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;

    // Return it to the PerNodeHeap, since we will have to check PerOnembInfo to get the actual size. 
    // After that, the object  may return to the PerThreadHeap or PerNodeHeap
    _nodes[index].deallocate(index, ptr);
  }

  size_t getSize(void *ptr) {
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _heapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;

    return _nodes[index].getSize(ptr);
  }

private:
  size_t _heapBegin;
  size_t _heapEnd; 
  PerNodeHeap _nodes[NUMA_NODES];
};	

#endif // __NUMAHEAP_HH__	
