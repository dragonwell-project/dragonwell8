/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef SHARED_VM_JWARMUP_JITWARMUP_HPP
#define SHARED_VM_JWARMUP_JITWARMUP_HPP

#include "code/codeBlob.hpp"
#include "libadt/dict.hpp"
#include "memory/allocation.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"
#include "oops/methodCounters.hpp"
#include "oops/methodData.hpp"
#include "runtime/atomic.hpp"
#include "runtime/timer.hpp"
#include "runtime/mutexLocker.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/linkedlist.hpp"
#include "utilities/ostream.hpp"
#include "utilities/symbolMatcher.hpp"

// forward
class ProfileRecorder;
class PreloadJitInfo;

#define INVALID_FIRST_INVOKE_INIT_ORDER -1

class JitWarmUp : public CHeapObj<mtInternal> {
protected:
  JitWarmUp();
  virtual ~JitWarmUp();

public:
  enum JitWarmUpState {
    NOT_INIT = 0,
    IS_OK = 1,
    IS_ERR = 2
  };

  bool         is_valid() { return _state == JitWarmUp::IS_OK; }
  unsigned int version()  { return _version; }

  // init in VM startup
  void init();
  // init for CompilationWarmUpRecording model
  JitWarmUpState init_for_recording();
  // init for CompilationWarmUp model
  JitWarmUpState init_for_warmup();

  static JitWarmUp* create_instance();
  static JitWarmUp* instance()  { return _instance; }

  ProfileRecorder* recorder()   { return _recorder; }
  PreloadJitInfo*  preloader()  { return _preloader; }

  SymbolMatcher<mtClass>* excluding_matcher() { return _excluding_matcher; }

  void set_dummy_method(Method* m)          { _dummy_method = m; }
  Method* dummy_method()                    { return _dummy_method; }

  // write log file for JitWarmUpRecording model
  JitWarmUpState flush_logfile();

  // commit a compilation task
  static bool commit_compilation(methodHandle m, int bci, TRAPS);

  // get loader name through ClassLoaderData, if ClassLoader is
  // bootstrap classloader, return "NULL"
  static Symbol* get_class_loader_name(ClassLoaderData* cld);

private:
  // singleton holder
  static JitWarmUp*            _instance;

  JitWarmUpState               _state;
  unsigned int                 _version;     // JitWarmUp version, for verify logfile
  Method*                      _dummy_method;
  ProfileRecorder*             _recorder;
  PreloadJitInfo*              _preloader;
  SymbolMatcher<mtClass>*      _excluding_matcher;
};

// =========================== Profile Recorder Module ========================== //

// this hashtable stores method profile info that will be
// record into log file
class ProfileRecorderEntry : public HashtableEntry<Method*, mtInternal> {
public:
  ProfileRecorderEntry() {  }
  virtual ~ProfileRecorderEntry() {  }

  // not use ctor function because Hashtable::new_entry is C-style design
  // It don't call ctor function when Hashtable constructing a entry
  void init() {
    _is_deopted = false;
    _bci = InvocationEntryBci;
  }

  void set_bci(int bci) { _bci = bci; }
  int  bci()            { return _bci; }

  void set_order(int order) { _order = order; }
  int  order()              { return _order; }

  ProfileRecorderEntry* next() {
    return (ProfileRecorderEntry*)HashtableEntry<Method*, mtInternal>::next();
  }

private:
  bool   _is_deopted; // not used
  int    _bci;        // bci of compilation task,
                      // InvocationEntryBci means standard compilation
                      // other means OSR compilation
  int    _order;      // compilation order
};

// a hash table stores compiled method
class ProfileRecordDictionary : public Hashtable<Method*, mtInternal> {
  friend class VMStructs;
  friend class JitWarmUp;
public:
  ProfileRecordDictionary(unsigned int size);
  virtual ~ProfileRecordDictionary();

  // add method into dictionary
  // return entry that holds this method
  ProfileRecorderEntry* add_method(unsigned int hash, Method* method, int bci);
  // find a method in the dictionary
  // if not found, return NULL
  ProfileRecorderEntry* find_entry(unsigned int hash, Method* method);

