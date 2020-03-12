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
#include "persizeclasslist.hh"
#include "hashmap.hh"
#include "hashfuncs.hh"
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
    typedef enum {
        E_CALLSITE_STATUS_INIT_PRIVATE = 0, // initial
        E_CALLSITE_STATUS_INIT_SHARED, 
        E_CALLSITE_STATUS_CHECKED_PRIVATE, // 2
        E_CALLSITE_STATUS_CHECKED_SHARED, // 3
    }eCallsiteStatus;

    class CallsiteInfo {
      eCallsiteStatus _status; 

      public:
        CallsiteInfo() {
        }

        void initSerialPhase() {
          _status = E_CALLSITE_STATUS_INIT_SHARED;
        }

        void initParallelPhase() {
          _status = E_CALLSITE_STATUS_INIT_PRIVATE;
        }

        inline bool isPrivate() {
          if(_status == E_CALLSITE_STATUS_INIT_PRIVATE || _status == E_CALLSITE_STATUS_CHECKED_PRIVATE) {
            return true;
          }
          else {
            return false;
          }
        }

        inline bool isShared() {
          if(_status == E_CALLSITE_STATUS_INIT_SHARED || _status == E_CALLSITE_STATUS_CHECKED_SHARED) {
            return true;
          }
          else {
            return false;
          }
        }

        inline bool notChecked() {
          if(_status == E_CALLSITE_STATUS_INIT_SHARED || _status == E_CALLSITE_STATUS_INIT_PRIVATE) {
           return true;
          }
          else {
           return false;
          } 
        }

        inline eCallsiteStatus  getStatus() {
          return _status;
        }

        inline void setPrivateCallsite() {
          _status = E_CALLSITE_STATUS_CHECKED_PRIVATE;
        }

        inline void setSharedCallsite() {
          _status = E_CALLSITE_STATUS_CHECKED_SHARED;
        }
    };

    // We should get a big chunk at first and do our allocation. 
    // Typically, there is no need to do the deallocation in the main thread.
    class localAllocator {
      public:
      static void * allocate(size_t size) {
        return Real::malloc(size);
      }

      static void free(void *ptr) {
        return Real::free(ptr);
      }
    };

    // CallsiteMap is used to save the callsite and corresponding information (whether it is
    // private or not).
    typedef HashMap<void *, CallsiteInfo, spinlock, localAllocator> CallsiteMap;
    CallsiteMap _callsiteMap;

    class ObjectInfo {
    public:
      CallsiteInfo * _callsiteInfo;
      union { 
        int  _tindex; // Thread index will be saved for an object in parallel phase
        int  _pSequence; // We will store sequence for an object in serial phase
      } _info; 
        
      void initSerialPhase(int sequence, CallsiteInfo * cs) {
        _callsiteInfo = cs;
        _info._pSequence = sequence;
      }

      void initParallelPhase(int threadIndex, CallsiteInfo * cs) {
        _callsiteInfo = cs;
        _info._tindex = threadIndex;
      }

      CallsiteInfo * getCallsiteInfo() {
        return _callsiteInfo;
      }

      int getSequence() {
        return _info._pSequence;
      }

      int getThreadIndex() {
        return _info._tindex;
      }
    };
    // ObjectsMap will save the mapping between the object address and its callsite
    typedef HashMap<void *, ObjectInfo, spinlock, localAllocator> ObjectsHashMap;
    ObjectsHashMap _objectsMap;

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
    _pheapBegin = 0x100000000000; 
    _pheapEnd = _pheapBegin + heapSize;

#ifdef INTERHEAP_SUPPORT
    _iheapBegin = _pheapBegin - 2*SIZE_PER_NODE;
    _iheapEnd = _pheapBegin;
    _iheap.initialize(getRealNodeIndex(), (void *)_iheapBegin);
    _serialPhase = true; 
    _phaseSequence = 0; 
      
    // Call stack map
    _callsiteMap.initialize(HashFuncs::hashStackAddr, HashFuncs::compareAddr, SIZE_CALL_SITE_MAP);
    _objectsMap.initialize(HashFuncs::hashAllocAddr, HashFuncs::compareAddr, SIZE_HASHED_OBJECTS_MAP);
#endif


    char * pnheapPointer =(char *)_pheapBegin;

    // Initialize for each PerNodeHeap right now, where their metadata will be allocated from 
    // their corresponding nodes. 
    for(int i = 0; i < NUMA_NODES; i++) {
      // Allocate the memory for pernode structure at first. 
      _nodes[i] = (PerNodeHeap *)MM::mmapFromNode(alignup(sizeof(PerNodeHeap), PAGE_SIZE), i);

      // Initialize the pernode heap
      _nodes[i]->initialize(i, pnheapPointer); 

      pnheapPointer += SIZE_PER_NODE;
    }
 
  }

#ifdef INTERHEAP_SUPPORT
  void stopSerialPhase() {
    _serialPhase = false;
    _phaseSequence++;
  } 

  void startSerialPhase() {
    _serialPhase = true;
    _phaseSequence++;
  }
  
  CallsiteInfo *getCallsiteInfo(void * key) {
    return _callsiteMap.find(key, 0); 
  }



  void * getCallsiteKey(void * ptr) {
    unsigned long key = (unsigned long)ptr;

  //fprintf(stderr, "user address %lx\n", *((unsigned long *)(address + MALLOC_SITE_OFFSET)));
    return (void *)(key + *((unsigned long *)(key + MALLOC_SITE_OFFSET)));
  }
