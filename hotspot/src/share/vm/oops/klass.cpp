/*
 * Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "gc_implementation/shared/markSweep.inline.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "memory/heapInspection.hpp"
#include "memory/metadataFactory.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "runtime/atomic.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "utilities/stack.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "gc_implementation/parallelScavenge/psParallelCompact.hpp"
#include "gc_implementation/parallelScavenge/psPromotionManager.hpp"
#include "gc_implementation/parallelScavenge/psScavenge.hpp"
#endif // INCLUDE_ALL_GCS
#if INCLUDE_JFR
#include "jfr/support/jfrTraceIdExtension.hpp"
#endif

bool Klass::is_cloneable() const {
  return _access_flags.is_cloneable() ||
         is_subtype_of(SystemDictionary::Cloneable_klass());
}

void Klass::set_is_cloneable() {
  if (oop_is_instance() && InstanceKlass::cast(this)->reference_type() != REF_NONE) {
    // Reference cloning should not be intrinsified and always happen in JVM_Clone.
  } else {
    _access_flags.set_is_cloneable();
  }
}

void Klass::set_name(Symbol* n) {
  _name = n;
  if (_name != NULL) _name->increment_refcount();
}

bool Klass::is_subclass_of(const Klass* k) const {
  // Run up the super chain and check
  if (this == k) return true;

  Klass* t = const_cast<Klass*>(this)->super();

  while (t != NULL) {
    if (t == k) return true;
    t = t->super();
  }
  return false;
}

bool Klass::search_secondary_supers(Klass* k) const {
  // Put some extra logic here out-of-line, before the search proper.
  // This cuts down the size of the inline method.

  // This is necessary, since I am never in my own secondary_super list.
  if (this == k)
    return true;
  // Scan the array-of-objects for a match
  int cnt = secondary_supers()->length();
  for (int i = 0; i < cnt; i++) {
    if (secondary_supers()->at(i) == k) {
      ((Klass*)this)->set_secondary_super_cache(k);
      return true;
    }
  }
  return false;
}

// Return self, except for abstract classes with exactly 1
// implementor.  Then return the 1 concrete implementation.
Klass *Klass::up_cast_abstract() {
  Klass *r = this;
  while( r->is_abstract() ) {   // Receiver is abstract?
    Klass *s = r->subklass();   // Check for exactly 1 subklass
    if( !s || s->next_sibling() ) // Oops; wrong count; give up
      return this;              // Return 'this' as a no-progress flag
    r = s;                    // Loop till find concrete class
  }
  return r;                   // Return the 1 concrete class
}

// Find LCA in class hierarchy
Klass *Klass::LCA( Klass *k2 ) {
  Klass *k1 = this;
  while( 1 ) {
    if( k1->is_subtype_of(k2) ) return k2;
    if( k2->is_subtype_of(k1) ) return k1;
    k1 = k1->super();
    k2 = k2->super();
  }
}


void Klass::check_valid_for_instantiation(bool throwError, TRAPS) {
  ResourceMark rm(THREAD);
  THROW_MSG(throwError ? vmSymbols::java_lang_InstantiationError()
            : vmSymbols::java_lang_InstantiationException(), external_name());
}


void Klass::copy_array(arrayOop s, int src_pos, arrayOop d, int dst_pos, int length, TRAPS) {
  THROW(vmSymbols::java_lang_ArrayStoreException());
}


void Klass::initialize(TRAPS) {
  ShouldNotReachHere();
}

bool Klass::compute_is_subtype_of(Klass* k) {
  assert(k->is_klass(), "argument must be a class");
  return is_subclass_of(k);
}

Klass* Klass::find_field(Symbol* name, Symbol* sig, fieldDescriptor* fd) const {
#ifdef ASSERT
  tty->print_cr("Error: find_field called on a klass oop."
                " Likely error: reflection method does not correctly"
                " wrap return value in a mirror object.");
#endif
  ShouldNotReachHere();
  return NULL;
}

Method* Klass::uncached_lookup_method(Symbol* name, Symbol* signature, OverpassLookupMode overpass_mode) const {
#ifdef ASSERT
  tty->print_cr("Error: uncached_lookup_method called on a klass oop."
                " Likely error: reflection method does not correctly"
                " wrap return value in a mirror object.");
#endif
  ShouldNotReachHere();
  return NULL;
}

void* Klass::operator new(size_t size, ClassLoaderData* loader_data, size_t word_size, TRAPS) throw() {
  return Metaspace::allocate(loader_data, word_size, /*read_only*/false,
                             MetaspaceObj::ClassType, THREAD);
}