  void free_entry(ProfileRecorderEntry* entry);

  unsigned int count() { return _count; }

  // NYI
  void print();
  // virtual void print_on(outputStream* st) const;

  ProfileRecorderEntry* bucket(int i) {
    return (ProfileRecorderEntry*)Hashtable<Method*, mtInternal>::bucket(i);
  }

private:
  unsigned int _count;  // entry count
  // create an entry for a given method
  ProfileRecorderEntry* new_entry(unsigned int hash, Method* method);
};

// This entry used in the list that records class symbol
class ClassSymbolEntry {
private:
  Symbol* _class_name;
  Symbol* _class_loader_name;
  Symbol* _path;
public:
  ClassSymbolEntry(Symbol* class_name, Symbol* class_loader_name, Symbol* path)
    : _class_name(class_name),
      _class_loader_name(class_loader_name),
      _path(path) {
    if (_class_name != NULL) _class_name->increment_refcount();
    if (_class_loader_name != NULL) _class_loader_name->increment_refcount();
    if (_path != NULL) _path->increment_refcount();
  }

  ClassSymbolEntry()
    : _class_name(NULL),
      _class_loader_name(NULL),
      _path(NULL) {
  }

  ~ClassSymbolEntry() {
    if (_class_name != NULL) _class_name->decrement_refcount();
    if (_class_loader_name != NULL) _class_loader_name->decrement_refcount();
    if (_path != NULL) _path->decrement_refcount();
  }

  Symbol* class_name() const { return _class_name; }
  Symbol* class_loader_name() const { return _class_loader_name; }
  Symbol* path() const { return _path; }

  // Necessary for LinkedList
  bool equals(const ClassSymbolEntry& rhs) const {
    return _class_name == rhs._class_name;
  }
};

// Profiling data collection
// record compiled method in a hash table(class ProfileRecorder)
// record java class initialization order in a linkedlist(class LinkedListImpl<Entry>)
class ProfileRecorder : public CHeapObj<mtInternal> {
public:
  ProfileRecorder();
  virtual ~ProfileRecorder();

  void init();
  unsigned int recorded_count()   { return _dict->count(); }
  ProfileRecordDictionary* dict() { return _dict; }

  JitWarmUp*   holder()                 { return _holder; }
  void         set_holder(JitWarmUp* h) { _holder = h; }

  unsigned int flushed()                { return _flushed; }
  void         set_flushed(bool value)  { _flushed = value; }

  const char*  logfile_name()                      { return _logfile_name; }
  void         set_logfile_name(const char* name)  { _logfile_name = name; }

  LinkedListImpl<ClassSymbolEntry>*
      class_init_list()                    { return _class_init_list; }

  int class_init_count()                   { return _class_init_order_count + 1; }

  // add a method into recorder
  void add_method(Method* method, int bci);
  // remove a method from recorder
  void remove_method(Method* method);

  // flush collected information into log file
  void flush();

  // increment class initialize count
  // class recorded and increase order number, returns the increased number.
  // this function is thread safe
  int assign_class_init_order(InstanceKlass* klass);

  // current class init order count and its address.
  address current_init_order_addr() { return (address)&_class_init_order_count; }

  unsigned int compute_hash(Method* method) {
    uint64_t m_addr = (uint64_t)method;
    return (m_addr >> 3) * 2654435761UL; // Knuth multiply hash
  }

  bool is_valid() { return _state == IS_OK; }

  static int compute_crc32(randomAccessFileStream* fs);

private:
  enum RecorderState {
    IS_OK = 0,
    IS_ERR = 1,
    NOT_INIT = 2
  };

