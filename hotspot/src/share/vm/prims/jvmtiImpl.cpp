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

#include "precompiled.hpp"
#include "classfile/systemDictionary.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/oopMapCache.hpp"
#include "jvmtifiles/jvmtiEnv.hpp"
#include "memory/resourceArea.hpp"
#include "oops/instanceKlass.hpp"
#include "prims/jvmtiAgentThread.hpp"
#include "prims/jvmtiEventController.inline.hpp"
#include "prims/jvmtiImpl.hpp"
#include "prims/jvmtiRedefineClasses.hpp"
#include "runtime/atomic.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/handles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/os.hpp"
#include "runtime/serviceThread.hpp"
#include "runtime/signature.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vframe.hpp"
#include "runtime/vframe_hp.hpp"
#include "runtime/vm_operations.hpp"
#include "utilities/exceptions.hpp"

//
// class JvmtiAgentThread
//
// JavaThread used to wrap a thread started by an agent
// using the JVMTI method RunAgentThread.
//

JvmtiAgentThread::JvmtiAgentThread(JvmtiEnv* env, jvmtiStartFunction start_fn, const void *start_arg)
    : JavaThread(start_function_wrapper) {
    _env = env;
    _start_fn = start_fn;
    _start_arg = start_arg;
}

void
JvmtiAgentThread::start_function_wrapper(JavaThread *thread, TRAPS) {
    // It is expected that any Agent threads will be created as
    // Java Threads.  If this is the case, notification of the creation
    // of the thread is given in JavaThread::thread_main().
    assert(thread->is_Java_thread(), "debugger thread should be a Java Thread");
    assert(thread == JavaThread::current(), "sanity check");

    JvmtiAgentThread *dthread = (JvmtiAgentThread *)thread;
    dthread->call_start_function();
}

void
JvmtiAgentThread::call_start_function() {
    ThreadToNativeFromVM transition(this);
    _start_fn(_env->jvmti_external(), jni_environment(), (void*)_start_arg);
}


//
// class GrowableCache - private methods
//

void GrowableCache::recache() {
  int len = _elements->length();

  FREE_C_HEAP_ARRAY(address, _cache, mtInternal);
  _cache = NEW_C_HEAP_ARRAY(address,len+1, mtInternal);

  for (int i=0; i<len; i++) {
    _cache[i] = _elements->at(i)->getCacheValue();
    //
    // The cache entry has gone bad. Without a valid frame pointer
    // value, the entry is useless so we simply delete it in product
    // mode. The call to remove() will rebuild the cache again
    // without the bad entry.
    //
    if (_cache[i] == NULL) {
      assert(false, "cannot recache NULL elements");
      remove(i);
      return;
    }
  }
  _cache[len] = NULL;

  _listener_fun(_this_obj,_cache);
}

bool GrowableCache::equals(void* v, GrowableElement *e2) {
  GrowableElement *e1 = (GrowableElement *) v;
  assert(e1 != NULL, "e1 != NULL");
  assert(e2 != NULL, "e2 != NULL");

  return e1->equals(e2);
}

//
// class GrowableCache - public methods
//

GrowableCache::GrowableCache() {
  _this_obj       = NULL;
  _listener_fun   = NULL;
  _elements       = NULL;
  _cache          = NULL;
}

GrowableCache::~GrowableCache() {
  clear();
  delete _elements;
  FREE_C_HEAP_ARRAY(address, _cache, mtInternal);
}

void GrowableCache::initialize(void *this_obj, void listener_fun(void *, address*) ) {
  _this_obj       = this_obj;
  _listener_fun   = listener_fun;
  _elements       = new (ResourceObj::C_HEAP, mtInternal) GrowableArray<GrowableElement*>(5,true);
  recache();
}

// number of elements in the collection
int GrowableCache::length() {
  return _elements->length();
}

// get the value of the index element in the collection
GrowableElement* GrowableCache::at(int index) {
  GrowableElement *e = (GrowableElement *) _elements->at(index);
  assert(e != NULL, "e != NULL");
  return e;
}

int GrowableCache::find(GrowableElement* e) {
  return _elements->find(e, GrowableCache::equals);
}

// append a copy of the element to the end of the collection
void GrowableCache::append(GrowableElement* e) {
  GrowableElement *new_e = e->clone();
  _elements->append(new_e);
  recache();
}

// insert a copy of the element using lessthan()
void GrowableCache::insert(GrowableElement* e) {
  GrowableElement *new_e = e->clone();
  _elements->append(new_e);

  int n = length()-2;
  for (int i=n; i>=0; i--) {
    GrowableElement *e1 = _elements->at(i);
    GrowableElement *e2 = _elements->at(i+1);
    if (e2->lessThan(e1)) {
      _elements->at_put(i+1, e1);
      _elements->at_put(i,   e2);
    }
  }

  recache();
}

