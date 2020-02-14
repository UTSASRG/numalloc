#ifndef __PER_THREAD_SIZE_CLASS_HH__
#define __PER_THREAD_SIZE_CLASS_HH__

#include "mm.hh"
#include "persizeclasslist.hh"

class PerThreadSizeClass {
private:
  char        * _bumpPointer; 
  char        * _bumpPointerEnd;
  unsigned int _size;  // How big for each object
  unsigned int _sc;
  unsigned int _watermark; 
  unsigned int _batch; // Allocate or contribute this number if necessary 

  // We will learn from TcMalloc. We will add all objects in the miniBag to the freelist in order
  // to warmup the objects. 
  unsigned int _miniBagSize;
  unsigned int _miniBagObjects;
  unsigned int _bagUsableSize;
  unsigned int _bagSize; // Allocate one bag from outside each time 
  // During the allocation,  reutilizing freed objects in the same node will be at a higher priority. 
  // However, we don't want to keep checking the per-node list if it fails. 
  // Therefore, we will utilize some heuristics to avoid frequent checks. 
  unsigned int _allocsBeforeCheck; 
  unsigned int _allocs;
  unsigned int _allocsCheckMin;
  unsigned int _allocsCheckMax;
  unsigned int _nodeindex; 
  PerSizeClassList _list; 

public: 
  void initialize(int nodeindex, int size, int sc, int batch) {
    _size = size;
    _sc = sc;
    _batch = batch;
    _bagSize = SIZE_ONE_MB;;
    _nodeindex = nodeindex;

    if(size <= SIZE_CLASS_SMALL_SIZE) {
      // We will warmup the objects beforehand
      _miniBagSize = (SIZE_MINI_BAG/size) * size;
      _miniBagObjects = _miniBagSize/size - 1;
      _bagUsableSize = (_bagSize/size) * size;
    } 

    _watermark = batch * 2;
    _allocsCheckMin = batch / 4;
    
    if(_allocsCheckMin <= 1) {
      _allocsCheckMin = 1;
    }
    _allocsCheckMax = batch * 4;

    _list.initialize();

    _allocsBeforeCheck = _batch/4;
    _allocs = 0;

    _bumpPointer = NULL;
    _bumpPointerEnd = _bumpPointer; 
  }
  // Allocate reversely. Since _next always points to 
  // the next availabe slot, we will use the object pointed by _next-1 
  int  moveObjectsFromNodeFreelist();
  void donateObjectsToNodeFreelist();
  void * allocate();

  void deallocate(void *ptr);
};

#endif
