#if !defined(_HASHMAP_H)
#define _HASHMAP_H

#define INIT_META_MAPPING 1

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "dlist.hh"
#include "xdefines.hh"
#include "real.hh"

template <class KeyType,                    
          class ValueType,              
          class LockType> 
class HashMap {

  // Each entry has a lock.
  struct HashEntry {
    list_t list;
    // Each entry has a separate lock
    LockType lock;
    size_t count; // How many _entries in this list

    void initialize() {
      count = 0;
      listInit(&list);
      LockInit();
    }

    void Lock() { lock.lock(); }
    void Unlock() { lock.unlock(); }
    void LockInit() { lock.init(); }

    void* getFirstEntry() { return (void*)list.next; }
  };

  struct Entry {
    list_t list;
    KeyType key;
    size_t keylen;
    ValueType value;

    void initialize(KeyType ikey = 0, int ikeylen = 0, ValueType ivalue = 0) {
      listInit(&list);
      key = ikey;
      keylen = ikeylen;
      value = ivalue;
    }

    void erase() { listRemoveNode(&list); }

    struct Entry* nextEntry() { return (struct Entry*)list.next; }

    ValueType getValue() { return value; }

    KeyType getKey() { return key; }
  };

  bool _initialized;
  struct HashEntry* _entries;
  size_t _buckets;     // How many buckets in total
  size_t _bucketsUsed; // How many buckets in total

#ifdef INIT_META_MAPPING
  LockType entrylock;
  struct Entry* allentries;
  size_t entryindex;
#endif

  size_t _totalEntry;

  typedef bool (*keycmpFuncPtr)(const KeyType, const KeyType, size_t);
  typedef size_t (*hashFuncPtr)(const KeyType, size_t);
  keycmpFuncPtr _keycmp;
  hashFuncPtr _hashfunc;

public:
  HashMap() : _initialized(false) {
  }

  void initialize(hashFuncPtr hfunc, keycmpFuncPtr kcmp, const size_t size = 4096) {
    _entries = NULL;
    _bucketsUsed = 0;
    _buckets = size;
    _totalEntry = 0;
#ifdef INIT_META_MAPPING
    entryindex = 0;
#endif

    if(hfunc == NULL || kcmp == NULL) {
      abort();
    }

    // Initialize those functions.
    _hashfunc = hfunc;
    _keycmp = kcmp;

    // Allocated predefined size.
    //_entries = (struct HashEntry*)SourceHeap::allocate(size * sizeof(struct HashEntry));
    _entries = (struct HashEntry*)Real::malloc(size * sizeof(struct HashEntry));
#ifdef INIT_META_MAPPING
    allentries = (struct Entry*)Real::malloc(size * sizeof(struct Entry));

    entrylock.init();
#endif

    // Initialize all of these _entries.
    struct HashEntry* entry;
    for(size_t i = 0; i < size; i++) {
      entry = getHashEntry(i);
      entry->initialize();
    }
    _initialized = true;
  }

  inline struct HashEntry* getHashEntry(size_t index) {
    if(index < _buckets) {
      return &_entries[index];
    } else {
      return NULL;
    }
  }

  inline size_t hashIndex(const KeyType& key, size_t keylen) {
    size_t hkey = _hashfunc(key, keylen);
    return hkey & (_buckets-1);
    //return hkey % _buckets;
  }

  // Look up whether an entry is existing or not.
  // If existing, return true. *value should be carried specific value for this key.
  // Otherwise, return false.
  bool find(const KeyType& key, size_t keylen, ValueType* value) {
    assert(_initialized == true);
    size_t hindex = hashIndex(key, keylen);
    struct HashEntry* first = getHashEntry(hindex);
    struct Entry* entry = getEntry(first, key, keylen);
    bool isFound = false;

    if(entry) {
      *value = entry->value;
      isFound = true;
    }

    return isFound;
  }

  // this function is customized for call stack array
  ValueType* findOrAdd(const KeyType& key, size_t keylen, ValueType newval){
    assert(_initialized == true);
    ValueType* ret = NULL;

    size_t hindex = hashIndex(key, keylen);
    struct HashEntry* first = getHashEntry(hindex);

    struct Entry* entry = NULL;
    first->Lock();
    // Check all _entries with the same hindex.
    entry = getEntry(first, key, keylen);
    if(entry == NULL) {
      // insert new call stack into map 
      entry = insertEntry(first, key, keylen, newval);

      //entry->value.depth = 0;
      entry->value.allocation = 0;
    }
    // return the actual call stack value
    ret = &entry->value; 
    
    first->Unlock();

    return ret;
  }

  void insert(const KeyType& key, size_t keylen, ValueType value) {
    if(_initialized != true) {
      fprintf(stderr, "process %d: initialized at  %p hashmap is not true\n", getpid(), &_initialized);
    }

    assert(_initialized == true);
    size_t hindex = hashIndex(key, keylen);
    // PRINF("Insert entry:  before inserting\n");
    struct HashEntry* first = getHashEntry(hindex);

    // PRINF("Insert entry: key %p\n", key);
    first->Lock();
    insertEntry(first, key, keylen, value);
    first->Unlock();
  }

