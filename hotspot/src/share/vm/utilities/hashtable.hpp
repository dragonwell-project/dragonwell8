/*
 * Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_UTILITIES_HASHTABLE_HPP
#define SHARE_VM_UTILITIES_HASHTABLE_HPP

#include "classfile/classLoaderData.hpp"
#include "memory/allocation.hpp"
#include "oops/oop.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.hpp"

// This is a generic hashtable, designed to be used for the symbol
// and string tables.
//
// It is implemented as an open hash table with a fixed number of buckets.
//
// %note:
//  - TableEntrys are allocated in blocks to reduce the space overhead.



template <MEMFLAGS F> class BasicHashtableEntry : public CHeapObj<F> {
  friend class VMStructs;
private:
  unsigned int         _hash;           // 32-bit hash for item

  // Link to next element in the linked list for this bucket.  EXCEPT
  // bit 0 set indicates that this entry is shared and must not be
  // unlinked from the table. Bit 0 is set during the dumping of the
  // archive. Since shared entries are immutable, _next fields in the
  // shared entries will not change.  New entries will always be
  // unshared and since pointers are align, bit 0 will always remain 0
  // with no extra effort.
  BasicHashtableEntry<F>* _next;

  // Windows IA64 compiler requires subclasses to be able to access these
protected:
  // Entry objects should not be created, they should be taken from the
  // free list with BasicHashtable.new_entry().
  BasicHashtableEntry() { ShouldNotReachHere(); }
  // Entry objects should not be destroyed.  They should be placed on
  // the free list instead with BasicHashtable.free_entry().
  ~BasicHashtableEntry() { ShouldNotReachHere(); }

public:

  unsigned int hash() const             { return _hash; }
  void set_hash(unsigned int hash)      { _hash = hash; }
  unsigned int* hash_addr()             { return &_hash; }

  static BasicHashtableEntry<F>* make_ptr(BasicHashtableEntry<F>* p) {
    return (BasicHashtableEntry*)((intptr_t)p & -2);
  }

  BasicHashtableEntry<F>* next() const {
    return make_ptr(_next);
  }

  void set_next(BasicHashtableEntry<F>* next) {
    _next = next;
  }

  BasicHashtableEntry<F>** next_addr() {
    return &_next;
  }

  bool is_shared() const {
    return ((intptr_t)_next & 1) != 0;
  }

  void set_shared() {
    _next = (BasicHashtableEntry<F>*)((intptr_t)_next | 1);
  }
};



template <class T, MEMFLAGS F> class HashtableEntry : public BasicHashtableEntry<F> {
  friend class VMStructs;
private:
  T               _literal;          // ref to item in table.

public:
  // Literal
  T literal() const                   { return _literal; }
  T* literal_addr()                   { return &_literal; }
  void set_literal(T s)               { _literal = s; }

  HashtableEntry* next() const {
    return (HashtableEntry*)BasicHashtableEntry<F>::next();
  }
  HashtableEntry** next_addr() {
    return (HashtableEntry**)BasicHashtableEntry<F>::next_addr();
  }
};



template <MEMFLAGS F> class HashtableBucket : public CHeapObj<F> {
  friend class VMStructs;
private:
  // Instance variable
  BasicHashtableEntry<F>*       _entry;

public:
  // Accessing
  void clear()                        { _entry = NULL; }

  // The following methods use order access methods to avoid race
  // conditions in multiprocessor systems.
  BasicHashtableEntry<F>* get_entry() const;
  void set_entry(BasicHashtableEntry<F>* l);

  // The following method is not MT-safe and must be done under lock.
  BasicHashtableEntry<F>** entry_addr()  { return &_entry; }
};


template <MEMFLAGS F> class BasicHashtable : public CHeapObj<F> {
  friend class VMStructs;

public:
  BasicHashtable(int table_size, int entry_size);
  BasicHashtable(int table_size, int entry_size,
                 HashtableBucket<F>* buckets, int number_of_entries);

  // Sharing support.
  void copy_buckets(char** top, char* end);
  void copy_table(char** top, char* end);

  // Bucket handling
  int hash_to_index(unsigned int full_hash) const {
    int h = full_hash % _table_size;
    assert(h >= 0 && h < _table_size, "Illegal hash value");
    return h;
  }

  // Reverse the order of elements in each of the buckets.
  void reverse();

private:
  // Instance variables
  int               _table_size;
  HashtableBucket<F>*     _buckets;
  BasicHashtableEntry<F>* volatile _free_list;
  char*             _first_free_entry;
  char*             _end_block;
  int               _entry_size;
  volatile int      _number_of_entries;

protected:

#ifdef ASSERT
  int               _lookup_count;
  int               _lookup_length;
  void verify_lookup_length(double load);
#endif
  // to record allocated memory chunks, only used by DeallocatableHashtable
  GrowableArray<char*>*  _memory_blocks;

  void initialize(int table_size, int entry_size, int number_of_entries);

  // Accessor
  int entry_size() const { return _entry_size; }

  // The following method is MT-safe and may be used with caution.
  BasicHashtableEntry<F>* bucket(int i) const;

  // The following method is not MT-safe and must be done under lock.
  BasicHashtableEntry<F>** bucket_addr(int i) { return _buckets[i].entry_addr(); }

  // Attempt to get an entry from the free list
  BasicHashtableEntry<F>* new_entry_free_list();

  // Table entry management
  BasicHashtableEntry<F>* new_entry(unsigned int hashValue);

  // Used when moving the entry to another table
  // Clean up links, but do not add to free_list
  void unlink_entry(BasicHashtableEntry<F>* entry) {
    entry->set_next(NULL);
    --_number_of_entries;
  }

  // Move over freelist and free block for allocation
  void copy_freelist(BasicHashtable* src) {
    _free_list = src->_free_list;
    src->_free_list = NULL;
    _first_free_entry = src->_first_free_entry;
    src->_first_free_entry = NULL;
    _end_block = src->_end_block;
    src->_end_block = NULL;
  }

  // Free the buckets in this hashtable
  void free_buckets();

  // Helper data structure containing context for the bucket entry unlink process,
  // storing the unlinked buckets in a linked list.
  // Also avoids the need to pass around these four members as parameters everywhere.
  struct BucketUnlinkContext {
    int _num_processed;
    int _num_removed;
    // Head and tail pointers for the linked list of removed entries.
    BasicHashtableEntry<F>* _removed_head;
    BasicHashtableEntry<F>* _removed_tail;

    BucketUnlinkContext() : _num_processed(0), _num_removed(0), _removed_head(NULL), _removed_tail(NULL) {
    }

    void free_entry(BasicHashtableEntry<F>* entry);
  };
  // Add of bucket entries linked together in the given context to the global free list. This method
  // is mt-safe wrt. to other calls of this method.
  void bulk_free_entries(BucketUnlinkContext* context);
public:
  int table_size() { return _table_size; }
  void set_entry(int index, BasicHashtableEntry<F>* entry);

  void add_entry(int index, BasicHashtableEntry<F>* entry);

  void free_entry(BasicHashtableEntry<F>* entry);

  int number_of_entries() { return _number_of_entries; }

  void verify() PRODUCT_RETURN;
};

//
// A derived BasicHashtable with dynamic memory deallocation support
//
// BasicHashtable will permanently hold memory allocated via
// NEW_C_HEAP_ARRAY2 without releasing, because it is intended
// to be used for global data structures like SymbolTable.
// Below implementation just deallocates memory chunks in destructor,
// thus may be used as transient data structure.
//
template <MEMFLAGS F> class DeallocatableHashtable : public BasicHashtable<F> {
public:
  DeallocatableHashtable(int table_size, int entry_size)
          : BasicHashtable<F>(table_size, entry_size)
  {
    // will be released in BasicHashtable<F>::release_memory()
    GrowableArray<char*>*& mem_blocks = BasicHashtable<F>::_memory_blocks;
    mem_blocks = new (ResourceObj::C_HEAP, F) GrowableArray<char*>(0x4 /* initial size */,
                                                                   true /* on C heap */, F);
    assert(NULL != mem_blocks, "pre-condition");
  }

  ~DeallocatableHashtable() {
    BasicHashtable<F>::free_buckets();
    GrowableArray<char*>*& mem_blocks = BasicHashtable<F>::_memory_blocks;
    assert(NULL != mem_blocks, "pre-condition");

    for (GrowableArrayIterator<char*> itr = mem_blocks->begin();
         itr != mem_blocks->end(); ++itr) {
      FREE_C_HEAP_ARRAY(char*, *itr, F);
    }
    mem_blocks->clear();

    delete mem_blocks;
    mem_blocks = NULL;
  }
};