  JitWarmUp*                                   _holder;
  // output stream
  randomAccessFileStream*                      _logfile;
  // position of log file in flush()
  unsigned int                                 _pos;
  RecorderState                                _state;
  // linked list that stores orderly initialization info of java classes
  LinkedListImpl<ClassSymbolEntry>*            _class_init_list;
  LinkedListNode<ClassSymbolEntry>*            _init_list_tail_node;
  // hash table that stores compiled methods
  ProfileRecordDictionary*                     _dict;
  // counter of class initialization order
  volatile int                                 _class_init_order_count;
  volatile bool                                _flushed;
  // log file name
  const char*                                  _logfile_name;
  // record max symbol length in log file
  int                                          _max_symbol_length;

private:
  // flush section
  void write_header();
  void write_inited_class();
  void write_record(Method* method, int bci, int order);
  void write_footer();

  void write_u1(u1 value);
  void write_u4(u4 value);
  void write_u8(u8 value);
  void write_string(const char* src);
  void write_string(const char* src, size_t len);

  void overwrite_u4(u4 value, unsigned int offset);

  void update_max_symbol_length(int len);
};

// =========================== Preload Class Module ========================== //

#define CLASSCACHE_HASHTABLE_SIZE 10240

// forward declare
class ProfileRecorder;
class PreloadClassHolder;

// a MDRecordInfo corresponds a ProfileData per bci (see oops/methodData.hpp)
// NYI about details
class MDRecordInfo : public CHeapObj<mtInternal> {
public:
  // empty class
  MDRecordInfo() {  }
  ~MDRecordInfo() {  }
};

// a method holder corresponds a method and its profile information
class PreloadMethodHolder : public CHeapObj<mtInternal> {
  friend class PreloadClassHolder;
public:
  PreloadMethodHolder(Symbol* name, Symbol* signature);
  PreloadMethodHolder(PreloadMethodHolder& rhs);
  virtual ~PreloadMethodHolder();

  Symbol* name()      const { return _name; }
  Symbol* signature() const { return _signature; }

  unsigned int intp_invocation_count() const { return _intp_invocation_count; }
  unsigned int intp_throwout_count()   const { return _intp_throwout_count;   }
  unsigned int invocation_count()      const { return _invocation_count;      }
  unsigned int backage_count()         const { return _backage_count;         }

  void set_intp_invocation_count(unsigned int value) { _intp_invocation_count = value; }
  void set_intp_throwout_count(unsigned int value)   { _intp_throwout_count = value; }
  void set_invocation_count(unsigned int value)      { _invocation_count = value; }
  void set_backage_count(unsigned int value)         { _backage_count = value; }

  unsigned int hash()            const { return _hash; }
  unsigned int size()            const { return _size; }
  int          bci()             const { return _bci; }
  int          mounted_offset()  const { return _mounted_offset; }

  void set_hash(unsigned int value)           { _hash = value; }
  void set_size(unsigned int value)           { _size = value; }
  void set_bci(int value)                     { _bci = value; }
  void set_mounted_offset(int value)          { _mounted_offset = value; }

  bool deopted()                 const { return _is_deopted; }
  void set_deopted(bool value)         { _is_deopted = value; }

  // check for size/hash
  bool check_matching(Method* method);

  PreloadMethodHolder* next() const                     { return _next; }
  void set_next(PreloadMethodHolder* h)                 { _next = h; }

  Method* resolved_method() const                       { return _resolved_method; }
  void set_resolved_method(Method* m)                   { _resolved_method = m; }

  GrowableArray<MDRecordInfo*>* md_list()         const { return _md_list; }
  void set_md_list(GrowableArray<MDRecordInfo*>* value) { _md_list = value; }

  PreloadMethodHolder* clone_and_add();

  // whether the resolved method is alive
  bool is_alive(BoolObjectClosure* is_alive_closure) const;

private:
  // below members are parsed from log file
  Symbol*      _name;
  Symbol*      _signature;

  unsigned int _size;
  unsigned int _hash;
  int          _bci;

  unsigned int _intp_invocation_count;
  unsigned int _intp_throwout_count;
  unsigned int _invocation_count;
  unsigned int _backage_count;

  int          _mounted_offset;

  // whether md_list array is owned by this
  bool         _owns_md_list;

  // whether resolved method has been deoptimized by JitWarmUp
  bool         _is_deopted;