  // Insert a hash table entry if it is not existing.
  // If the entry is already existing, return true
  bool insertIfAbsent(const KeyType& key, size_t keylen, ValueType value) {
    assert(_initialized == true);
    size_t hindex = hashIndex(key, keylen);
    struct HashEntry* first = getHashEntry(hindex);
    struct Entry* entry;
    bool isFound = true;

    first->Lock();

    // Check all _entries with the same hindex.
    entry = getEntry(first, key, keylen);
    if(!entry) {
      isFound = false;
      insertEntry(first, key, keylen, value);
    }

    first->Unlock();

    return isFound;
  }

  // Free an entry with specified
  bool erase(const KeyType& key, size_t keylen) {
    assert(_initialized == true);
    size_t hindex = hashIndex(key, keylen);
    struct HashEntry* first = getHashEntry(hindex);
    struct Entry* entry;
    bool isFound = false;

    first->Lock();

    entry = getEntry(first, key, keylen);

    if(entry) {
      isFound = true;

      // Check whether this entry is the first entry.
      // Remove this entry if existing.
      entry->erase();

      Real::free(entry);
    }

    first->count--;

    first->Unlock();

    return isFound;
  }

  size_t getEntryNumber() { return _totalEntry; }

  // Clear all _entries
  void clear() {}

private:
#ifdef INIT_META_MAPPING
  void expendMapping(){
    allentries = (struct Entry*)Real::malloc(_buckets * sizeof(struct Entry));
    entryindex = 0;
  }
#endif

  // Create a new Entry with specified key and value.
  struct Entry* createNewEntry(const KeyType& key, size_t keylen, ValueType value) {
    //struct Entry* entry = (struct Entry*)SourceHeap::allocate(sizeof(struct Entry));
#ifdef INIT_META_MAPPING
    struct Entry* entry = NULL;
    entrylock.lock();
    if(entryindex > _buckets - 1){
      expendMapping();
    }
    entry = &allentries[entryindex++];
    entrylock.unlock();
#else
    struct Entry* entry = (struct Entry*)Real::malloc(sizeof(struct Entry));
#endif

    // Initialize this new entry.
    entry->initialize(key, keylen, value);
    return entry;
  }

  struct Entry* insertEntry(struct HashEntry* head, const KeyType& key, size_t keylen, ValueType value) {
    // Check whether the first entry is empty or not.
    // Create an entry
    struct Entry* entry = createNewEntry(key, keylen, value);
    listInsertTail(&entry->list, &head->list);
    head->count++;
    // increment total number
    __atomic_add_fetch(&_totalEntry, 1, __ATOMIC_RELAXED);
    return entry;
  }

  // Search the entry in the corresponding list.
  struct Entry* getEntry(struct HashEntry* first, const KeyType& key, size_t keylen) {
    struct Entry* entry = (struct Entry*)first->getFirstEntry();
    struct Entry* result = NULL;

    // Check all _entries with the same hindex.
    int count = first->count;
    while(count > 0) {
      if(likely(_keycmp(entry->key, key, keylen) && entry->keylen == keylen)) {
        result = entry;
        break;
      }

      entry = entry->nextEntry();
      count--;
    }

    return result;
  }

public:
  class iterator {
    friend class HashMap<KeyType, ValueType, LockType>;
    struct Entry* _entry; // Which entry in the current hash entry?
    size_t _pos;          // which bucket at the moment? [0, nbucket-1]
    HashMap* _hashmap;

  public:
    iterator(struct Entry* ientry = NULL, int ipos = 0, HashMap* imap = NULL) {
      _pos = ipos;
      _entry = ientry;
      _hashmap = imap;
    }

    ~iterator() {}

    iterator& operator++(int) // in postfix ++  /* parameter? */
    {
      struct HashEntry* hashentry = _hashmap->getHashEntry(_pos);

      // Check whether this entry is the last entry in current hash entry.
      if(!isListTail(&_entry->list, &hashentry->list)) {
        // If not, then we simply get next entry. No need to change pos.
        _entry = _entry->nextEntry();
      } else {
        // Since current list is empty, we must search next hash entry.
        _pos++;
        while((hashentry = _hashmap->getHashEntry(_pos)) != NULL) {
          if(hashentry->count != 0) {
            // Now we can return it.
            _entry = (struct Entry*)hashentry->getFirstEntry();
            return *this;
          }
          _pos++;
        }

        _entry = NULL;
      }

      return *this;
    }

    // iterator& operator -- ();
    // Iterpreted as a = b is treated as a.operator=(b)
    iterator& operator=(const iterator& that) {
      _entry = that._entry;
      _pos = that._pos;
      _hashmap = that._hashmap;
      return *this;
    }

    bool operator==(const iterator& that) const { return _entry == that._entry; }

    bool operator!=(const iterator& that) const { return _entry != that._entry; }

    ValueType getData() { return _entry->getValue(); }

    KeyType getkey() { return _entry->getKey(); }
  };

  // Acquire the first entry of the hash table
  iterator begin() {
    size_t pos = 0;
    struct HashEntry* head = NULL;
    struct Entry* entry;

    // Get the first non-null entry
    while(pos < _buckets) {
      head = getHashEntry(pos);
      if(head->count != 0) {
        // Now we can return it.
        entry = (struct Entry*)head->getFirstEntry();
        return iterator(entry, pos, this);
      }
      pos++;
    }

    return end();
  }

  iterator end() { return iterator(NULL, 0, this); }
};

#endif