template <class T, MEMFLAGS F> class Hashtable : public BasicHashtable<F> {
  friend class VMStructs;

public:
  Hashtable(int table_size, int entry_size)
    : BasicHashtable<F>(table_size, entry_size) { }

  Hashtable(int table_size, int entry_size,
                   HashtableBucket<F>* buckets, int number_of_entries)
    : BasicHashtable<F>(table_size, entry_size, buckets, number_of_entries) { }

  // Debugging
  void print()               PRODUCT_RETURN;

  // Reverse the order of elements in each of the buckets. Hashtable
  // entries which refer to objects at a lower address than 'boundary'
  // are separated from those which refer to objects at higher
  // addresses, and appear first in the list.
  void reverse(void* boundary = NULL);


  unsigned int compute_hash(Symbol* name) const  {
    return (unsigned int) name->identity_hash();
  }

protected:
  int index_for(Symbol* name) {
    return this->hash_to_index(compute_hash(name));
  }

  // Table entry management
  HashtableEntry<T, F>* new_entry(unsigned int hashValue, T obj);

  // The following method is MT-safe and may be used with caution.
  HashtableEntry<T, F>* bucket(int i) const {
    return (HashtableEntry<T, F>*)BasicHashtable<F>::bucket(i);
  }

  // The following method is not MT-safe and must be done under lock.
  HashtableEntry<T, F>** bucket_addr(int i) {
    return (HashtableEntry<T, F>**)BasicHashtable<F>::bucket_addr(i);
  }

};