Klass::Klass() {
  Klass* k = this;

  // Preinitialize supertype information.
  // A later call to initialize_supers() may update these settings:
  set_super(NULL);
  for (juint i = 0; i < Klass::primary_super_limit(); i++) {
    _primary_supers[i] = NULL;
  }
  set_secondary_supers(NULL);
  set_secondary_super_cache(NULL);
  _primary_supers[0] = k;
  set_super_check_offset(in_bytes(primary_supers_offset()));

  // The constructor is used from init_self_patching_vtbl_list,
  // which doesn't zero out the memory before calling the constructor.
  // Need to set the field explicitly to not hit an assert that the field
  // should be NULL before setting it.
  _java_mirror = NULL;

  set_modifier_flags(0);
  set_layout_helper(Klass::_lh_neutral_value);
  set_name(NULL);
  AccessFlags af;
  af.set_flags(0);
  set_access_flags(af);
  set_subklass(NULL);
  set_next_sibling(NULL);
  set_next_link(NULL);

  set_prototype_header(markOopDesc::prototype());
  set_biased_lock_revocation_count(0);
  set_last_biased_lock_bulk_revocation_time(0);

  // The klass doesn't have any references at this point.
  clear_modified_oops();
  clear_accumulated_modified_oops();
  _shared_class_path_index = -1;
}

jint Klass::array_layout_helper(BasicType etype) {
  assert(etype >= T_BOOLEAN && etype <= T_OBJECT, "valid etype");
  // Note that T_ARRAY is not allowed here.
  int  hsize = arrayOopDesc::base_offset_in_bytes(etype);
  int  esize = type2aelembytes(etype);
  bool isobj = (etype == T_OBJECT);
  int  tag   =  isobj ? _lh_array_tag_obj_value : _lh_array_tag_type_value;
  int lh = array_layout_helper(tag, hsize, etype, exact_log2(esize));

  assert(lh < (int)_lh_neutral_value, "must look like an array layout");
  assert(layout_helper_is_array(lh), "correct kind");
  assert(layout_helper_is_objArray(lh) == isobj, "correct kind");
  assert(layout_helper_is_typeArray(lh) == !isobj, "correct kind");
  assert(layout_helper_header_size(lh) == hsize, "correct decode");
  assert(layout_helper_element_type(lh) == etype, "correct decode");
  assert(1 << layout_helper_log2_element_size(lh) == esize, "correct decode");

  return lh;
}

bool Klass::can_be_primary_super_slow() const {
  if (super() == NULL)
    return true;
  else if (super()->super_depth() >= primary_super_limit()-1)
    return false;
  else
    return true;
}