// remove the element at index
void GrowableCache::remove (int index) {
  GrowableElement *e = _elements->at(index);
  assert(e != NULL, "e != NULL");
  _elements->remove(e);
  delete e;
  recache();
}

// clear out all elements, release all heap space and
// let our listener know that things have changed.
void GrowableCache::clear() {
  int len = _elements->length();
  for (int i=0; i<len; i++) {
    delete _elements->at(i);
  }
  _elements->clear();
  recache();
}

void GrowableCache::oops_do(OopClosure* f) {
  int len = _elements->length();
  for (int i=0; i<len; i++) {
    GrowableElement *e = _elements->at(i);
    e->oops_do(f);
  }
}

void GrowableCache::metadata_do(void f(Metadata*)) {
  int len = _elements->length();
  for (int i=0; i<len; i++) {
    GrowableElement *e = _elements->at(i);
    e->metadata_do(f);
  }
}

void GrowableCache::gc_epilogue() {
  int len = _elements->length();
  for (int i=0; i<len; i++) {
    _cache[i] = _elements->at(i)->getCacheValue();
  }
}

//
// class JvmtiBreakpoint
//

JvmtiBreakpoint::JvmtiBreakpoint() {
  _method = NULL;
  _bci    = 0;
  _class_holder = NULL;
}

JvmtiBreakpoint::JvmtiBreakpoint(Method* m_method, jlocation location) {
  _method        = m_method;
  _class_holder  = _method->method_holder()->klass_holder();
#ifdef CHECK_UNHANDLED_OOPS
  // _class_holder can't be wrapped in a Handle, because JvmtiBreakpoints are
  // sometimes allocated on the heap.
  //
  // The code handling JvmtiBreakpoints allocated on the stack can't be
  // interrupted by a GC until _class_holder is reachable by the GC via the
  // oops_do method.
  Thread::current()->allow_unhandled_oop(&_class_holder);
#endif // CHECK_UNHANDLED_OOPS
  assert(_method != NULL, "_method != NULL");
  _bci           = (int) location;
  assert(_bci >= 0, "_bci >= 0");
}

void JvmtiBreakpoint::copy(JvmtiBreakpoint& bp) {
  _method   = bp._method;
  _bci      = bp._bci;
  _class_holder = bp._class_holder;
}

bool JvmtiBreakpoint::lessThan(JvmtiBreakpoint& bp) {
  Unimplemented();
  return false;
}

bool JvmtiBreakpoint::equals(JvmtiBreakpoint& bp) {
  return _method   == bp._method
    &&   _bci      == bp._bci;
}

bool JvmtiBreakpoint::is_valid() {
  // class loader can be NULL
  return _method != NULL &&
         _bci >= 0;
}

address JvmtiBreakpoint::getBcp() {
  return _method->bcp_from(_bci);
}

void JvmtiBreakpoint::each_method_version_do(method_action meth_act) {
  ((Method*)_method->*meth_act)(_bci);

  // add/remove breakpoint to/from versions of the method that are EMCP.
  Thread *thread = Thread::current();
  instanceKlassHandle ikh = instanceKlassHandle(thread, _method->method_holder());
  Symbol* m_name = _method->name();
  Symbol* m_signature = _method->signature();

  // search previous versions if they exist
  for (InstanceKlass* pv_node = ikh->previous_versions();
       pv_node != NULL;
       pv_node = pv_node->previous_versions()) {
    Array<Method*>* methods = pv_node->methods();

    for (int i = methods->length() - 1; i >= 0; i--) {
      Method* method = methods->at(i);
      // Only set breakpoints in running EMCP methods.
      if (method->is_running_emcp() &&
          method->name() == m_name &&
          method->signature() == m_signature) {
        RC_TRACE(0x00000800, ("%sing breakpoint in %s(%s)",
          meth_act == &Method::set_breakpoint ? "sett" : "clear",
          method->name()->as_C_string(),
          method->signature()->as_C_string()));

        (method->*meth_act)(_bci);
        break;
      }
    }
  }
}

void JvmtiBreakpoint::set() {
  each_method_version_do(&Method::set_breakpoint);
}

void JvmtiBreakpoint::clear() {
  each_method_version_do(&Method::clear_breakpoint);
}