template <class T, MEMFLAGS F> class RehashableHashtable : public Hashtable<T, F> {
 protected:

  enum {
    rehash_count = 100,
    rehash_multiple = 60
  };

  // Check that the table is unbalanced
  bool check_rehash_table(int count);

 public:
  RehashableHashtable(int table_size, int entry_size)
    : Hashtable<T, F>(table_size, entry_size) { }

  RehashableHashtable(int table_size, int entry_size,
                   HashtableBucket<F>* buckets, int number_of_entries)
    : Hashtable<T, F>(table_size, entry_size, buckets, number_of_entries) { }


  // Function to move these elements into the new table.
  void move_to(RehashableHashtable<T, F>* new_table);
  static bool use_alternate_hashcode();
  static juint seed();

  static int literal_size(Symbol *symbol);
  static int literal_size(oop oop);

  // The following two are currently not used, but are needed anyway because some
  // C++ compilers (MacOS and Solaris) force the instantiation of
  // Hashtable<ConstantPool*, mtClass>::dump_table() even though we never call this function
  // in the VM code.
  static int literal_size(ConstantPool *cp) {Unimplemented(); return 0;}
  static int literal_size(Klass *k)         {Unimplemented(); return 0;}

  void dump_table(outputStream* st, const char *table_name);

 private:
  static juint _seed;
};

template <class T, MEMFLAGS F> juint RehashableHashtable<T, F>::_seed = 0;
template <class T, MEMFLAGS F> juint RehashableHashtable<T, F>::seed() { return _seed; };
template <class T, MEMFLAGS F> bool  RehashableHashtable<T, F>::use_alternate_hashcode() { return _seed != 0; };

//  Verions of hashtable where two handles are used to compute the index.

template <class T, MEMFLAGS F> class TwoOopHashtable : public Hashtable<T, F> {
  friend class VMStructs;
protected:
  TwoOopHashtable(int table_size, int entry_size)
    : Hashtable<T, F>(table_size, entry_size) {}

  TwoOopHashtable(int table_size, int entry_size, HashtableBucket<F>* t,
                  int number_of_entries)
    : Hashtable<T, F>(table_size, entry_size, t, number_of_entries) {}

public:
  unsigned int compute_hash(const Symbol* name, ClassLoaderData* loader_data) const {
    unsigned int name_hash = (unsigned int)name->identity_hash();
    // loader is null with CDS
    assert(loader_data != NULL || UseSharedSpaces || DumpSharedSpaces,
           "only allowed with shared spaces");
    unsigned int loader_hash = loader_data == NULL ? 0 : loader_data->identity_hash();
    return name_hash ^ loader_hash;
  }

  int index_for(Symbol* name, ClassLoaderData* loader_data) {
    return this->hash_to_index(compute_hash(name, loader_data));
  }
};

