/*
 * Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_CLASSFILE_CLASSLOADERDATA_HPP
#define SHARE_VM_CLASSFILE_CLASSLOADERDATA_HPP

#include "memory/allocation.hpp"
#include "memory/memRegion.hpp"
#include "memory/metaspace.hpp"
#include "runtime/mutex.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_JFR
#include "jfr/support/jfrTraceIdExtension.hpp"
#endif

//
// A class loader represents a linkset. Conceptually, a linkset identifies
// the complete transitive closure of resolved links that a dynamic linker can
// produce.
//
// A ClassLoaderData also encapsulates the allocation space, called a metaspace,
// used by the dynamic linker to allocate the runtime representation of all
// the types it defines.
//
// ClassLoaderData are stored in the runtime representation of classes and the
// system dictionary, are roots of garbage collection, and provides iterators
// for root tracing and other GC operations.

class ClassLoaderData;
class JNIMethodBlock;
class Metadebug;

// GC root for walking class loader data created

class ClassLoaderDataGraph : public AllStatic {
  friend class ClassLoaderData;
  friend class ClassLoaderDataGraphMetaspaceIterator;
  friend class ClassLoaderDataGraphKlassIteratorAtomic;
  friend class VMStructs;
  friend class MetaspaceDumper;
 private:
  // All CLDs (except the null CLD) can be reached by walking _head->_next->...
  static ClassLoaderData* _head;
  static ClassLoaderData* _unloading;
  // CMS support.
  static ClassLoaderData* _saved_head;
  static ClassLoaderData* _saved_unloading;
  static bool _should_purge;

  static ClassLoaderData* add(Handle class_loader, bool anonymous, bool& is_created, TRAPS);
  static void clean_metaspaces();
 public:
  static ClassLoaderData* find_or_create(Handle class_loader, TRAPS);
  static void purge();
  static void clear_claimed_marks();
  // oops do
  static void oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim);
  static void keep_alive_oops_do(OopClosure* blk, KlassClosure* klass_closure, bool must_claim);
  static void always_strong_oops_do(OopClosure* blk, KlassClosure* klass_closure, bool must_claim);
  // cld do
  static void cld_do(CLDClosure* cl);
  static void cld_unloading_do(CLDClosure* cl);
  static void roots_cld_do(CLDClosure* strong, CLDClosure* weak);
  static void keep_alive_cld_do(CLDClosure* cl);
  static void always_strong_cld_do(CLDClosure* cl);
  // klass do
  static void classes_do(KlassClosure* klass_closure);
  static void classes_do(void f(Klass* const));
  static void loaded_classes_do(KlassClosure* klass_closure);
  static void classes_unloading_do(void f(Klass* const));
  static bool do_unloading(BoolObjectClosure* is_alive, bool clean_alive);

  // CMS support.
  static void remember_new_clds(bool remember) { _saved_head = (remember ? _head : NULL); }
  static GrowableArray<ClassLoaderData*>* new_clds();

  static void set_should_purge(bool b) { _should_purge = b; }
  static void purge_if_needed() {
    // Only purge the CLDG for CMS if concurrent sweep is complete.
    if (_should_purge) {
      purge();
      // reset for next time.
      set_should_purge(false);
    }
  }

  static void free_deallocate_lists();

  static void dump_on(outputStream * const out) PRODUCT_RETURN;
  static void dump() { dump_on(tty); }
  static void verify();

  static bool unload_list_contains(const void* x);
#ifndef PRODUCT
  static bool contains_loader_data(ClassLoaderData* loader_data);
#endif
};

// ClassLoaderData class

class ClassLoaderData : public CHeapObj<mtClass> {
  friend class VMStructs;
  friend class MetaspaceDumper;
 private:
  class Dependencies VALUE_OBJ_CLASS_SPEC {
    objArrayOop _list_head;
    void locked_add(objArrayHandle last,
                    objArrayHandle new_dependency,
                    Thread* THREAD);
   public:
    Dependencies() : _list_head(NULL) {}
    Dependencies(TRAPS) : _list_head(NULL) {
      init(CHECK);
    }
    void add(Handle dependency, TRAPS);
    void init(TRAPS);
    void oops_do(OopClosure* f);
  };

  class ChunkedHandleList VALUE_OBJ_CLASS_SPEC {
    struct Chunk : public CHeapObj<mtClass> {
      static const size_t CAPACITY = 32;

      oop _data[CAPACITY];
      volatile juint _size;
      Chunk* _next;

      Chunk(Chunk* c) : _next(c), _size(0) { }
    };

    Chunk* _head;

    void oops_do_chunk(OopClosure* f, Chunk* c, const juint size);

   public:
    ChunkedHandleList() : _head(NULL) {}
    ~ChunkedHandleList();

    // Only one thread at a time can add, guarded by ClassLoaderData::metaspace_lock().
    // However, multiple threads can execute oops_do concurrently with add.
    oop* add(oop o);
    void oops_do(OopClosure* f);
  };

  friend class ClassLoaderDataGraph;
  friend class ClassLoaderDataGraphKlassIteratorAtomic;
  friend class ClassLoaderDataGraphMetaspaceIterator;
  friend class MetaDataFactory;
  friend class Method;

  static ClassLoaderData * _the_null_class_loader_data;

  oop _class_loader;          // oop used to uniquely identify a class loader
                              // class loader or a canonical class path
  Dependencies _dependencies; // holds dependencies from this class loader
                              // data to others.

  Metaspace * volatile _metaspace;  // Meta-space where meta-data defined by the
                                    // classes in the class loader are allocated.
  Mutex* _metaspace_lock;  // Locks the metaspace for allocations and setup.
  bool _unloading;         // true if this class loader goes away
  bool _keep_alive;        // if this CLD is kept alive without a keep_alive_object().
  bool _is_anonymous;      // if this CLD is for an anonymous class
  volatile int _claimed;   // true if claimed, for example during GC traces.
                           // To avoid applying oop closure more than once.
                           // Has to be an int because we cas it.
  Klass* volatile _klasses;   // The classes defined by the class loader.

  ChunkedHandleList _handles; // Handles to constant pool arrays, etc, which
                              // have the same life cycle of the corresponding ClassLoader.

  // These method IDs are created for the class loader and set to NULL when the
  // class loader is unloaded.  They are rarely freed, only for redefine classes
  // and if they lose a data race in InstanceKlass.
  JNIMethodBlock*                  _jmethod_ids;

  // Metadata to be deallocated when it's safe at class unloading, when
  // this class loader isn't unloaded itself.
  GrowableArray<Metadata*>*      _deallocate_list;

  // Support for walking class loader data objects
  ClassLoaderData* _next; /// Next loader_datas created

  // ReadOnly and ReadWrite metaspaces (static because only on the null
  // class loader for now).
  static Metaspace* _ro_metaspace;
  static Metaspace* _rw_metaspace;

  JFR_ONLY(DEFINE_TRACE_ID_FIELD;)

  void set_next(ClassLoaderData* next) { _next = next; }
  ClassLoaderData* next() const        { return _next; }

  ClassLoaderData(Handle h_class_loader, bool is_anonymous, Dependencies dependencies);
  ~ClassLoaderData();

  Mutex* metaspace_lock() const { return _metaspace_lock; }

  void unload();
  bool keep_alive() const       { return _keep_alive; }
  void classes_do(void f(Klass*));
  void loaded_classes_do(KlassClosure* klass_closure);
  void classes_do(void f(InstanceKlass*));

  // Deallocate free list during class unloading.
  void free_deallocate_list();

  // Allocate out of this class loader data
  MetaWord* allocate(size_t size);

 public:

  // GC interface.
  void clear_claimed()          { _claimed = 0; }
  bool claimed() const          { return _claimed == 1; }
  bool claim();

  bool is_alive(BoolObjectClosure* is_alive_closure) const;

  // Accessors
  Metaspace* metaspace_or_null() const     { return _metaspace; }

  static ClassLoaderData* the_null_class_loader_data() {
    return _the_null_class_loader_data;
  }

  bool is_anonymous() const { return _is_anonymous; }

  static void init_null_class_loader_data() {
    assert(_the_null_class_loader_data == NULL, "cannot initialize twice");
    assert(ClassLoaderDataGraph::_head == NULL, "cannot initialize twice");

    // We explicitly initialize the Dependencies object at a later phase in the initialization
    _the_null_class_loader_data = new ClassLoaderData((oop)NULL, false, Dependencies());
    ClassLoaderDataGraph::_head = _the_null_class_loader_data;
    assert(_the_null_class_loader_data->is_the_null_class_loader_data(), "Must be");
    if (DumpSharedSpaces) {
      _the_null_class_loader_data->initialize_shared_metaspaces();
    }
  }

  bool is_the_null_class_loader_data() const {
    return this == _the_null_class_loader_data;
  }
  bool is_ext_class_loader_data() const;

  bool is_builtin_class_loader_data() const;

  bool is_system_class_loader_data() const;

  // The Metaspace is created lazily so may be NULL.  This
  // method will allocate a Metaspace if needed.
  Metaspace* metaspace_non_null();

  oop class_loader() const      { return _class_loader; }

  // The object the GC is using to keep this ClassLoaderData alive.
  oop keep_alive_object() const;

  // Returns true if this class loader data is for a loader going away.
  bool is_unloading() const     {
    assert(!(is_the_null_class_loader_data() && _unloading), "The null class loader can never be unloaded");
    return _unloading;
  }

  // Used to make sure that this CLD is not unloaded.
  void set_keep_alive(bool value) { _keep_alive = value; }

  unsigned int identity_hash() {
    return _class_loader == NULL ? 0 : _class_loader->identity_hash();
  }

  // Used when tracing from klasses.
  void oops_do(OopClosure* f, KlassClosure* klass_closure, bool must_claim);

  void classes_do(KlassClosure* klass_closure);

  JNIMethodBlock* jmethod_ids() const              { return _jmethod_ids; }
  void set_jmethod_ids(JNIMethodBlock* new_block)  { _jmethod_ids = new_block; }

  void print_value() { print_value_on(tty); }
  void print_value_on(outputStream* out) const;
  void dump(outputStream * const out) PRODUCT_RETURN;
  void verify();
  const char* loader_name();

  jobject add_handle(Handle h);
  void add_class(Klass* k);
  void remove_class(Klass* k);
  bool contains_klass(Klass* k);
  void record_dependency(Klass* to, TRAPS);
  void init_dependencies(TRAPS);

  void add_to_deallocate_list(Metadata* m);

  static ClassLoaderData* class_loader_data(oop loader);
  static ClassLoaderData* class_loader_data_or_null(oop loader);
  static ClassLoaderData* anonymous_class_loader_data(oop loader, TRAPS);
  static void print_loader(ClassLoaderData *loader_data, outputStream *out);

  // CDS support
  Metaspace* ro_metaspace();
  Metaspace* rw_metaspace();
  void initialize_shared_metaspaces();

  JFR_ONLY(DEFINE_TRACE_ID_METHODS;)
};

// An iterator that distributes Klasses to parallel worker threads.
class ClassLoaderDataGraphKlassIteratorAtomic : public StackObj {
 Klass* volatile _next_klass;
 public:
  ClassLoaderDataGraphKlassIteratorAtomic();
  Klass* next_klass();
 private:
  static Klass* next_klass_in_cldg(Klass* klass);
};

class ClassLoaderDataGraphMetaspaceIterator : public StackObj {
  ClassLoaderData* _data;
 public:
  ClassLoaderDataGraphMetaspaceIterator();
  ~ClassLoaderDataGraphMetaspaceIterator();
  bool repeat() { return _data != NULL; }
  Metaspace* get_next() {
    assert(_data != NULL, "Should not be NULL in call to the iterator");
    Metaspace* result = _data->metaspace_or_null();
    _data = _data->next();
    // This result might be NULL for class loaders without metaspace
    // yet.  It would be nice to return only non-null results but
    // there is no guarantee that there will be a non-null result
    // down the list so the caller is going to have to check.
    return result;
  }
};
#endif // SHARE_VM_CLASSFILE_CLASSLOADERDATA_HPP