void JvmtiBreakpoint::print() {
#ifndef PRODUCT
  const char *class_name  = (_method == NULL) ? "NULL" : _method->klass_name()->as_C_string();
  const char *method_name = (_method == NULL) ? "NULL" : _method->name()->as_C_string();

  tty->print("Breakpoint(%s,%s,%d,%p)",class_name, method_name, _bci, getBcp());
#endif
}


//
// class VM_ChangeBreakpoints
//
// Modify the Breakpoints data structure at a safepoint
//

void VM_ChangeBreakpoints::doit() {
  switch (_operation) {
  case SET_BREAKPOINT:
    _breakpoints->set_at_safepoint(*_bp);
    break;
  case CLEAR_BREAKPOINT:
    _breakpoints->clear_at_safepoint(*_bp);
    break;
  default:
    assert(false, "Unknown operation");
  }
}

void VM_ChangeBreakpoints::oops_do(OopClosure* f) {
  // The JvmtiBreakpoints in _breakpoints will be visited via
  // JvmtiExport::oops_do.
  if (_bp != NULL) {
    _bp->oops_do(f);
  }
}

void VM_ChangeBreakpoints::metadata_do(void f(Metadata*)) {
  // Walk metadata in breakpoints to keep from being deallocated with RedefineClasses
  if (_bp != NULL) {
    _bp->metadata_do(f);
  }
}

//
// class JvmtiBreakpoints
//
// a JVMTI internal collection of JvmtiBreakpoint
//

JvmtiBreakpoints::JvmtiBreakpoints(void listener_fun(void *,address *)) {
  _bps.initialize(this,listener_fun);
}

JvmtiBreakpoints:: ~JvmtiBreakpoints() {}

void  JvmtiBreakpoints::oops_do(OopClosure* f) {
  _bps.oops_do(f);
}

void  JvmtiBreakpoints::metadata_do(void f(Metadata*)) {
  _bps.metadata_do(f);
}

void JvmtiBreakpoints::gc_epilogue() {
  _bps.gc_epilogue();
}

void  JvmtiBreakpoints::print() {
#ifndef PRODUCT
  ResourceMark rm;

  int n = _bps.length();
  for (int i=0; i<n; i++) {
    JvmtiBreakpoint& bp = _bps.at(i);
    tty->print("%d: ", i);
    bp.print();
    tty->cr();
  }
#endif
}


void JvmtiBreakpoints::set_at_safepoint(JvmtiBreakpoint& bp) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");

  int i = _bps.find(bp);
  if (i == -1) {
    _bps.append(bp);
    bp.set();
  }
}

void JvmtiBreakpoints::clear_at_safepoint(JvmtiBreakpoint& bp) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");

  int i = _bps.find(bp);
  if (i != -1) {
    _bps.remove(i);
    bp.clear();
  }
}

int JvmtiBreakpoints::length() { return _bps.length(); }

int JvmtiBreakpoints::set(JvmtiBreakpoint& bp) {
  if ( _bps.find(bp) != -1) {
     return JVMTI_ERROR_DUPLICATE;
  }
  VM_ChangeBreakpoints set_breakpoint(VM_ChangeBreakpoints::SET_BREAKPOINT, &bp);
  VMThread::execute(&set_breakpoint);
  return JVMTI_ERROR_NONE;
}

int JvmtiBreakpoints::clear(JvmtiBreakpoint& bp) {
  if ( _bps.find(bp) == -1) {
     return JVMTI_ERROR_NOT_FOUND;
  }

  VM_ChangeBreakpoints clear_breakpoint(VM_ChangeBreakpoints::CLEAR_BREAKPOINT, &bp);
  VMThread::execute(&clear_breakpoint);
  return JVMTI_ERROR_NONE;
}

void JvmtiBreakpoints::clearall_in_class_at_safepoint(Klass* klass) {
  bool changed = true;
  // We are going to run thru the list of bkpts
  // and delete some.  This deletion probably alters
  // the list in some implementation defined way such
  // that when we delete entry i, the next entry might
  // no longer be at i+1.  To be safe, each time we delete
  // an entry, we'll just start again from the beginning.
  // We'll stop when we make a pass thru the whole list without
  // deleting anything.
  while (changed) {
    int len = _bps.length();
    changed = false;
    for (int i = 0; i < len; i++) {
      JvmtiBreakpoint& bp = _bps.at(i);
      if (bp.method()->method_holder() == klass) {
        bp.clear();
        _bps.remove(i);
        // This changed 'i' so we have to start over.
        changed = true;
        break;
      }
    }
  }
}

//
// class JvmtiCurrentBreakpoints
//

JvmtiBreakpoints *JvmtiCurrentBreakpoints::_jvmti_breakpoints  = NULL;
address *         JvmtiCurrentBreakpoints::_breakpoint_list    = NULL;