  // A single LinkedList store entries for same init order
  PreloadMethodHolder*           _next;
  // resolved method in holder's list
  Method*                        _resolved_method;
  // profile info array, shared between same PreloadMethodHolder
  GrowableArray<MDRecordInfo*>*  _md_list;
};

// a class holder corresponds a java class
class PreloadClassHolder : public CHeapObj<mtInternal> {
public:
  PreloadClassHolder(Symbol* name, Symbol* loader_name,
                     Symbol* path, unsigned int size,
                     unsigned int hash, unsigned int crc32);
  virtual ~PreloadClassHolder();

  void add_method(PreloadMethodHolder* mh) {
      assert(_method_list != NULL, "not initialize");
      _method_list->append(mh);
  }

  unsigned int             size() const              { return _size; }
  unsigned int             hash() const              { return _hash; }
  unsigned int             crc32() const             { return _crc32; }
  unsigned int             methods_count() const     { return _method_list->length(); }
  Symbol*                  class_name() const        { return _class_name; }
  Symbol*                  class_loader_name() const { return _class_loader_name; }
  Symbol*                  path() const              { return _path; }
  PreloadClassHolder*      next() const              { return _next; }
  bool                     resolved() const          { return _resolved; }

  void                     set_resolved()            { _resolved = true; }
  void                     set_next(PreloadClassHolder* h) { _next = h; }

  GrowableArray<PreloadMethodHolder*>* method_list() const { return _method_list; }

private:
  // parsed from log file
  unsigned int                          _size;
  unsigned int                          _hash;
  unsigned int                          _crc32;
  Symbol*                               _class_name;
  Symbol*                               _class_loader_name;
  Symbol*                               _path;
  // method holder list
  GrowableArray<PreloadMethodHolder*>*  _method_list;
  // if the holder is resolved
  bool                                  _resolved;

  unsigned int                          _class_init_chain_index;
  // next class holder that has same class name
  PreloadClassHolder*                   _next;
};

// a class entry corresponds a java class name symbol
// it mounts a list of PreloadClassHolder which has same name
class PreloadClassEntry : public HashtableEntry<Symbol*, mtInternal> {
  friend class PreloadJitInfo;
public:
  PreloadClassEntry(PreloadClassHolder* holder)
      : _head_holder(holder),
        _chain_offset(-1),
        _loader_name(NULL),
        _path(NULL) {
      // do nothing
  }

  PreloadClassEntry()
      : _head_holder(NULL),
        _chain_offset(-1),
        _loader_name(NULL),
        _path(NULL) {
  }

  virtual ~PreloadClassEntry() {  }

  void init() {
      _head_holder = NULL;
      _chain_offset = -1;
      _loader_name = NULL;
      _path = NULL;
  }

  PreloadClassHolder* head_holder()                          { return _head_holder; }
  void                set_head_holder(PreloadClassHolder* h) { _head_holder = h; }

  int                 chain_offset()               { return _chain_offset; }
  void                set_chain_offset(int offset) { _chain_offset = offset; }

  Symbol*             loader_name()                { return _loader_name; }
  void                set_loader_name(Symbol* s)   { _loader_name = s; }
  Symbol*             path()                       { return _path; }
  void                set_path(Symbol* s)          { _path = s; }


  PreloadClassEntry*  next() {
      return (PreloadClassEntry*)HashtableEntry<Symbol*, mtInternal>::next();
  }

  // an entry has a chain of class holder
  void add_class_holder(PreloadClassHolder* h) {
      h->set_next(_head_holder);
      _head_holder = h;
  }

  // find class holder with specified size&crc32
  // if not found, return NULL
  PreloadClassHolder* find_holder_in_entry(unsigned int size, unsigned int crc32);

private:
  PreloadClassHolder* _head_holder;  // head node of holder list
  int                 _chain_offset; // chain index is initialization order of this class, used in class PreloadClassChain
  Symbol*             _loader_name;  // classloader name
  Symbol*             _path;         // class file path
};