void Klass::initialize_supers(Klass* k, TRAPS) {
  if (FastSuperclassLimit == 0) {
    // None of the other machinery matters.
    set_super(k);
    return;
  }
  if (k == NULL) {
    set_super(NULL);
    _primary_supers[0] = this;
    assert(super_depth() == 0, "Object must already be initialized properly");
  } else if (k != super() || k == SystemDictionary::Object_klass()) {
    assert(super() == NULL || super() == SystemDictionary::Object_klass(),
           "initialize this only once to a non-trivial value");
    set_super(k);
    Klass* sup = k;
    int sup_depth = sup->super_depth();
    juint my_depth  = MIN2(sup_depth + 1, (int)primary_super_limit());
    if (!can_be_primary_super_slow())
      my_depth = primary_super_limit();
    for (juint i = 0; i < my_depth; i++) {
      _primary_supers[i] = sup->_primary_supers[i];
    }
    Klass* *super_check_cell;
    if (my_depth < primary_super_limit()) {
      _primary_supers[my_depth] = this;
      super_check_cell = &_primary_supers[my_depth];
    } else {
      // Overflow of the primary_supers array forces me to be secondary.
      super_check_cell = &_secondary_super_cache;
    }
    set_super_check_offset((address)super_check_cell - (address) this);

#ifdef ASSERT
    {
      juint j = super_depth();
      assert(j == my_depth, "computed accessor gets right answer");
      Klass* t = this;
      while (!t->can_be_primary_super()) {
        t = t->super();
        j = t->super_depth();
      }
      for (juint j1 = j+1; j1 < primary_super_limit(); j1++) {
        assert(primary_super_of_depth(j1) == NULL, "super list padding");
      }
      while (t != NULL) {
        assert(primary_super_of_depth(j) == t, "super list initialization");
        t = t->super();
        --j;
      }
      assert(j == (juint)-1, "correct depth count");
    }
#endif
  }

  if (secondary_supers() == NULL) {
    KlassHandle this_kh (THREAD, this);

    // Now compute the list of secondary supertypes.
    // Secondaries can occasionally be on the super chain,
    // if the inline "_primary_supers" array overflows.
    int extras = 0;
    Klass* p;
    for (p = super(); !(p == NULL || p->can_be_primary_super()); p = p->super()) {
      ++extras;
    }

    ResourceMark rm(THREAD);  // need to reclaim GrowableArrays allocated below

    // Compute the "real" non-extra secondaries.
    GrowableArray<Klass*>* secondaries = compute_secondary_supers(extras);
    if (secondaries == NULL) {
      // secondary_supers set by compute_secondary_supers
      return;
    }

    GrowableArray<Klass*>* primaries = new GrowableArray<Klass*>(extras);

    for (p = this_kh->super(); !(p == NULL || p->can_be_primary_super()); p = p->super()) {
      int i;                    // Scan for overflow primaries being duplicates of 2nd'arys

      // This happens frequently for very deeply nested arrays: the
      // primary superclass chain overflows into the secondary.  The
      // secondary list contains the element_klass's secondaries with
      // an extra array dimension added.  If the element_klass's
      // secondary list already contains some primary overflows, they
      // (with the extra level of array-ness) will collide with the
      // normal primary superclass overflows.
      for( i = 0; i < secondaries->length(); i++ ) {
        if( secondaries->at(i) == p )
          break;
      }
      if( i < secondaries->length() )
        continue;               // It's a dup, don't put it in
      primaries->push(p);
    }
    // Combine the two arrays into a metadata object to pack the array.
    // The primaries are added in the reverse order, then the secondaries.
    int new_length = primaries->length() + secondaries->length();
    Array<Klass*>* s2 = MetadataFactory::new_array<Klass*>(
                                       class_loader_data(), new_length, CHECK);
    int fill_p = primaries->length();
    for (int j = 0; j < fill_p; j++) {
      s2->at_put(j, primaries->pop());  // add primaries in reverse order.
    }
    for( int j = 0; j < secondaries->length(); j++ ) {
      s2->at_put(j+fill_p, secondaries->at(j));  // add secondaries on the end.
    }

  #ifdef ASSERT
      // We must not copy any NULL placeholders left over from bootstrap.
    for (int j = 0; j < s2->length(); j++) {
      assert(s2->at(j) != NULL, "correct bootstrapping order");
    }
  #endif

    this_kh->set_secondary_supers(s2);
  }
}

GrowableArray<Klass*>* Klass::compute_secondary_supers(int num_extra_slots) {
  assert(num_extra_slots == 0, "override for complex klasses");
  set_secondary_supers(Universe::the_empty_klass_array());
  return NULL;
}


Klass* Klass::subklass() const {
  return _subklass == NULL ? NULL : _subklass;
}

InstanceKlass* Klass::superklass() const {
  assert(super() == NULL || super()->oop_is_instance(), "must be instance klass");
  return _super == NULL ? NULL : InstanceKlass::cast(_super);
}

Klass* Klass::next_sibling() const {
  return _next_sibling == NULL ? NULL : _next_sibling;
}

void Klass::set_subklass(Klass* s) {
  assert(s != this, "sanity check");
  _subklass = s;
}

void Klass::set_next_sibling(Klass* s) {
  assert(s != this, "sanity check");
  _next_sibling = s;
}

