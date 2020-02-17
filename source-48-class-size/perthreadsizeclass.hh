#ifndef __PER_THREAD_SIZE_CLASS_HH__
#define __PER_THREAD_SIZE_CLASS_HH__

#include "mm.hh"
#include "freelist.hh"

#define DEBUG 1
class PerThreadSizeClass {
private:
  char        * _bumpPointer; 
  char        * _bumpPointerEnd;
  unsigned int _size;  // How big for each object
  unsigned int _sc;
  unsigned int _watermark; 
  unsigned int _batch; // Allocate or contribute this number if necessary 

  unsigned int _bagSize; // Allocate one bag from outside each time 
  // During the allocation,  reutilizing freed objects in the same node will be at a higher priority. 
  // However, we don't want to keep checking the per-node list if it fails. 
  // Therefore, we will utilize some heuristics to avoid frequent checks. 
  unsigned int _allocsBeforeCheck; 
  unsigned int _allocs;
  unsigned int _allocsCheckMin;
  unsigned int _allocsCheckMax;
  unsigned int _nodeindex; 
#ifdef DEBUG
//  unsigned long _allocMBs;
#endif 
  FreeList _flist; 

public: 
  void initialize(int nodeindex, int size, int sc, int batch) {
    _size = size;
    _sc = sc;
    _batch = batch;
    _bagSize = SIZE_ONE_MB;;
    _nodeindex = nodeindex; 
    _watermark = batch * 2;
    _allocsCheckMin = batch / 4;
    
    if(_allocsCheckMin <= 1) {
      _allocsCheckMin = 1;
    }
    _allocsCheckMax = batch * 4;

    _flist.Init();

    _allocsBeforeCheck = _batch/4;
    _allocs = 0;

    _bumpPointer = NULL;
    _bumpPointerEnd = _bumpPointer; 
  }
 
  // Allocate reversely. Since _next always points to 
  // the next availabe slot, we will use the object pointed by _next-1 
  void * allocateFromFreelist();
  int moveObjectsFromNodeFreelist();
  void donateObjectsToNodeFreelist();
  void * allocate();

  void deallocate(void *ptr);
};

#endif