// a hash table stores PreloadClassEntrys that parsed from log file
//
// PreloadClassDictionary extend Hashtable
//   |
//   + -- ClassEntry: +- ClassHolder +- class name
//                    .              +- size
//                    .              +- crc32
//                    .              +- MethodHolder list
//                    .                  |
//                    .                  +- MDRecordInfo list
//                    .
//                    +- ClassHolder ..
//
class PreloadClassDictionary : public Hashtable<Symbol*, mtInternal> {
public:
  PreloadClassDictionary(int size);
  virtual ~PreloadClassDictionary();

  // remove a class entry
  void remove_entry(unsigned int hash_value, unsigned int class_size,
                    unsigned int crc32, Symbol* symbol);

  // find a class entry with name, classloader and path
  PreloadClassEntry* find_entry(unsigned int hash_value, Symbol* name,
                                Symbol* loader_name, Symbol* path);

  // find the first class entry with the given class name
  PreloadClassEntry* find_head_entry(unsigned int hash_value, Symbol* name);

  PreloadClassEntry* find_entry(InstanceKlass* k);

  // find a specified class holder
  PreloadClassHolder* find_holder(unsigned int hash_value, unsigned int class_size,
                                  unsigned int crc32, Symbol* name,
                                  Symbol* loader_name, Symbol* path);

  PreloadClassEntry* bucket(int i) {
      return (PreloadClassEntry*)Hashtable<Symbol*, mtInternal>::bucket(i);
  }

  // find or create a class entry, if not found, new a entry and return it
  PreloadClassEntry* find_and_add_class_entry(unsigned int hash_value, Symbol* symbol,
                                              Symbol* loader_name, Symbol* path,
                                              int order);

private:
  // new entry
  PreloadClassEntry* new_entry(Symbol* symbol);
};

class PreloadClassChain;

// traverse MethodHolder from end to begin in PreloadClassChain
class MethodHolderIterator {
public:
  MethodHolderIterator()
    : _chain(NULL),
      _cur(NULL),
      _index(-1) {
  }

  MethodHolderIterator(PreloadClassChain* chain, PreloadMethodHolder* holder, int index)
    : _chain(chain),
      _cur(holder),
      _index(index) {
  }

  ~MethodHolderIterator() { /* do nothing */ }

  PreloadMethodHolder* operator*() { return _cur; }

  int index() { return _index; }

  bool initialized() { return _chain != NULL; }

  PreloadMethodHolder* next();

private:
  PreloadClassChain*      _chain;
  PreloadMethodHolder* _cur;
  int                  _index;  // current holder's position in PreloadClassChain
};

// record java class load event
// forcely initialize class in order(from jwarmup log file)
// PreloadClassChain member layout & force initialization diagram
//
//
//  state -------- |      PreloadClassChainEntry :  +-----------------+
//                 v                 |              |  class name     |
//               -----               |              |  state          |
// head entry -> | 3 | <-------------+              |  method list    |
//               | 3 |                              |  InstanceKlass* |
//               | 3 |                              +-----------------+
//               | 3 | <- | method(compiled) | <- | method(compiled) |
//               | 1 |
// init_index -> | 3 | <- | method(compilable) |
//               | 2 |
// load_index -> | 2 |
//               | 0 |
//               | 2 | <- | method(not compiled) | <- | method(not compiled) |
//               | 3 |
//                 .
//                 .
//               | 0 |
//               | 2 |
//               | 0 |
//               | 0 |
//               | 0 |
//               -----
//  Entry state: 0 : class is NOT loaded
//               1 : entry skipped
//               2 : class has been loaded
//               3 : class has been initialized
//  Compiling methods list which begins the entry at index i
//  need ensure that all entries before index i have been either initialized or skipped
//
class PreloadClassChain : public CHeapObj<mtClass> {
public:
  class PreloadClassChainEntry : public CHeapObj<mtClass> {
  public:
    enum InitState {
      _not_loaded = 0,
      _is_skipped,
      _is_loaded,
      _is_inited
    };

