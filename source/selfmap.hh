#if !defined(__SELFMAP_H)
#define __SELFMAP_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <functional>
#include <map>
#include <new>
#include <string>
#include <utility>

#include "interval.hh"

using namespace std;

struct regioninfo {
  void* start;
  void* end;
};

/**
 * A single mapping parsed from the /proc/self/maps file
 */
class mapping {
  public:
    mapping() : _valid(false) {}

    mapping(uintptr_t base, uintptr_t limit, char* perms, size_t offset, std::string file)
      : _valid(true), _base(base), _limit(limit), _readable(perms[0] == 'r'),
      _writable(perms[1] == 'w'), _executable(perms[2] == 'x'), _copy_on_write(perms[3] == 'p'),
      _offset(offset), _file(file) {}

    bool valid() const { return _valid; }

    bool isText() const { return _readable && !_writable && _executable; }

    bool isData() const { return _readable && _writable && !_executable && _copy_on_write; }

    bool isStack() const { return _file.find("[stack") != std::string::npos; }

    bool isGlobals(std::string mainfile) const {
      // global mappings are RW_P, and either the heap, or the mapping is backed
      // by a file (and all files have absolute paths)
      // the file is the current executable file, with [heap], or with lib*.so
      // Actually, the mainfile can be longer if it has some parameters.
      return (_readable && _writable && !_executable && _copy_on_write) &&
        (_file.size() > 0 && (_file == mainfile ||  _file == "[heap]" || _file.find(".so") != std::string::npos));
    }

    //maybe it is global area
    bool isGlobalsExt() const {
      return _readable && _writable && !_executable && _copy_on_write && _file.size() == 0;
    }

    uintptr_t getBase() const { return _base; }

    uintptr_t getLimit() const { return _limit; }

    size_t getOffset() const { return _offset; }

    const std::string& getFile() const { return _file; }

  private:
    bool _valid;
    uintptr_t _base;
    uintptr_t _limit;
    bool _readable;
    bool _writable;
    bool _executable;
    bool _copy_on_write;
    size_t _offset;
    std::string _file;
};

/// Read a mapping from a file input stream
static std::ifstream& operator>>(std::ifstream& f, mapping& m) {
  if(f.good() && !f.eof()) {
    uintptr_t base, limit;
    char perms[5];
    size_t offset;
    size_t dev_major, dev_minor;
    int inode;
    string path;

    // Skip over whitespace
    f >> std::skipws;

    // Read in "<base>-<limit> <perms> <offset> <dev_major>:<dev_minor> <inode>"
    f >> std::hex >> base;
    if(f.get() != '-')
      return f;
    f >> std::hex >> limit;

    if(f.get() != ' ')
      return f;
    f.get(perms, 5);

    f >> std::hex >> offset;
    f >> std::hex >> dev_major;
    if(f.get() != ':')
      return f;
    f >> std::hex >> dev_minor;
    f >> std::dec >> inode;

    // Skip over spaces and tabs
    while(f.peek() == ' ' || f.peek() == '\t') {
      f.ignore(1);
    }

    // Read out the mapped file's path
    getline(f, path);

    m = mapping(base, limit, perms, offset, path);
  }

  return f;
}

class selfmap {
  public:
    static selfmap& getInstance() {
      static char buf[sizeof(selfmap)];
      static selfmap* theOneTrueObject = new (buf) selfmap();
      return *theOneTrueObject;
    }

    /// Check whether an address is inside the DoubleTake library itself.
    bool isProberLibrary(void* pcaddr, void** offset = NULL) {
      if(offset != NULL){
        *offset = (void*)((intptr_t)pcaddr - (intptr_t)_proberStart);
      }
      return ((pcaddr >= _proberStart) && (pcaddr <= _proberEnd));
    }

    bool isPthreadLibrary(void* pcaddr, void** offset = NULL) {
      if(offset != NULL){
        *offset = (void*)((intptr_t)pcaddr - (intptr_t)_libthreadStart);
      }
      return ((pcaddr >= _libthreadStart) && (pcaddr <= _libthreadEnd));
    }