void Klass::append_to_sibling_list() {
  debug_only(verify();)
  // add ourselves to superklass' subklass list
  InstanceKlass* super = superklass();
  if (super == NULL) return;        // special case: class Object
  assert((!super->is_interface()    // interfaces cannot be supers
          && (super->superklass() == NULL || !is_interface())),
         "an interface can only be a subklass of Object");
  Klass* prev_first_subklass = super->subklass_oop();
  if (prev_first_subklass != NULL) {
    // set our sibling to be the superklass' previous first subklass
    set_next_sibling(prev_first_subklass);
  }
  // make ourselves the superklass' first subklass
  super->set_subklass(this);
  debug_only(verify();)
}

bool Klass::is_loader_alive(BoolObjectClosure* is_alive) {
#ifdef ASSERT
  // The class is alive iff the class loader is alive.
  oop loader = class_loader();
  bool loader_alive = (loader == NULL) || is_alive->do_object_b(loader);
#endif // ASSERT

  // The class is alive if it's mirror is alive (which should be marked if the
  // loader is alive) unless it's an anoymous class.
  bool mirror_alive = is_alive->do_object_b(java_mirror());
  assert(!mirror_alive || loader_alive, "loader must be alive if the mirror is"
                        " but not the other way around with anonymous classes");
  return mirror_alive;
}

void Klass::clean_weak_klass_links(BoolObjectClosure* is_alive, bool clean_alive_klasses) {
  if (!ClassUnloading) {
    return;
  }

  Klass* root = SystemDictionary::Object_klass();
  Stack<Klass*, mtGC> stack;

  stack.push(root);
  while (!stack.is_empty()) {
    Klass* current = stack.pop();

    assert(current->is_loader_alive(is_alive), "just checking, this should be live");

    // Find and set the first alive subklass
    Klass* sub = current->subklass_oop();
    while (sub != NULL && !sub->is_loader_alive(is_alive)) {
#ifndef PRODUCT
      if (TraceClassUnloading && WizardMode) {
        ResourceMark rm;
        tty->print_cr("[Unlinking class (subclass) %s]", sub->external_name());
      }
#endif
      sub = sub->next_sibling_oop();
    }
    current->set_subklass(sub);
    if (sub != NULL) {
      stack.push(sub);
    }

    // Find and set the first alive sibling
    Klass* sibling = current->next_sibling_oop();
    while (sibling != NULL && !sibling->is_loader_alive(is_alive)) {
      if (TraceClassUnloading && WizardMode) {
        ResourceMark rm;
        tty->print_cr("[Unlinking class (sibling) %s]", sibling->external_name());
      }
      sibling = sibling->next_sibling_oop();
    }
    current->set_next_sibling(sibling);
    if (sibling != NULL) {
      stack.push(sibling);
    }

    // Clean the implementors list and method data.
    if (clean_alive_klasses && current->oop_is_instance()) {
      InstanceKlass* ik = InstanceKlass::cast(current);
      ik->clean_weak_instanceklass_links(is_alive);

      // JVMTI RedefineClasses creates previous versions that are not in
      // the class hierarchy, so process them here.
      while ((ik = ik->previous_versions()) != NULL) {
        ik->clean_weak_instanceklass_links(is_alive);
      }
    }
  }
}

void Klass::klass_update_barrier_set(oop v) {
  record_modified_oops();
}

// This barrier is used by G1 to remember the old oop values, so
// that we don't forget any objects that were live at the snapshot at
// the beginning. This function is only used when we write oops into Klasses.
void Klass::klass_update_barrier_set_pre(oop* p, oop v) {
#if INCLUDE_ALL_GCS
  if (UseG1GC) {
    oop obj = *p;
    if (obj != NULL) {
      G1SATBCardTableModRefBS::enqueue(obj);
    }
  }
#endif
}

void Klass::klass_oop_store(oop* p, oop v) {
  assert(!Universe::heap()->is_in_reserved((void*)p), "Should store pointer into metadata");
  assert(v == NULL || Universe::heap()->is_in_reserved((void*)v), "Should store pointer to an object");

  // do the store
  if (always_do_update_barrier) {
    klass_oop_store((volatile oop*)p, v);
  } else {
    klass_update_barrier_set_pre(p, v);
    *p = v;
    klass_update_barrier_set(v);
  }
}

void Klass::klass_oop_store(volatile oop* p, oop v) {
  assert(!Universe::heap()->is_in_reserved((void*)p), "Should store pointer into metadata");
  assert(v == NULL || Universe::heap()->is_in_reserved((void*)v), "Should store pointer to an object");

  klass_update_barrier_set_pre((oop*)p, v); // Cast away volatile.
  OrderAccess::release_store_ptr(p, v);
  klass_update_barrier_set(v);
}