    PreloadClassChainEntry()
      : _class_name(NULL),
        _loader_name(NULL),
        _path(NULL),
        _state(_not_loaded),
        _method_holder(NULL),
        _resolved_klasses(new (ResourceObj::C_HEAP, mtClass)
                          GrowableArray<InstanceKlass*>(1, true, mtClass)) {  }

    PreloadClassChainEntry(Symbol* class_name, Symbol* loader_name, Symbol* path)
      : _class_name(class_name),
        _loader_name(loader_name),
        _path(path),
        _state(_not_loaded),
        _method_holder(NULL),
        _resolved_klasses(new (ResourceObj::C_HEAP, mtClass)
                          GrowableArray<InstanceKlass*>(1, true, mtClass)) {  }

    virtual ~PreloadClassChainEntry() {  }

    Symbol*        class_name()                  const { return _class_name; }
    Symbol*        loader_name()                 const { return _loader_name; }
    Symbol*        path()                        const { return _path; }
    void           set_class_name(Symbol* name)  { _class_name = name; }
    void           set_loader_name(Symbol* name) { _loader_name = name; }
    void           set_path(Symbol* path)        { _path = path; }

    GrowableArray<InstanceKlass*>* resolved_klasses()
                   { return _resolved_klasses; }

    // entry state
    bool           is_not_loaded()        const { return _state == _not_loaded; }
    bool           is_skipped()           const { return _state == _is_skipped; }
    bool           is_loaded()            const { return _state == _is_loaded; }
    bool           is_inited()            const { return _state == _is_inited; }
    void           set_not_loaded()       { _state = _not_loaded; }
    void           set_skipped()          { _state = _is_skipped; }
    void           set_loaded()           { _state = _is_loaded; }
    void           set_inited()           { _state = _is_inited; }

    int            state()                { return _state; }

    void add_method_holder(PreloadMethodHolder* h) {
      h->set_next(_method_holder);
      _method_holder = h;
    }

    bool is_all_initialized();

    // check if any resolved classes has been redefined before warmup compilation
    bool has_redefined_class();

    InstanceKlass* get_first_uninitialized_klass();

    PreloadMethodHolder* method_holder()  { return _method_holder; }

  private:
    Symbol*                              _class_name;
    Symbol*                              _loader_name;
    Symbol*                              _path;
    int                                  _state;
    PreloadMethodHolder*                 _method_holder;
    GrowableArray<InstanceKlass*>*       _resolved_klasses;
  };

  PreloadClassChain(unsigned int size);
  virtual ~PreloadClassChain();

  enum ClassChainState {
    NOT_INITED = 0,
    INITED = 1,
    PRE_WARMUP = 2,
    WARMUP_COMPILING = 3,
    WARMUP_DONE = 4,
    WARMUP_PRE_DEOPTIMIZE = 5,
    WARMUP_DEOPTIMIZING = 6,
    WARMUP_DEOPTIMIZED = 7,
    ERROR_STATE = 8
  };
  bool state_trans_to(ClassChainState new_state);
  ClassChainState current_state() { return _state; }

  int  inited_index()        const { return _inited_index; }
  int  loaded_index()        const { return _loaded_index; }
  int  length()              const { return _length; }

  void set_inited_index(int index)         { _inited_index = index; }
  void set_loaded_index(int index)         { _loaded_index = index; }
  void set_length(int length)              { _length = length; }

  bool has_unmarked_compiling_flag()               { return _has_unmarked_compiling_flag; }
  void set_has_unmarked_compiling_flag(bool value) { _has_unmarked_compiling_flag = value; }

  PreloadJitInfo* holder() { return _holder; }
  void set_holder(PreloadJitInfo* preloader) { _holder = preloader; }

  bool can_do_initialize() {
    return _state == PRE_WARMUP;
  }

  bool notify_deopt_signal() {
    return state_trans_to(WARMUP_PRE_DEOPTIMIZE);
  }

  // recording loaded class is only doable before warmup compilation
  bool can_record_class() {
    return _state == INITED || _state == PRE_WARMUP || _state == WARMUP_COMPILING;
  }

  bool deopt_has_started() {
    return _state == WARMUP_DEOPTIMIZING || _state == WARMUP_DEOPTIMIZED;
  }