//======================================================================
// A hash map implementation based on hashtable
//======================================================================

// forward declaration
template <typename K, typename V, MEMFLAGS F> class HashMap;
template <typename K, typename V, MEMFLAGS F> class HashMapIterator;

// utility classes to extract hash_code from various types
class HashMapUtil : public AllStatic {
public:
  // for integers, use its own value as hash
  static unsigned int hash(long l) { return *(unsigned int*)&l; }
  static unsigned int hash(int i) { return (unsigned int)i; }
  static unsigned int hash(short s) { return (unsigned int)s; }
  static unsigned int hash(char c) { return (unsigned int)c; }
  static unsigned int hash(unsigned long sz) { return (unsigned int)(sz & 0xFFFFFFFF); }

  // use the middle bits of address value
  static unsigned int hash(void *p) {
#ifdef _LP64
    uint64_t val = *(uint64_t*)&p;
    return (unsigned int)((val & 0xFFFFFFFF) >> 3);
#else
    uint64_t val = *(uint32_t*)&p;
    return (unsigned int)((val & 0xFFFF) >> 2);
#endif // _LP64
  }

  static unsigned int hash(oop o);

  static unsigned int hash(Handle h) { return hash(h()); }

  // a general contract of getting hash code for all non pre-defined types
  // the type must define a non-static member method `unsigned int hash_code()`
  // to return an `identical` unsigned int value as hash code.
  template<typename T>
  static unsigned int hash(T t) { return t.hash_code(); }
};

// entry type
template <typename K, typename V, MEMFLAGS F = mtInternal>
class HashMapEntry : public BasicHashtableEntry<F> {
  friend class HashMap<K, V, F>;
#ifndef PRODUCT
  friend class HashMapTest;
#endif // PRODUCT

private:
  K   _key;
  V   _value;

public:
  HashMapEntry* next() {
    return (HashMapEntry*)BasicHashtableEntry<F>::next();
  }

  void set_next(HashMapEntry* next) {
    BasicHashtableEntry<F>::set_next((BasicHashtableEntry<F>*)next);
  }

  K key()         { return _key;          }
  V value()       { return _value;        }
  K* key_addr()   { return &_key;         }
  V* value_addr() { return &_value;       }
};

//
// hash map class implemented in C++ based on BasicHashtable
// - unordered
// - unique
// - not MT-safe
// - deallocatable
//
template <typename K, typename V, MEMFLAGS F = mtInternal>
class HashMap : public DeallocatableHashtable<F> {
  friend class HashMapIterator<K, V, F>;
public:
  typedef HashMapEntry<K, V, F>       Entry;
  typedef HashMapIterator<K, V, F>    Iterator;
  // alternative hashing function
  typedef int (*AltHashFunc) (K);

private:
  // if table_size == 1, will cause new_entry() to fail
  // have to allocate table with larger size, here using 0x4
  static const int MIN_TABLE_SIZE = 0x4;

  // alternativ hashing method
  AltHashFunc*     _alt_hasher;

protected:
  unsigned int compute_hash(K k) {
    if (_alt_hasher != NULL) {
      return ((AltHashFunc)_alt_hasher)(k);
    }
    return HashMapUtil::hash(k);
  }

  Entry* bucket(int index) {
    return (Entry*)BasicHashtable<F>::bucket(index);
  }

  Entry* get_entry(int index, unsigned int hash, K k) {
    for (Entry* pp = bucket(index); pp != NULL; pp = pp->next()) {
      if (pp->hash() == hash && pp->_key == k) {
        return pp;
      }
    }
    return NULL;
  }

  Entry* get_entry(K k) {
    unsigned int hash = compute_hash(k);
    return get_entry(BasicHashtable<F>::hash_to_index(hash), hash, k);
  }

  Entry* new_entry(K k, V v) {
    unsigned int hash = compute_hash(k);
    Entry* pp = (Entry*)BasicHashtable<F>::new_entry(hash);
    pp->_key = k;
    pp->_value = v;
    return pp;
  }