void Klass::oops_do(OopClosure* cl) {
  cl->do_oop(&_java_mirror);
}

void Klass::remove_java_mirror() {
  assert (DumpSharedSpaces, "only called for DumpSharedSpaces");
  if (TraceClassLoading) {
    ResourceMark rm;
    tty->print_cr("remove java_mirror: %s", external_name());
  }
  // Just null out the mirror. The class_loader_data() no longer exists.
  set_java_mirror(NULL);
}

void Klass::remove_unshareable_info() {
  assert (DumpSharedSpaces, "only called for DumpSharedSpaces");

  JFR_ONLY(REMOVE_ID(this);)
  set_subklass(NULL);
  set_next_sibling(NULL);
  // Clear the java mirror
  set_java_mirror(NULL);
  set_next_link(NULL);

  // Null out class_loader_data because we don't share that yet.
  set_class_loader_data(NULL);
  set_is_shared();
}

void Klass::restore_unshareable_info(ClassLoaderData* loader_data, Handle protection_domain, TRAPS) {
  JFR_ONLY(RESTORE_ID(this);)
  // If an exception happened during CDS restore, some of these fields may already be
  // set.  We leave the class on the CLD list, even if incomplete so that we don't
  // modify the CLD list outside a safepoint.
  if (class_loader_data() == NULL) {
    // Restore class_loader_data
    set_class_loader_data(loader_data);

    // Add to class loader list first before creating the mirror
    // (same order as class file parsing)
    loader_data->add_class(this);
  }

  // Recreate the class mirror.
  // Only recreate it if not present.  A previous attempt to restore may have
  // gotten an OOM later but keep the mirror if it was created.
  if (java_mirror() == NULL) {
    java_lang_Class::create_mirror(this, class_loader(), protection_domain, CHECK);
  }
}

Klass* Klass::array_klass_or_null(int rank) {
  EXCEPTION_MARK;
  // No exception can be thrown by array_klass_impl when called with or_null == true.
  // (In anycase, the execption mark will fail if it do so)
  return array_klass_impl(true, rank, THREAD);
}


Klass* Klass::array_klass_or_null() {
  EXCEPTION_MARK;
  // No exception can be thrown by array_klass_impl when called with or_null == true.
  // (In anycase, the execption mark will fail if it do so)
  return array_klass_impl(true, THREAD);
}


Klass* Klass::array_klass_impl(bool or_null, int rank, TRAPS) {
  fatal("array_klass should be dispatched to InstanceKlass, ObjArrayKlass or TypeArrayKlass");
  return NULL;
}


Klass* Klass::array_klass_impl(bool or_null, TRAPS) {
  fatal("array_klass should be dispatched to InstanceKlass, ObjArrayKlass or TypeArrayKlass");
  return NULL;
}

oop Klass::class_loader() const { return class_loader_data()->class_loader(); }

const char* Klass::external_name() const {
  if (oop_is_instance()) {
    InstanceKlass* ik = (InstanceKlass*) this;
    if (ik->is_anonymous()) {
      assert(EnableInvokeDynamic, "");
      intptr_t hash = 0;
      if (ik->java_mirror() != NULL) {
        // java_mirror might not be created yet, return 0 as hash.
        hash = ik->java_mirror()->identity_hash();
      }
      char     hash_buf[40];
      sprintf(hash_buf, "/" UINTX_FORMAT, (uintx)hash);
      size_t   hash_len = strlen(hash_buf);

      size_t result_len = name()->utf8_length();
      char*  result     = NEW_RESOURCE_ARRAY(char, result_len + hash_len + 1);
      name()->as_klass_external_name(result, (int) result_len + 1);
      assert(strlen(result) == result_len, "");
      strcpy(result + result_len, hash_buf);
      assert(strlen(result) == result_len + hash_len, "");
      return result;
    }
  }
  if (name() == NULL)  return "<unknown>";
  return name()->as_klass_external_name();
}


const char* Klass::signature_name() const {
  if (name() == NULL)  return "<unknown>";
  return name()->as_C_string();
}