JvmtiBreakpoints& JvmtiCurrentBreakpoints::get_jvmti_breakpoints() {
  if (_jvmti_breakpoints != NULL) return (*_jvmti_breakpoints);
  _jvmti_breakpoints = new JvmtiBreakpoints(listener_fun);
  assert(_jvmti_breakpoints != NULL, "_jvmti_breakpoints != NULL");
  return (*_jvmti_breakpoints);
}

void  JvmtiCurrentBreakpoints::listener_fun(void *this_obj, address *cache) {
  JvmtiBreakpoints *this_jvmti = (JvmtiBreakpoints *) this_obj;
  assert(this_jvmti != NULL, "this_jvmti != NULL");

  debug_only(int n = this_jvmti->length(););
  assert(cache[n] == NULL, "cache must be NULL terminated");

  set_breakpoint_list(cache);
}


void JvmtiCurrentBreakpoints::oops_do(OopClosure* f) {
  if (_jvmti_breakpoints != NULL) {
    _jvmti_breakpoints->oops_do(f);
  }
}

void JvmtiCurrentBreakpoints::metadata_do(void f(Metadata*)) {
  if (_jvmti_breakpoints != NULL) {
    _jvmti_breakpoints->metadata_do(f);
  }
}

void JvmtiCurrentBreakpoints::gc_epilogue() {
  if (_jvmti_breakpoints != NULL) {
    _jvmti_breakpoints->gc_epilogue();
  }
}

///////////////////////////////////////////////////////////////
//
// class VM_GetOrSetLocal
//

// Constructor for non-object getter
VM_GetOrSetLocal::VM_GetOrSetLocal(JavaThread* thread, jint depth, int index, BasicType type)
  : _thread(thread)
  , _calling_thread(NULL)
  , _depth(depth)
  , _index(index)
  , _type(type)
  , _set(false)
  , _jvf(NULL)
  , _result(JVMTI_ERROR_NONE)
{
}

// Constructor for object or non-object setter
VM_GetOrSetLocal::VM_GetOrSetLocal(JavaThread* thread, jint depth, int index, BasicType type, jvalue value)
  : _thread(thread)
  , _calling_thread(NULL)
  , _depth(depth)
  , _index(index)
  , _type(type)
  , _value(value)
  , _set(true)
  , _jvf(NULL)
  , _result(JVMTI_ERROR_NONE)
{
}

// Constructor for object getter
VM_GetOrSetLocal::VM_GetOrSetLocal(JavaThread* thread, JavaThread* calling_thread, jint depth, int index)
  : _thread(thread)
  , _calling_thread(calling_thread)
  , _depth(depth)
  , _index(index)
  , _type(T_OBJECT)
  , _set(false)
  , _jvf(NULL)
  , _result(JVMTI_ERROR_NONE)
{
}

vframe *VM_GetOrSetLocal::get_vframe() {
  if (!_thread->has_last_Java_frame()) {
    return NULL;
  }
  RegisterMap reg_map(_thread);
  vframe *vf = _thread->last_java_vframe(&reg_map);
  int d = 0;
  while ((vf != NULL) && (d < _depth)) {
    vf = vf->java_sender();
    d++;
  }
  return vf;
}

javaVFrame *VM_GetOrSetLocal::get_java_vframe() {
  vframe* vf = get_vframe();
  if (vf == NULL) {
    _result = JVMTI_ERROR_NO_MORE_FRAMES;
    return NULL;
  }
  javaVFrame *jvf = (javaVFrame*)vf;

  if (!vf->is_java_frame()) {
    _result = JVMTI_ERROR_OPAQUE_FRAME;
    return NULL;
  }
  return jvf;
}

// Check that the klass is assignable to a type with the given signature.
// Another solution could be to use the function Klass::is_subtype_of(type).
// But the type class can be forced to load/initialize eagerly in such a case.
// This may cause unexpected consequences like CFLH or class-init JVMTI events.
// It is better to avoid such a behavior.
bool VM_GetOrSetLocal::is_assignable(const char* ty_sign, Klass* klass, Thread* thread) {
  assert(ty_sign != NULL, "type signature must not be NULL");
  assert(thread != NULL, "thread must not be NULL");
  assert(klass != NULL, "klass must not be NULL");

  int len = (int) strlen(ty_sign);
  if (ty_sign[0] == 'L' && ty_sign[len-1] == ';') { // Need pure class/interface name
    ty_sign++;
    len -= 2;
  }
  TempNewSymbol ty_sym = SymbolTable::new_symbol(ty_sign, len, thread);
  if (klass->name() == ty_sym) {
    return true;
  }
  // Compare primary supers
  int super_depth = klass->super_depth();
  int idx;
  for (idx = 0; idx < super_depth; idx++) {
    if (klass->primary_super_of_depth(idx)->name() == ty_sym) {
      return true;
    }
  }
  // Compare secondary supers
  Array<Klass*>* sec_supers = klass->secondary_supers();
  for (idx = 0; idx < sec_supers->length(); idx++) {
    if (((Klass*) sec_supers->at(idx))->name() == ty_sym) {
      return true;
    }
  }
  return false;
}