    bool isLibcLibrary(void* pcaddr, void** offset = NULL) {
      if(offset != NULL){
        *offset = (void*)((intptr_t)pcaddr - (intptr_t)_libcStart);
      }
      return ((pcaddr >= _libcStart) && (pcaddr <= _libcEnd));
    }

    /// Check whether an address is inside the main application.
    bool isApplication(void* pcaddr, void** offset = NULL) {
      if(offset != NULL){
        *offset = pcaddr;
      }
      return ((pcaddr >= _appTextStart) && (pcaddr <= _appTextEnd));
    }

    std::string getMainNameString(){
      return _main_exe;
    }
    
    std::string getPthreadLibNameString(){
      return _threadLibrary;
    }
    
    std::string getLibcNameString(){
      return _libcLibrary;
    }

    const char* getMainName(){
      return _main_exe.c_str();
    }
    
    const char* getPthreadLibName(){
      return _threadLibrary.c_str();
    }
    
    const char* getLibcName(){
      return _libcLibrary.c_str();
    }

    mapping getMappingByAddress(void* ptr){
      auto found = _mappings.find(interval(ptr, ptr));
      if(found != _mappings.end()) {
        return found->second;
      }
      return mapping();
    }

    mapping getMappingByFileName(std::string name){
      auto found = _textmappings.find(name);
      if(found != _textmappings.end()) {
        return found->second;
      }
      return mapping();
    }

    void getStackInformation(void** stackBottom, void** stackTop) {
      for(const auto& entry : _mappings) {
        const mapping& m = entry.second;
        if(m.isStack()) {
          *stackBottom = (void*)m.getBase();
          *stackTop = (void*)m.getLimit();
          return;
        }
      }
      fprintf(stderr, "Couldn't find stack mapping. Giving up.\n");
      abort();
    }

    /// Get information about global regions.
    void getTextRegions() {
      for(const auto& entry : _mappings) {
        const mapping& m = entry.second;
        if(m.isText()) {
          if(m.getFile().find("/libprober") != std::string::npos) {
            _proberStart = (void*)m.getBase();
            _proberEnd = (void*)m.getLimit();
            _currentLibrary = std::string(m.getFile());
          } else if(m.getFile().find("/libpthread-") != std::string::npos) {
            _libthreadStart = (void*)m.getBase();
            _libthreadEnd = (void*)m.getLimit();
            _threadLibrary = std::string(m.getFile());
          } else if(m.getFile().find("/libc-") != std::string::npos) {
            _libcStart = (void*)m.getBase();
            _libcEnd = (void*)m.getLimit();
            _libcLibrary = std::string(m.getFile());
          }else if(m.getFile() == _main_exe) {
            _appTextStart = (void*)m.getBase();
            _appTextEnd = (void*)m.getLimit();
          }
        }
      }
    }

  private:
    selfmap() {
      // Read the name of the main executable
      // char buffer[PATH_MAX];
      bool gotMainExe = false;
      // Build the mappings data structure
      ifstream maps_file("/proc/self/maps");

      mapping m;
      while(maps_file >> m) {
        // It is more clean that that of using readlink. 
        // readlink will have some additional bytes after the executable file 
        // if there are parameters.	
        if(!gotMainExe) {
          _main_exe = std::string(m.getFile());
          gotMainExe = true;
        }

        if(m.isText()) {
          _mappings[interval(m.getBase(), m.getLimit())] = m;
          _textmappings[m.getFile()] = m;
        }
      }
    }

    std::map<interval, mapping> _mappings;
    std::map<std::string, mapping> _textmappings;

    std::string _main_exe;
    std::string _currentLibrary;
    std::string _threadLibrary;
    std::string _libcLibrary;
    void* _appTextStart;
    void* _appTextEnd;
    void * _libthreadStart;
    void * _libthreadEnd;
    void* _proberStart;
    void* _proberEnd;
    void* _libcStart;
    void* _libcEnd;
};

#endif