// Unless overridden, modifier_flags is 0.
jint Klass::compute_modifier_flags(TRAPS) const {
  return 0;
}

int Klass::atomic_incr_biased_lock_revocation_count() {
  return (int) Atomic::add(1, &_biased_lock_revocation_count);
}

// Unless overridden, jvmti_class_status has no flags set.
jint Klass::jvmti_class_status() const {
  return 0;
}


// Printing

void Klass::print_on(outputStream* st) const {
  ResourceMark rm;
  // print title
  st->print("%s", internal_name());
  print_address_on(st);
  st->cr();
}

void Klass::oop_print_on(oop obj, outputStream* st) {
  ResourceMark rm;
  // print title
  st->print_cr("%s ", internal_name());
  obj->print_address_on(st);

  if (WizardMode) {
     // print header
     obj->mark()->print_on(st);
  }

  // print class
  st->print(" - klass: ");
  obj->klass()->print_value_on(st);
  st->cr();
}

void Klass::oop_print_value_on(oop obj, outputStream* st) {
  // print title
  ResourceMark rm;              // Cannot print in debug mode without this
  st->print("%s", internal_name());
  obj->print_address_on(st);
}

#if INCLUDE_SERVICES
// Size Statistics
void Klass::collect_statistics(KlassSizeStats *sz) const {
  sz->_klass_bytes = sz->count(this);
  sz->_mirror_bytes = sz->count(java_mirror());
  sz->_secondary_supers_bytes = sz->count_array(secondary_supers());

  sz->_ro_bytes += sz->_secondary_supers_bytes;
  sz->_rw_bytes += sz->_klass_bytes + sz->_mirror_bytes;
}
#endif // INCLUDE_SERVICES

vtableEntry* Klass::start_of_vtable() const {
  return (vtableEntry*) ((address)this + in_bytes(vtable_start_offset()));
}

ByteSize Klass::vtable_start_offset() {
  return in_ByteSize(InstanceKlass::header_size() * wordSize);
}

// Verification

void Klass::verify_on(outputStream* st) {

  // This can be expensive, but it is worth checking that this klass is actually
  // in the CLD graph but not in production.
  assert(Metaspace::contains((address)this), "Should be");

  guarantee(this->is_klass(),"should be klass");

  if (super() != NULL) {
    guarantee(super()->is_klass(), "should be klass");
  }
  if (secondary_super_cache() != NULL) {
    Klass* ko = secondary_super_cache();
    guarantee(ko->is_klass(), "should be klass");
  }
  for ( uint i = 0; i < primary_super_limit(); i++ ) {
    Klass* ko = _primary_supers[i];
    if (ko != NULL) {
      guarantee(ko->is_klass(), "should be klass");
    }
  }

  if (java_mirror() != NULL) {
    guarantee(java_mirror()->is_oop(), "should be instance");
  }
}

void Klass::oop_verify_on(oop obj, outputStream* st) {
  guarantee(obj->is_oop(),  "should be oop");
  guarantee(obj->klass()->is_klass(), "klass field is not a klass");
}

#ifndef PRODUCT

bool Klass::verify_vtable_index(int i) {
  if (oop_is_instance()) {
    int limit = ((InstanceKlass*)this)->vtable_length()/vtableEntry::size();
    assert(i >= 0 && i < limit, err_msg("index %d out of bounds %d", i, limit));
  } else {
    assert(oop_is_array(), "Must be");
    int limit = ((ArrayKlass*)this)->vtable_length()/vtableEntry::size();
    assert(i >= 0 && i < limit, err_msg("index %d out of bounds %d", i, limit));
  }
  return true;
}

bool Klass::verify_itable_index(int i) {
  assert(oop_is_instance(), "");
  int method_count = klassItable::method_count_for_interface(this);
  assert(i >= 0 && i < method_count, "index out of bounds");
  return true;
}

#endif

/////////////// Unit tests ///////////////

#ifndef PRODUCT

class TestKlass {
 public:
  static void test_oop_is_instanceClassLoader() {
    assert(SystemDictionary::ClassLoader_klass()->oop_is_instanceClassLoader(), "assert");
    assert(!SystemDictionary::String_klass()->oop_is_instanceClassLoader(), "assert");
  }
};

void TestKlass_test() {
  TestKlass::test_oop_is_instanceClassLoader();
}

#endif