// Checks error conditions:
//   JVMTI_ERROR_INVALID_SLOT
//   JVMTI_ERROR_TYPE_MISMATCH
// Returns: 'true' - everything is Ok, 'false' - error code

bool VM_GetOrSetLocal::check_slot_type(javaVFrame* jvf) {
  Method* method_oop = jvf->method();
  if (!method_oop->has_localvariable_table()) {
    // Just to check index boundaries
    jint extra_slot = (_type == T_LONG || _type == T_DOUBLE) ? 1 : 0;
    if (_index < 0 || _index + extra_slot >= method_oop->max_locals()) {
      _result = JVMTI_ERROR_INVALID_SLOT;
      return false;
    }
    return true;
  }

  jint num_entries = method_oop->localvariable_table_length();
  if (num_entries == 0) {
    _result = JVMTI_ERROR_INVALID_SLOT;
    return false;       // There are no slots
  }
  int signature_idx = -1;
  int vf_bci = jvf->bci();
  LocalVariableTableElement* table = method_oop->localvariable_table_start();
  for (int i = 0; i < num_entries; i++) {
    int start_bci = table[i].start_bci;
    int end_bci = start_bci + table[i].length;

    // Here we assume that locations of LVT entries
    // with the same slot number cannot be overlapped
    if (_index == (jint) table[i].slot && start_bci <= vf_bci && vf_bci <= end_bci) {
      signature_idx = (int) table[i].descriptor_cp_index;
      break;
    }
  }
  if (signature_idx == -1) {
    _result = JVMTI_ERROR_INVALID_SLOT;
    return false;       // Incorrect slot index
  }
  Symbol*   sign_sym  = method_oop->constants()->symbol_at(signature_idx);
  const char* signature = (const char *) sign_sym->as_utf8();
  BasicType slot_type = char2type(signature[0]);

  switch (slot_type) {
  case T_BYTE:
  case T_SHORT:
  case T_CHAR:
  case T_BOOLEAN:
    slot_type = T_INT;
    break;
  case T_ARRAY:
    slot_type = T_OBJECT;
    break;
  };
  if (_type != slot_type) {
    _result = JVMTI_ERROR_TYPE_MISMATCH;
    return false;
  }

  jobject jobj = _value.l;
  if (_set && slot_type == T_OBJECT && jobj != NULL) { // NULL reference is allowed
    // Check that the jobject class matches the return type signature.
    JavaThread* cur_thread = JavaThread::current();
    HandleMark hm(cur_thread);

    Handle obj = Handle(cur_thread, JNIHandles::resolve_external_guard(jobj));
    NULL_CHECK(obj, (_result = JVMTI_ERROR_INVALID_OBJECT, false));
    KlassHandle ob_kh = KlassHandle(cur_thread, obj->klass());
    NULL_CHECK(ob_kh, (_result = JVMTI_ERROR_INVALID_OBJECT, false));

    if (!is_assignable(signature, ob_kh(), cur_thread)) {
      _result = JVMTI_ERROR_TYPE_MISMATCH;
      return false;
    }
  }
  return true;
}

static bool can_be_deoptimized(vframe* vf) {
  return (vf->is_compiled_frame() && vf->fr().can_be_deoptimized());
}

bool VM_GetOrSetLocal::doit_prologue() {
  _jvf = get_java_vframe();
  NULL_CHECK(_jvf, false);

  if (_jvf->method()->is_native()) {
    if (getting_receiver() && !_jvf->method()->is_static()) {
      return true;
    } else {
      _result = JVMTI_ERROR_OPAQUE_FRAME;
      return false;
    }
  }

  if (!check_slot_type(_jvf)) {
    return false;
  }
  return true;
}

