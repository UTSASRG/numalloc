#ifndef __PER_SIZE_CLASS_HH__
#define __PER_SIZE_CLASS_HH__

#include <assert.h>
#include <stdio.h>
#include "persizeclasslist.hh"

class PerSizeClass {
private:
  char        * _bumpPointer; 
  char        * _bumpPointerEnd;
  unsigned int _size;  // How big for each object
  unsigned int _sc;
  unsigned int _batch; // Allocate or contribute this number if necessary 
  unsigned int _watermark;

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
  PerSizeClassList _list; 

public: 
  void initialize(int size, int sc, int batch) {
    _size = size;
    _sc = sc;
    _batch = batch;
    _bagSize = SIZE_ONE_MB;;

    if(size <= SIZE_CLASS_SMALL_SIZE) {
      // We will warmup the objects beforehand
      _miniBagSize = (SIZE_MINI_BAG/size) * size;
      _miniBagObjects = _miniBagSize/size;
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
    _bumpPointerEnd = NULL;
  }

  size_t getBagSize(void) {
    return _bagSize;
  }

  size_t getClassSize(void) {
    return _size;
  }

  bool hasItems(void) {
    return _list.hasItems();
  }

  // Allocate an object. It will return NULL
  // if no objects left
  void * allocateFromFreeList() {
    return _list.pop();
  }

  void pushRangeToList(int numb, void * head, void * tail) {
    if(numb > 0) {
     // fprintf(stderr, "push numb %d head %p tail %p\n", numb, head, tail);
      _list.pushRange(numb, head, tail);
    }
  }

  int getObjectsFromCentralList(int nodeIndex, void ** head, void ** tail);
  //int moveObjectsFromNodeFreelist();

  int getObjectsFromBumpPointer(void ** head, void ** tail);
  int updateBumpPointerAndGetObjects(void * ptr, void ** head, void ** tail);

  bool checkDonation(void) {
    return _list.length() > _watermark;
  }

  int getDonateObjects(void ** head, void ** tail) {
    assert(_watermark >= _batch);
    int numb = 0;

    if(_list.length() > _batch) { 
      numb = _list.popRange(_batch, head, tail);
    }

    return numb;
  }

  void deallocate(void *ptr) {
    _list.push(ptr);
  }

};

#endif