#endif

  void * allocate(size_t size) {
    void * ptr = NULL;
    bool shared = false;

#ifdef INTERHEAP_SUPPORT
    CallsiteInfo * info;  
    bool toInsert = false;
    void * key = getCallsiteKey(&size);

    // Get the callsite type
    info = getCallsiteInfo(key);

    if(!info) {
      if(_serialPhase) 
        shared = true;
    }
    else {
      shared = info->isShared();
    }

    if(shared) {
      // Allocate from the interleaved heap if the callsite is shared.    
      ptr = _iheap.allocate(size);
    }
    else { 
#endif
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
      ptr = _nodes[index]->allocateBigObject(size);
     // fprintf(stderr, "allocate big size %lx ptr %p\n", size, ptr);
    }
#if INTERHEAP_SUPPORT
   } 
    
   // Add this object and corresponding callsite into the hashmap if necessary. 
    bool insertObjectMap = false; 
    if(!info) {
      // TODO: if multiple threads have the same callsite but with a different stack address, 
      
      // If the info is null, that means that this callsite has not been seen before. 
      CallsiteInfo cs;
      
      insertObjectMap = true;
      if(_serialPhase == true) {
        cs.initSerialPhase();
      }
      else {
        cs.initParallelPhase();
      }

      info = _callsiteMap.insertAndReturn(key, 0, cs);
    }
    else if (info->notChecked()) {
      // the corresponding callsite has not been confirmed
      insertObjectMap = true;
    }

    if(insertObjectMap) {
      ObjectInfo  objInfo;
      if(_serialPhase == true) { 
        objInfo.initSerialPhase(_phaseSequence, info); 
      }
      else {
        objInfo.initParallelPhase(getThreadIndex(), info);
      } 

      // Insert this object to the object map
      _objectsMap.insert((void *)ptr, 0, objInfo);
    }
#endif 

    return ptr;
  } 

  // Allocate the specified number of freed objects from the specified node(nindex)'s size class (sc).
  int moveBatchFromNodeFreelist(int nindex, int sc, unsigned long num, void ** head, void ** tail) {
    return _nodes[nindex]->allocateBatch(sc, num, head, tail);
  }

  void * allocateOneBagFromNode(int nindex, size_t size, size_t bagSize) {
    return _nodes[nindex]->allocateOneBag(size, bagSize);
  }
  
  // Contribure some objects to the node's freelist
  void donateBatchToNodeFreelist(int nindex, int sc, unsigned long num, void * head, void *tail) {
    _nodes[nindex]->deallocateBatch(sc, num, head, tail);
  }

  void deallocate(void * ptr) {
#ifdef INTERHEAP_SUPPORT
    // Confirm whether the corresponding object is inside the hashmap or not
    ObjectInfo * info = _objectsMap.find((void *)ptr, 0);
    if(info) {
      CallsiteInfo * cs = info->getCallsiteInfo();
      eCallsiteStatus csStatus = cs->getStatus();

      if(csStatus == E_CALLSITE_STATUS_INIT_SHARED) {
        // Check the sequence number
        if(info->getSequence() == _phaseSequence) {
          cs->setPrivateCallsite();
        }
        else {
          cs->setSharedCallsite();
        }
      }
      else if(csStatus == E_CALLSITE_STATUS_INIT_PRIVATE) {
        // Check whether the allocator is the same as the current thread
        if(info->getThreadIndex() == getThreadIndex()) {
          cs->setPrivateCallsite();
        }
        else {
          cs->setSharedCallsite();
        }
      }

      _objectsMap.erase((void *)ptr, 0);
    } 

    if(((uintptr_t)ptr >= _iheapBegin) && ((uintptr_t)ptr < _iheapEnd)) {
      return _iheap.deallocate(ptr);
    }
#endif
    if((uintptr_t)ptr < _pheapBegin || (uintptr_t)ptr > _pheapEnd) {
      return; 
    }
    
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _pheapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;
    
    // Return it to the PerNodeHeap, since we will have to check PerOnembInfo to get the actual size. 
    // After that, the object  may return to the PerThreadHeap or PerNodeHeap
    _nodes[index]->deallocate(index, ptr);
  }

  size_t getSize(void *ptr) {
#ifdef INTERHEAP_SUPPORT
    if(((uintptr_t)ptr >= _iheapBegin) && ((uintptr_t)ptr < _iheapBegin)) {
      return _iheap.getSize(ptr);      
    }
#endif
    // Check the address range of this ptr
    size_t offset = (size_t)ptr - _pheapBegin;

    // Compute the node index by the offset
    int index = offset >> SIZE_PER_NODE_SHIFT;

    return _nodes[index]->getSize(ptr);
  }

private:
  size_t _pheapBegin;
  size_t _pheapEnd; 
#ifdef INTERHEAP_SUPPORT
  size_t _iheapBegin;
  size_t _iheapEnd;
  InterHeap _iheap;
  bool   _serialPhase;
  int    _phaseSequence;
#endif
  PerNodeHeap * _nodes[NUMA_NODES];
};	

#endif // __NUMAHEAP_HH__	