void VM_GetOrSetLocal::doit() {
  InterpreterOopMap oop_mask;
  _jvf->method()->mask_for(_jvf->bci(), &oop_mask);
  if (oop_mask.is_dead(_index)) {
    // The local can be invalid and uninitialized in the scope of current bci
    _result = JVMTI_ERROR_INVALID_SLOT;
    return;
  }
  if (_set) {
    // Force deoptimization of frame if compiled because it's
    // possible the compiler emitted some locals as constant values,
    // meaning they are not mutable.
    if (can_be_deoptimized(_jvf)) {

      // Schedule deoptimization so that eventually the local
      // update will be written to an interpreter frame.
      Deoptimization::deoptimize_frame(_jvf->thread(), _jvf->fr().id());

      // Now store a new value for the local which will be applied
      // once deoptimization occurs. Note however that while this
      // write is deferred until deoptimization actually happens
      // can vframe created after this point will have its locals
      // reflecting this update so as far as anyone can see the
      // write has already taken place.

      // If we are updating an oop then get the oop from the handle
      // since the handle will be long gone by the time the deopt
      // happens. The oop stored in the deferred local will be
      // gc'd on its own.
      if (_type == T_OBJECT) {
        _value.l = (jobject) (JNIHandles::resolve_external_guard(_value.l));
      }
      // Re-read the vframe so we can see that it is deoptimized
      // [ Only need because of assert in update_local() ]
      _jvf = get_java_vframe();
      ((compiledVFrame*)_jvf)->update_local(_type, _index, _value);
      return;
    }
    StackValueCollection *locals = _jvf->locals();
    HandleMark hm;

    switch (_type) {
      case T_INT:    locals->set_int_at   (_index, _value.i); break;
      case T_LONG:   locals->set_long_at  (_index, _value.j); break;
      case T_FLOAT:  locals->set_float_at (_index, _value.f); break;
      case T_DOUBLE: locals->set_double_at(_index, _value.d); break;
      case T_OBJECT: {
        Handle ob_h(JNIHandles::resolve_external_guard(_value.l));
        locals->set_obj_at (_index, ob_h);
        break;
      }
      default: ShouldNotReachHere();
    }
    _jvf->set_locals(locals);
  } else {
    if (_jvf->method()->is_native() && _jvf->is_compiled_frame()) {
      assert(getting_receiver(), "Can only get here when getting receiver");
      oop receiver = _jvf->fr().get_native_receiver();
      _value.l = JNIHandles::make_local(_calling_thread, receiver);
    } else {
      StackValueCollection *locals = _jvf->locals();

      if (locals->at(_index)->type() == T_CONFLICT) {
        memset(&_value, 0, sizeof(_value));
        _value.l = NULL;
        return;
      }

      switch (_type) {
        case T_INT:    _value.i = locals->int_at   (_index);   break;
        case T_LONG:   _value.j = locals->long_at  (_index);   break;
        case T_FLOAT:  _value.f = locals->float_at (_index);   break;
        case T_DOUBLE: _value.d = locals->double_at(_index);   break;
        case T_OBJECT: {
          // Wrap the oop to be returned in a local JNI handle since
          // oops_do() no longer applies after doit() is finished.
          oop obj = locals->obj_at(_index)();
          _value.l = JNIHandles::make_local(_calling_thread, obj);
          break;
        }
        default: ShouldNotReachHere();
      }
    }
  }
}


bool VM_GetOrSetLocal::allow_nested_vm_operations() const {
  return true; // May need to deoptimize
}


VM_GetReceiver::VM_GetReceiver(
    JavaThread* thread, JavaThread* caller_thread, jint depth)
    : VM_GetOrSetLocal(thread, caller_thread, depth, 0) {}

/////////////////////////////////////////////////////////////////////////////////////////

//
// class JvmtiSuspendControl - see comments in jvmtiImpl.hpp
//

bool JvmtiSuspendControl::suspend(JavaThread *java_thread) {
  // external suspend should have caught suspending a thread twice

  // Immediate suspension required for JPDA back-end so JVMTI agent threads do
  // not deadlock due to later suspension on transitions while holding
  // raw monitors.  Passing true causes the immediate suspension.
  // java_suspend() will catch threads in the process of exiting
  // and will ignore them.
  java_thread->java_suspend();

  // It would be nice to have the following assertion in all the time,
  // but it is possible for a racing resume request to have resumed
  // this thread right after we suspended it. Temporarily enable this
  // assertion if you are chasing a different kind of bug.
  //
  // assert(java_lang_Thread::thread(java_thread->threadObj()) == NULL ||
  //   java_thread->is_being_ext_suspended(), "thread is not suspended");

  if (java_lang_Thread::thread(java_thread->threadObj()) == NULL) {
    // check again because we can get delayed in java_suspend():
    // the thread is in process of exiting.
    return false;
  }

  return true;
}