  void add_entry(Entry* pp) {
    int index = BasicHashtable<F>::hash_to_index(pp->hash());
    BasicHashtable<F>::add_entry(index, pp);
  }

public:
  HashMap(int table_size)
          : DeallocatableHashtable<F>((table_size < MIN_TABLE_SIZE ? MIN_TABLE_SIZE : table_size),
                                      sizeof(Entry)),
            _alt_hasher(NULL)
  { }

  // Associates the specified value with the specified key in this map.
  // If the map previously contained a mapping for the key, the old value is replaced.
  void put(K k, V v) {
    Entry* e = get_entry(k);
    if (NULL != e) {
      e->_value = v;
    } else {
      Entry* e = new_entry(k, v);
      assert(NULL != e, "cannot create new entry");
      add_entry(e);
    }
  }

  Entry* remove(K k) {
    int index = this->hash_to_index(compute_hash(k));
    Entry *e = bucket(index);
    Entry *prev = NULL;
    for (; e != NULL ; prev = e, e = e->next()) {
      if (e->_key == k) {
        if (prev != NULL) {
          prev->set_next(e->next());
        } else {
          this->set_entry(index, e->next());
        }
        this->free_entry(e);
        return e;
      }
    }
    return NULL;
  }

  // Returns true if this map contains a mapping for the specified key
  bool contains(K k) {
    return NULL != get_entry(k);
  }

  // Returns the entry to which the specified key is mapped,
  // or null if this map contains no mapping for the key.
  Entry* get(K k) {
    return get_entry(k);
  }

  // Removes all of the mappings from this map. The map will be empty after this call returns.
  void clear() {
    // put all entries just into free list
    for (int idx = 0; idx < BasicHashtable<F>::table_size(); ++idx) {
      for (Entry* entry = bucket(idx); NULL != entry;) {
        Entry* next = entry->next();
        this->free_entry(entry);
        entry = next;
      }
      BasicHashtable<F>::set_entry(idx, NULL);
    }
  }

  Iterator begin() {
    return Iterator(this);
  }

  Iterator end() {
    Iterator itr(this);
    itr._cur = NULL;
    itr._idx = BasicHashtable<F>::table_size();
    return itr;
  }

  // set an alternative hashing function
  void set_alt_hasher(AltHashFunc* hash_func) {
    _alt_hasher = hash_func;
  }
};

// External iteration support
template <typename K, typename V, MEMFLAGS F = mtInternal>
class HashMapIterator VALUE_OBJ_CLASS_SPEC {
  friend class HashMap<K, V, F>;
#ifndef PRODUCT
  friend class HashMapTest;
#endif // PRODUCT

private:
  typedef typename HashMap<K, V, F>::Entry Entry;
  Entry*              _cur;
  HashMap<K, V, F>*   _map;
  int                 _idx;

public:
  HashMapIterator(HashMap<K, V, F>* map)
          : _map(map), _cur(NULL), _idx(0) {
    for (;_idx < _map->table_size(); ++_idx) {
      _cur = _map->bucket(_idx);
      if (NULL != _cur) {
        break;
      }
    }
  }

  HashMapIterator(const HashMapIterator<K, V, F>& other)
          : _map(other._map), _cur(other._cur), _idx(other._idx)
  { }

  HashMapIterator<K, V, F>& operator++() {
    if (NULL != _cur) {
      if (NULL != _cur->next()) {
        _cur = _cur->next();
      } else {
        do {
          ++_idx;
        } while (_idx < _map->table_size()
                 && NULL == _map->bucket(_idx));

        assert(_idx <= _map->table_size(), "pre-condition");

        if (_idx == _map->table_size()) {
          // end of iteration
          _cur = NULL;
        } else {
          // move to next bucket
          _cur = _map->bucket(_idx);
        }
      }
    }

    return *this;
  }

  HashMapIterator<K, V, F>& operator = (const HashMapIterator<K, V, F>& other) {
    if (&other != this) {
      _map = other._map;
      _cur = other._cur;
      _idx = other._idx;
    }
    return *this;
  }

  Entry& operator*() { return *_cur; }

  Entry* operator->() { return _cur; }

  bool operator == (const HashMapIterator<K, V, F>& other) const {
    return (_map == other._map && _cur == other._cur && _idx == other._idx);
  }

  bool operator != (const HashMapIterator<K, V, F>& other) const {
    return (_map != other._map || _cur != other._cur || _idx != other._idx);
  }
};

#endif // SHARE_VM_UTILITIES_HASHTABLE_HPP