  bool deopt_has_done() {
    return _state == WARMUP_DEOPTIMIZED;
  }

  // record java class load event,
  // update indexes and do class loading eagerly
  void record_loaded_class(InstanceKlass* klass);

  PreloadClassChainEntry* at(int index) { return &_entries[index]; }

  // refresh indexes and entry's state
  void refresh_indexes();

  // iterate preload class chain and submit warmup compilation requests
  void warmup_impl();

  // mount method
  void mount_method_at(PreloadMethodHolder* mh, int index);

  // a PreloadMethodHolder represents a java method
  bool compile_methodholder(PreloadMethodHolder* mh);

  // fix InstanceKlass* and Method* pointer during metaspace gc
  void do_unloading(BoolObjectClosure* is_alive);

  // proactive deoptimize methods in safepoint after CompilationWarmUpDeoptTime second
  void deopt_prologue();
  void deopt_epilogue();

  bool should_deoptimize_methods();

  // deoptimize CompilationWarmUpDeoptNumOfMethodsPerIter number methods per invocation
  void deoptimize_methods();

  // invoke a VM_Deoptimize operation
  void invoke_deoptimize_vmop();

  // load class eagerly that occurs in JitWarmUp log file through constant-pool traversal
  void eager_load_class_in_constantpool();

  // for debug
  void print_not_loaded_before(int index);
  void print_method_mount_before(int index);

private:
  // left of _inited_index(contain it) has been initialization
  int                   _inited_index;
  // left of _loaded_index(contain it) has been loaded
  int                   _loaded_index;
  // length of entries
  int                   _length;
  // state of PreloadClassChain
  volatile ClassChainState _state;

  PreloadClassChainEntry*  _entries;

  PreloadJitInfo*       _holder;

  // method deoptimization support
  TimeStamp             _init_timestamp;  // timestamp of deoptimization beginning
  TimeStamp             _last_timestamp;  // timestamp of last deoptimization task

  int                   _deopt_index;
  PreloadMethodHolder*  _deopt_cur_holder;

  bool                  _has_unmarked_compiling_flag;

  void compile_methodholders_queue(Stack<PreloadMethodHolder*, mtInternal>& compile_queue);

  // update _loaded_index
  void update_loaded_index(int index);

  // assign profile info to this method
  PreloadMethodHolder* resolve_method_info(Method* method,
                                           PreloadClassHolder* holder);
};

// This class about preload & compile method from
// log file in JitWarmUp model
class PreloadJitInfo : public CHeapObj<mtInternal> {
public:
  enum PreloadInfoState {
    NOT_INIT = 0,
    IS_OK = 1,
    IS_ERR = 2
  };

  PreloadJitInfo();
  virtual ~PreloadJitInfo();

  bool is_valid() { return _state == IS_OK; }
  void init();

  bool should_load_class_eagerly(Symbol* s);

  PreloadClassDictionary* dict() { return _dict; }
  uint64_t                loaded_count() { return _loaded_count; }

  PreloadClassChain*      chain() { return _chain; }
  void                    set_chain(PreloadClassChain* chain) { _chain = chain; }

  JitWarmUp*              holder() { return _holder; }
  void                    set_holder(JitWarmUp* h) { _holder = h; }

  // record java class loading and assign profile info to this klass
  bool resolve_loaded_klass(InstanceKlass* klass);

  // will be called after jvm booted (in runtime/init.cpp)
  void jvm_booted_is_done();

  // will be called by JWarmUP java API nofityApplicationStartUpIsDone
  void notify_application_startup_is_done();

  // remove known meaningless suffix
  static Symbol* remove_meaningless_suffix(Symbol* s);

private:
  PreloadClassDictionary*  _dict;
  PreloadClassChain*       _chain;
  uint64_t                 _loaded_count; // methods parsed from JitWarmUp log file
  PreloadInfoState         _state;
  JitWarmUp*               _holder;
  bool                     _jvm_booted_is_done;
};

#endif //SHARED_VM_JWARMUP_JITWARMUP_HPP