bool JvmtiSuspendControl::resume(JavaThread *java_thread) {
  // external suspend should have caught resuming a thread twice
  assert(java_thread->is_being_ext_suspended(), "thread should be suspended");

  // resume thread
  {
    // must always grab Threads_lock, see JVM_SuspendThread
    MutexLocker ml(Threads_lock);
    java_thread->java_resume();
  }

  return true;
}


void JvmtiSuspendControl::print() {
#ifndef PRODUCT
  MutexLocker mu(Threads_lock);
  ResourceMark rm;

  tty->print("Suspended Threads: [");
  for (JavaThread *thread = Threads::first(); thread != NULL; thread = thread->next()) {
#ifdef JVMTI_TRACE
    const char *name   = JvmtiTrace::safe_get_thread_name(thread);
#else
    const char *name   = "";
#endif /*JVMTI_TRACE */
    tty->print("%s(%c ", name, thread->is_being_ext_suspended() ? 'S' : '_');
    if (!thread->has_last_Java_frame()) {
      tty->print("no stack");
    }
    tty->print(") ");
  }
  tty->print_cr("]");
#endif
}

JvmtiDeferredEvent JvmtiDeferredEvent::compiled_method_load_event(
    nmethod* nm) {
  JvmtiDeferredEvent event = JvmtiDeferredEvent(TYPE_COMPILED_METHOD_LOAD);
  event._event_data.compiled_method_load = nm;
  return event;
}

JvmtiDeferredEvent JvmtiDeferredEvent::compiled_method_unload_event(
    nmethod* nm, jmethodID id, const void* code) {
  JvmtiDeferredEvent event = JvmtiDeferredEvent(TYPE_COMPILED_METHOD_UNLOAD);
  event._event_data.compiled_method_unload.nm = nm;
  event._event_data.compiled_method_unload.method_id = id;
  event._event_data.compiled_method_unload.code_begin = code;
  // Keep the nmethod alive until the ServiceThread can process
  // this deferred event. This will keep the memory for the
  // generated code from being reused too early. We pass
  // zombie_ok == true here so that our nmethod that was just
  // made into a zombie can be locked.
  nmethodLocker::lock_nmethod(nm, true /* zombie_ok */);
  return event;
}

JvmtiDeferredEvent JvmtiDeferredEvent::dynamic_code_generated_event(
      const char* name, const void* code_begin, const void* code_end) {
  JvmtiDeferredEvent event = JvmtiDeferredEvent(TYPE_DYNAMIC_CODE_GENERATED);
  // Need to make a copy of the name since we don't know how long
  // the event poster will keep it around after we enqueue the
  // deferred event and return. strdup() failure is handled in
  // the post() routine below.
  event._event_data.dynamic_code_generated.name = os::strdup(name);
  event._event_data.dynamic_code_generated.code_begin = code_begin;
  event._event_data.dynamic_code_generated.code_end = code_end;
  return event;
}

void JvmtiDeferredEvent::post() {
  assert(Thread::current()->is_service_thread(),
         "Service thread must post enqueued events");
  switch(_type) {
    case TYPE_COMPILED_METHOD_LOAD: {
      nmethod* nm = _event_data.compiled_method_load;
      JvmtiExport::post_compiled_method_load(nm);
      break;
    }
    case TYPE_COMPILED_METHOD_UNLOAD: {
      nmethod* nm = _event_data.compiled_method_unload.nm;
      JvmtiExport::post_compiled_method_unload(
        _event_data.compiled_method_unload.method_id,
        _event_data.compiled_method_unload.code_begin);
      // done with the deferred event so unlock the nmethod
      nmethodLocker::unlock_nmethod(nm);
      break;
    }
    case TYPE_DYNAMIC_CODE_GENERATED: {
      JvmtiExport::post_dynamic_code_generated_internal(
        // if strdup failed give the event a default name
        (_event_data.dynamic_code_generated.name == NULL)
          ? "unknown_code" : _event_data.dynamic_code_generated.name,
        _event_data.dynamic_code_generated.code_begin,
        _event_data.dynamic_code_generated.code_end);
      if (_event_data.dynamic_code_generated.name != NULL) {
        // release our copy
        os::free((void *)_event_data.dynamic_code_generated.name);
      }
      break;
    }
    default:
      ShouldNotReachHere();
  }
}

// Keep the nmethod for compiled_method_load from being unloaded.
void JvmtiDeferredEvent::oops_do(OopClosure* f, CodeBlobClosure* cf) {
  if (cf != NULL && _type == TYPE_COMPILED_METHOD_LOAD) {
    cf->do_code_blob(_event_data.compiled_method_load);
  }
}

