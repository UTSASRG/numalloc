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
#include "dlist.hh"
#include "pernodeheap.hh"
#include "perthread.hh"
#include "real.hh"
#ifdef INTERHEAP_SUPPORT
#include "interheap.hh"
#endif

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
unsigned long long rdtscp();
extern volatile unsigned long long allocsfor48; 
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
    unsigned long heapSize = NUMA_NODES * SIZE_PER_NODE;
    _heapBegin = 0x100000000000; 
#ifdef INTERHEAP_SUPPORT
    _mainHeapBegin = _heapBegin - 2*SIZE_PER_NODE;
    _interHeap.initialize(getRealNodeIndex(), (void *)_mainHeapBegin);
    _sequentialPhase = true;  
#endif

  // fprintf(stderr, "_heapBegin is %lx\n", _heapBegin);
    // Setting the _heapEnd address
    _heapEnd = _heapBegin + heapSize;

    char * pnheapPointer =(char *)_heapBegin;

    // Initialize the memory for the main thread
    // The main thread's memory is always located in the beginning of the heap
    // For it, we will maintain the memory location for each page, so that the memory can be re-utilized.

    // Allocate memory from PerNode    
    // Initialize for each PerNodeHeap right now. 
    for(int i = 0; i < NUMA_NODES; i++) {
      _nodes[i] = (PerNodeHeap *)MM::mmapFromNode(alignup(sizeof(PerNodeHeap), PAGE_SIZE), i);

      // Allocate the memory for pernode structure at first. 
      _nodes[i]->initialize(i, pnheapPointer); 

      pnheapPointer += SIZE_PER_NODE;
    }
 
  }

#ifdef INTERHEAP_SUPPORT
  void stopSerialPhase() {
    _sequentialPhase = false;
    _interHeap.stopSerialPhase();
  } 

  void startSerialPhase() {
    _sequentialPhase = true;
    _interHeap.startSerialPhase();
  }

#endif

  void * allocate(size_t size) {
    void * ptr = NULL; 

#ifdef INTERHEAP_SUPPORT
    //if(_sequentialPhase && (size > (PAGE_SIZE * NUMA_NODES/2))) {
    if(_sequentialPhase) {
      ptr = _interHeap.allocate(size);
      if(ptr) { 
        return ptr;
      }
    }
    // If we can't allocate, the object should be allocated in the local PerNodeHeap. 
#endif

    // Check the size information. 
    if(size <= BIG_OBJECT_SIZE_THRESHOLD) {
     // fprintf(stderr, "allocate from numaheap size %lx\n", size);
      // Small objects will be always allocated via PerThreadSizeClass
      // although PerThreadSizeClass may get objects from PerNodeHeap as well 
      ptr = current->ptheap->allocate(size);
    }
    else { 
      // Getting the node index that the current thread is running on
      int index = getNodeIndex();

      // Always allocate a large object from PerNodeHeap directly 
      ptr = _nodes[index]->allocateBigObject(size);
    }
    
     //fprintf(stderr, "allocate size %lx ptr %p\n", size, ptr);

    return ptr;
  } 
  
  void * allocateOneBagFromNode(int nindex, size_t classSize, size_t bagSize, bool allocBig) {
    return _nodes[nindex]->allocateOneBag(classSize, bagSize, allocBig);
  }

  // Allocate the specified number of freed objects from the specified node(nindex)'s size class (sc).
  int getObjectsFromNode(unsigned int nindex, unsigned int classIndex, void ** head, void ** tail) {
    return _nodes[nindex]->allocateObjects(classIndex, head, tail);
  }

  // Contribure some objects to the node's freelist
  void donateObjectsToNode(int nindex, int sc, unsigned long num, void * head, void *tail) {
    _nodes[nindex]->deallocateBatch(sc, num, head, tail);
  }

  void deallocate(void * ptr) {
 //   fprintf(stderr, "deallocate ptr %p\n", ptr);
#ifdef INTERHEAP_SUPPORT
    if(((uintptr_t)ptr >= _mainHeapBegin) && ((uintptr_t)ptr < _heapBegin)) {
      return _interHeap.deallocate(ptr);
    }
#endif
    if((uintptr_t)ptr < _heapBegin || (uintptr_t)ptr > _heapEnd) {
      return; 
    }
    
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _heapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;
    
    // Return it to the PerNodeHeap, since we will have to check PerOnembInfo to get the actual size. 
    // After that, the object  may return to the PerThreadHeap or PerNodeHeap
    _nodes[index]->deallocate(index, ptr);
  }

  size_t getSize(void *ptr) {
#ifdef INTERHEAP_SUPPORT
    if(((uintptr_t)ptr >= _mainHeapBegin) && ((uintptr_t)ptr < _heapBegin)) {
      return _interHeap.getSize(ptr);      
    }
#endif
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _heapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;

    ///Jin
    if(_nodes[index] == nullptr) {
        return -1;
    }

    return _nodes[index]->getSize(ptr);
  }

private:
  size_t _heapBegin;
#ifdef INTERHEAP_SUPPORT
  size_t _mainHeapBegin;
  InterHeap _interHeap;
  bool   _sequentialPhase;
#endif
  size_t _heapEnd; 
  PerNodeHeap * _nodes[NUMA_NODES];
};	

#endif // __NUMAHEAP_HH__	