// The sweeper calls this and marks the nmethods here on the stack so that
// they cannot be turned into zombies while in the queue.
void JvmtiDeferredEvent::nmethods_do(CodeBlobClosure* cf) {
  if (cf != NULL && _type == TYPE_COMPILED_METHOD_LOAD) {
    cf->do_code_blob(_event_data.compiled_method_load);
  }  // May add UNLOAD event but it doesn't work yet.
}

JvmtiDeferredEventQueue::QueueNode* JvmtiDeferredEventQueue::_queue_tail = NULL;
JvmtiDeferredEventQueue::QueueNode* JvmtiDeferredEventQueue::_queue_head = NULL;

volatile JvmtiDeferredEventQueue::QueueNode*
    JvmtiDeferredEventQueue::_pending_list = NULL;

bool JvmtiDeferredEventQueue::has_events() {
  assert(Service_lock->owned_by_self(), "Must own Service_lock");
  return _queue_head != NULL || _pending_list != NULL;
}

void JvmtiDeferredEventQueue::enqueue(const JvmtiDeferredEvent& event) {
  assert(Service_lock->owned_by_self(), "Must own Service_lock");

  process_pending_events();

  // Events get added to the end of the queue (and are pulled off the front).
  QueueNode* node = new QueueNode(event);
  if (_queue_tail == NULL) {
    _queue_tail = _queue_head = node;
  } else {
    assert(_queue_tail->next() == NULL, "Must be the last element in the list");
    _queue_tail->set_next(node);
    _queue_tail = node;
  }

  Service_lock->notify_all();
  assert((_queue_head == NULL) == (_queue_tail == NULL),
         "Inconsistent queue markers");
}

JvmtiDeferredEvent JvmtiDeferredEventQueue::dequeue() {
  assert(Service_lock->owned_by_self(), "Must own Service_lock");

  process_pending_events();

  assert(_queue_head != NULL, "Nothing to dequeue");

  if (_queue_head == NULL) {
    // Just in case this happens in product; it shouldn't but let's not crash
    return JvmtiDeferredEvent();
  }

  QueueNode* node = _queue_head;
  _queue_head = _queue_head->next();
  if (_queue_head == NULL) {
    _queue_tail = NULL;
  }

  assert((_queue_head == NULL) == (_queue_tail == NULL),
         "Inconsistent queue markers");

  JvmtiDeferredEvent event = node->event();
  delete node;
  return event;
}

void JvmtiDeferredEventQueue::add_pending_event(
    const JvmtiDeferredEvent& event) {

  QueueNode* node = new QueueNode(event);

  bool success = false;
  QueueNode* prev_value = (QueueNode*)_pending_list;
  do {
    node->set_next(prev_value);
    prev_value = (QueueNode*)Atomic::cmpxchg_ptr(
        (void*)node, (volatile void*)&_pending_list, (void*)node->next());
  } while (prev_value != node->next());
}

// This method transfers any events that were added by someone NOT holding
// the lock into the mainline queue.
void JvmtiDeferredEventQueue::process_pending_events() {
  assert(Service_lock->owned_by_self(), "Must own Service_lock");

  if (_pending_list != NULL) {
    QueueNode* head =
        (QueueNode*)Atomic::xchg_ptr(NULL, (volatile void*)&_pending_list);

    assert((_queue_head == NULL) == (_queue_tail == NULL),
           "Inconsistent queue markers");

    if (head != NULL) {
      // Since we've treated the pending list as a stack (with newer
      // events at the beginning), we need to join the bottom of the stack
      // with the 'tail' of the queue in order to get the events in the
      // right order.  We do this by reversing the pending list and appending
      // it to the queue.

      QueueNode* new_tail = head;
      QueueNode* new_head = NULL;

      // This reverses the list
      QueueNode* prev = new_tail;
      QueueNode* node = new_tail->next();
      new_tail->set_next(NULL);
      while (node != NULL) {
        QueueNode* next = node->next();
        node->set_next(prev);
        prev = node;
        node = next;
      }
      new_head = prev;

      // Now append the new list to the queue
      if (_queue_tail != NULL) {
        _queue_tail->set_next(new_head);
      } else { // _queue_head == NULL
        _queue_head = new_head;
      }
      _queue_tail = new_tail;
    }
  }
}

void JvmtiDeferredEventQueue::oops_do(OopClosure* f, CodeBlobClosure* cf) {
  for(QueueNode* node = _queue_head; node != NULL; node = node->next()) {
     node->event().oops_do(f, cf);
  }
}

void JvmtiDeferredEventQueue::nmethods_do(CodeBlobClosure* cf) {
  for(QueueNode* node = _queue_head; node != NULL; node = node->next()) {
     node->event().nmethods_do(cf);
  }
}
