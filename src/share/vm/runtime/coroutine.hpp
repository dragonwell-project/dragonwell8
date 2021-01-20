/*
 * Copyright 1999-2010 Sun Microsystems, Inc.  All Rights Reserved.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

#ifndef SHARE_VM_RUNTIME_COROUTINE_HPP
#define SHARE_VM_RUNTIME_COROUTINE_HPP

#include "runtime/jniHandles.hpp"
#include "runtime/handles.hpp"
#include "memory/allocation.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/javaFrameAnchor.hpp"
#include "runtime/monitorChunk.hpp"
#include "runtime/thread.hpp"

// number of heap words that prepareSwitch will add as a safety measure to the CoroutineData size
#define COROUTINE_DATA_OVERSIZE (64)

//#define DEBUG_COROUTINES

#ifdef DEBUG_COROUTINES
#define DEBUG_CORO_ONLY(x) x
#define DEBUG_CORO_PRINT(x) tty->print(x)
#else
#define DEBUG_CORO_ONLY(x)
#define DEBUG_CORO_PRINT(x)
#endif

class Coroutine;
class CoroutineStack;
class WispThread;


template<class T>
class DoublyLinkedList {
private:
  T*  _last;
  T*  _next;

public:
  DoublyLinkedList() {
    _last = NULL;
    _next = NULL;
  }

  typedef T* pointer;

  void remove_from_list(pointer& list);
  void insert_into_list(pointer& list);
  static void move(pointer& coro, pointer& target);

  T* last() const   { return _last; }
  T* next() const   { return _next; }
};

class FrameClosure: public StackObj {
public:
  virtual void frames_do(frame* fr, RegisterMap* map) = 0;
};

class WispPostStealHandleUpdateMark;
class WispResourceArea;

class Coroutine: public CHeapObj<mtCoroutine>, public DoublyLinkedList<Coroutine> {
public:
  enum CoroutineState {
    _created    = 0x00000000,      // not inited
    _onstack    = 0x00000001,
    _current    = 0x00000002,
    _dead       = 0x00000003,      // TODO is this really needed?
    _dummy      = 0xffffffff
  };
  enum Consts {
    WISP_ID_NOT_SET = -1,
  };

private:
  CoroutineState  _state;

  bool            _is_thread_coroutine;

  JavaThread*     _thread;
  CoroutineStack* _stack;

  int             _wisp_task_id;
  oop             _wisp_engine;
  oop             _wisp_task;
  oop             _coroutine;
  WispThread*     _wisp_thread;

  ResourceArea*   _resource_area;
  HandleArea*     _handle_area;
  HandleMark*     _last_handle_mark;
  JNIHandleBlock* _active_handles;
  GrowableArray<Metadata*>* _metadata_handles;
  JavaFrameAnchor _anchor;
  PrivilegedElement*  _privileged_stack_top;
  java_lang_Thread::ThreadStatus _thread_status;
  int             _enable_steal_count;
  int             _java_call_counter;
  int             _last_native_call_counter;
  int             _clinit_call_counter;
  volatile int    _native_call_counter;

  // work steal pool
  WispResourceArea*       _wisp_post_steal_resource_area;
  bool            _is_yielding;

#ifdef _LP64
  intptr_t        _storage[2];
#endif

  // objects of this type can only be created via static functions
  Coroutine() { }

  void frames_do(FrameClosure* fc);
  bool is_coroutine_frame(javaVFrame* jvf);
  bool in_critical(JavaThread* thread);
  static void set_coroutine_base(intptr_t **&base, JavaThread* thread, jobject obj, Coroutine *coro, oop coroutineObj, address coroutine_start);
public:
  virtual ~Coroutine();

  void run(jobject coroutine);

  static void initialize_coroutine_support(JavaThread* thread);
  static Coroutine* create_thread_coroutine(JavaThread* thread, CoroutineStack* stack);
  static Coroutine* create_coroutine(JavaThread* thread, CoroutineStack* stack, oop coroutineObj);
  static void after_safepoint(JavaThread* thread);

  CoroutineState state() const      { return _state; }
  void set_state(CoroutineState x)  { _state = x; }

  bool is_thread_coroutine() const  { return _is_thread_coroutine; }
  bool is_yielding() const          { return _is_yielding; }

  JavaThread* thread() const        { return _thread; }
  void set_thread(JavaThread* x)    { _thread = x; }

  CoroutineStack* stack() const     { return _stack; }

  int wisp_task_id() const          { return _wisp_task_id; }
  void set_wisp_task_id(int x)      { _wisp_task_id = x; }

  oop wisp_engine() const           { return _wisp_engine; }
  void set_wisp_engine(oop x);

  oop wisp_task() const             { return _wisp_task; }
  void set_wisp_task(oop x)         { _wisp_task = x;    }

  oop coroutine() const             { return _coroutine; }
  void set_coroutine(oop x)         { _coroutine = x;    }

  WispThread* wisp_thread() const   { return _wisp_thread; }

  ResourceArea* resource_area() const     { return _resource_area; }
  void set_resource_area(ResourceArea* x) { _resource_area = x; }

  // work steal support
  WispResourceArea* wisp_post_steal_resource_area() const  { return _wisp_post_steal_resource_area; }
  void change_thread_for_wisp(JavaThread *thread);

  HandleArea* handle_area() const         { return _handle_area; }
  void set_handle_area(HandleArea* x)     { _handle_area = x; }

  HandleMark* last_handle_mark() const    { return _last_handle_mark; }
  void set_last_handle_mark(HandleMark* x){ _last_handle_mark = x; }

  JNIHandleBlock* active_handles() const         { return _active_handles; }
  void set_active_handles(JNIHandleBlock* block) { _active_handles = block; }

  GrowableArray<Metadata*>* metadata_handles() const          { return _metadata_handles; }
  void set_metadata_handles(GrowableArray<Metadata*>* handles){ _metadata_handles = handles; }

  int enable_steal_count()                { return _enable_steal_count; }
  int add_enable_steal_count()            { return ++_enable_steal_count; }
  int dec_enable_steal_count()            { return _enable_steal_count--; }

  int clinit_call_count()                  { return _clinit_call_counter; }
  int inc_clinit_call_count()              { return _clinit_call_counter++; }
  int dec_clinit_call_count()              { return _clinit_call_counter--; }

  int java_call_counter() const           { return _java_call_counter; }
  void set_java_call_counter(int x)       { _java_call_counter = x; }

  int last_native_call_counter() const    { return _last_native_call_counter; }
  void set_last_native_call_counter(int x) { _last_native_call_counter = x; }

  int native_call_counter() const         { return _native_call_counter; }

  bool is_disposable();

  oop print_stack_header_on(outputStream* st);
  void print_stack_on(outputStream* st);

  // GC support
  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);
  void nmethods_do(CodeBlobClosure* cf);
  void metadata_do(void f(Metadata*));
  void frames_do(void f(frame*, const RegisterMap* map));

  static ByteSize thread_offset()             { return byte_offset_of(Coroutine, _thread); }

  static ByteSize state_offset()              { return byte_offset_of(Coroutine, _state); }
  static ByteSize stack_offset()              { return byte_offset_of(Coroutine, _stack); }

  static ByteSize resource_area_offset()      { return byte_offset_of(Coroutine, _resource_area); }
  static ByteSize handle_area_offset()        { return byte_offset_of(Coroutine, _handle_area); }
  static ByteSize last_handle_mark_offset()   { return byte_offset_of(Coroutine, _last_handle_mark); }
  static ByteSize active_handles_offset()     { return byte_offset_of(Coroutine, _active_handles); }
  static ByteSize metadata_handles_offset()   { return byte_offset_of(Coroutine, _metadata_handles); }
  static ByteSize privileged_stack_top_offset(){ return byte_offset_of(Coroutine, _privileged_stack_top); }
  static ByteSize last_Java_sp_offset()       {
    return byte_offset_of(Coroutine, _anchor) + JavaFrameAnchor::last_Java_sp_offset();
  }
  static ByteSize last_Java_pc_offset()       {
    return byte_offset_of(Coroutine, _anchor) + JavaFrameAnchor::last_Java_pc_offset();
  }
  static ByteSize thread_status_offset()      { return byte_offset_of(Coroutine, _thread_status); }
  static ByteSize java_call_counter_offset()  { return byte_offset_of(Coroutine, _java_call_counter); }
  static ByteSize native_call_counter_offset(){ return byte_offset_of(Coroutine, _native_call_counter); }
#ifdef _LP64
  static ByteSize storage_offset()            { return byte_offset_of(Coroutine, _storage); }
#endif

#if defined(_WINDOWS)
private:
  address _last_SEH;
public:
  static ByteSize last_SEH_offset()           { return byte_offset_of(Coroutine, _last_SEH); }
#endif
  static ByteSize wisp_thread_offset()        { return byte_offset_of(Coroutine, _wisp_thread); }
};

class CoroutineStack: public CHeapObj<mtCoroutine>, public DoublyLinkedList<CoroutineStack> {
private:
  JavaThread*     _thread;

  bool            _is_thread_stack;
  ReservedSpace   _reserved_space;
  VirtualSpace    _virtual_space;

  address         _stack_base;
  intptr_t        _stack_size;
  bool            _default_size;

  address         _last_sp;

  // objects of this type can only be created via static functions
  CoroutineStack(intptr_t size) : _reserved_space(size) { }
  virtual ~CoroutineStack() { }

  static Register get_fp_reg();

public:
  static CoroutineStack* create_thread_stack(JavaThread* thread);
  static CoroutineStack* create_stack(JavaThread* thread, intptr_t size = -1);
  static void free_stack(CoroutineStack* stack, JavaThread* THREAD);

  static intptr_t get_start_method();

  JavaThread* thread() const                { return _thread; }
  bool is_thread_stack() const              { return _is_thread_stack; }

  address last_sp() const                   { return _last_sp; }
  void set_last_sp(address x)               { _last_sp = x; }

  address stack_base() const                { return _stack_base; }
  intptr_t stack_size() const               { return _stack_size; }
  bool is_default_size() const              { return _default_size; }

  frame last_frame(Coroutine* coro, RegisterMap& map) const;

  // GC support
  void frames_do(FrameClosure* fc);

  static ByteSize stack_base_offset()         { return byte_offset_of(CoroutineStack, _stack_base); }
  static ByteSize stack_size_offset()         { return byte_offset_of(CoroutineStack, _stack_size); }
  static ByteSize last_sp_offset()            { return byte_offset_of(CoroutineStack, _last_sp); }
};

template<class T> void DoublyLinkedList<T>::remove_from_list(pointer& list) {
  if (list == this) {
    if (list->_next == list)
      list = NULL;
    else
      list = list->_next;
  }
  _last->_next = _next;
  _next->_last = _last;
  _last = NULL;
  _next = NULL;
}

template<class T> void DoublyLinkedList<T>::insert_into_list(pointer& list) {
  if (list == NULL) {
    _next = (T*)this;
    _last = (T*)this;
    list = (T*)this;
  } else {
    _next = list->_next;
    list->_next = (T*)this;
    _last = list;
    _next->_last = (T*)this;
  }
}

template<class T> void DoublyLinkedList<T>::move(pointer &coro, pointer &target) {
  assert(coro != NULL, "coroutine can't be null");
  assert(target != NULL, "target can't be null");
  assert(coro != target, "target can't be equal to current");

  // remove current coro from the ring
  coro->_last->_next = coro->_next;
  coro->_next->_last = coro->_last;

  // insert at new position
  coro->_next = target->_next;
  coro->_last = target;
  coro->_next->_last = coro;
  target->_next = coro;
}

class ObjectWaiter;

class WispThread: public JavaThread {
  friend class Coroutine;
private:
  static bool _wisp_booted;
  static Method* yieldMethod;
  static Method* parkMethod;
  static Method* unparkMethod;
  static Method* runOutsideWispMethod;
  static GrowableArray<int>* _proxy_unpark;

  Coroutine*  _coroutine;
  JavaThread* _thread;

  bool _is_proxy_unpark;

  int _unpark_status;
  int _os_park_reason;
  bool _has_exception;

  Coroutine* _unpark_coroutine;

  WispThread(Coroutine *c): _coroutine(c), _thread(c->thread()), _is_proxy_unpark(false) {
    set_osthread(new OSThread(NULL, NULL));
    set_thread_state(_thread_in_vm);
    _unpark_status = 0;
    _os_park_reason = 0;
    _has_exception = false;
    _unpark_coroutine = NULL;
  }

  virtual ~WispThread() {
    delete osthread();
    set_osthread(NULL);
  }

public:
  enum OsParkReason {
    _not_os_park = 0,
    _wisp_not_booted = 1,
    _wisp_id_not_set = 2,
    _wispengine_in_critical = 3,
    _monitor_is_pending_lock = 4,
    _thread_in_critical = 5,
    _thread_owned_compile_lock = 6,
    _is_sd_lock = 7
  };

  enum BlockingStatus {
    _no_park = 0,
    _wisp_parking = 1,
    _os_parking = 2,
    _wisp_unpark_begin = 3,
    _wisp_unpark_before_call_java = 4,
    _wisp_unpark_after_call_java = 5,
    _wisp_unpark_done = 6,
    _proxy_unpark_begin = 7,
    _proxy_unpark_done = 8,
    _os_unpark = 9
  };

  static bool wisp_booted()           { return _wisp_booted; }
  static void set_wisp_booted(Thread* thread);
  static const char *print_os_park_reason(int reason);
  static const char *print_blocking_status(int status);

  virtual bool is_Wisp_thread() const { return true; }

  Coroutine* coroutine() const        { return _coroutine; }

  JavaThread* thread()   const        { return _thread; }

  void change_thread(JavaThread *thread) { assert(thread->is_Java_thread(), "must be"); _thread = thread; }    // steal support

  bool is_interrupted(bool clear_interrupted);
  static void interrupt(int task_id, TRAPS);

  static ByteSize thread_offset()     { return byte_offset_of(WispThread, _thread); }

  bool is_proxy_unpark()      { return _is_proxy_unpark; }
  void set_proxy_unpark_flag()         { _is_proxy_unpark = true; }
  void clear_proxy_unpark_flag()       { _is_proxy_unpark = false; }

  virtual bool is_lock_owned(address adr) const {
    CoroutineStack* stack = _coroutine->stack();
    return stack->stack_base() >= adr &&
      adr > (stack->stack_base() - stack->stack_size());
  }

  // we must set ObjectWaiter members before node enqueued(observed by other threads)
  void before_enqueue(ObjectMonitor* monitor, ObjectWaiter* ow);
  bool need_wisp_park(ObjectMonitor* monitor, ObjectWaiter* ow, bool is_sd_lock);
  static void park(long millis, const ObjectWaiter* ow);
  static void unpark(const ObjectWaiter* ow, TRAPS) {
    unpark(ow->_park_wisp_id, ow->_using_wisp_park, ow->_proxy_wisp_unpark, ow->_event, (WispThread*)ow->_thread, THREAD);
  }
  static void unpark(int task_id, bool using_wisp_park, bool proxy_unpark, ParkEvent* event, WispThread* wisp_thread, TRAPS);
  static int get_proxy_unpark(jintArray res);

  static WispThread* current(Thread* thread) {
    assert(thread->is_Java_thread(), "invariant") ;
    return thread->is_Wisp_thread() ? (WispThread*) thread :
      ((JavaThread*) thread)->current_coroutine()->wisp_thread();
  }
};

// we supported coroutine stealing for following native calls:
// 1. sun.reflect.NativeMethodAccessorImpl.invoke0()
// 2. sun.reflect.NativeConstructorAccessorImpl.newInstance0()
// 3. java.security.AccessController.doPrivileged()
// 4. java.lang.Object.wait()
// 5,6,7. monitorenter for interp, c1 and c2
class EnableStealMark: public StackObj {
private:
  JavaThread*  _thread;
  Coroutine*   _coroutine;
  int          _enable_steal_count;
public:
  EnableStealMark(Thread* thread);
  ~EnableStealMark();
};

struct WispStealCandidate {
private:
  Symbol *_holder;
  Symbol *_name;
  Symbol *_signature;
public:
  WispStealCandidate (Symbol *holder, Symbol *name, Symbol *signature) : _holder(holder), _name(name), _signature(signature) {}
private:
  bool is_method_invoke() {
    return _holder == vmSymbols::sun_reflect_NativeMethodAccessorImpl() && // sun.reflect.NativeMethodAccessorImpl.invoke0()
           _name == vmSymbols::invoke0_name() &&
           _signature == vmSymbols::invoke0_signature();
  }
  bool is_constructor_newinstance() {
    return _holder == vmSymbols::sun_reflect_NativeConstructorAccessorImpl() &&  // sun.reflect.NativeConstructorAccessorImpl.newInstance0()
           _name == vmSymbols::newInstance0_name() &&
           _signature == vmSymbols::newInstance0_signature();
  }
  bool is_doPrivilege() {
    return _holder == vmSymbols::java_security_AccessController() && // java.security.AccessController.doPrivilege(***)
           _name == vmSymbols::doPrivileged_name() &&
           (_signature == vmSymbols::doPrivileged_signature_1() ||   // doPrivilege() has four native functions
            _signature == vmSymbols::doPrivileged_signature_2() ||
            _signature == vmSymbols::doPrivileged_signature_3() ||
            _signature == vmSymbols::doPrivileged_signature_4());
  }
  bool is_object_wait() {
    return _holder == vmSymbols::java_lang_Object() &&   // java.lang.Object.wait()
           _name == vmSymbols::wait_name() &&
           _signature == vmSymbols::long_void_signature();
  }
public:
  bool is_steal_candidate() {
    return is_constructor_newinstance() || is_doPrivilege() || is_method_invoke() || is_object_wait();
  }
};

struct WispPostStealResource {
public:
  union {
    Thread **thread_ref;
    JNIEnv **jnienv_ref;
  } u;
  enum Type{
    ThreadRef,
    JNIEnvRef
  } type;
public:
  void update_thread_ref(JavaThread *real_thread) { *u.thread_ref = real_thread; }
  void update_jnienv_ref(JNIEnv *real_jnienv) { *u.jnienv_ref = real_jnienv; }
};

// First letter indicates type of the frame:
//    J: Java frame (compiled)
//    j: Java frame (interpreted)
//    V: VM frame (C/C++)
//    v: Other frames running VM generated code (e.g. stubs, adapters, etc.)
//    C: C/C++ frame
#ifdef TARGET_ARCH_x86
# include "coroutine_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "coroutine_aarch64.hpp"
#endif
#define WISP_THREAD_UPDATE get_thread(R_TH)
#define WISP_CALLING_CONVENTION_V2J_UPDATE __ WISP_THREAD_UPDATE
#define WISP_CALLING_CONVENTION_V2j_UPDATE __ WISP_THREAD_UPDATE
#define WISP_COMPILER_RESTORE_FORCE_UPDATE __ WISP_THREAD_UPDATE
#define WISP_V2v_UPDATE WISP_THREAD_UPDATE

class WispPostStealHandleUpdateMark;

class WispResourceArea: public ResourceArea {
  friend class WispPostStealHandleUpdateMark;
private:
  Coroutine *_outer;
public:
  WispResourceArea(Coroutine *outer, MEMFLAGS flags = mtWisp) : ResourceArea(flags), _outer(outer) {}

  WispResourceArea(Coroutine *outer, size_t init_size, MEMFLAGS flags = mtWisp) : ResourceArea(init_size, flags), _outer(outer) { }

  WispPostStealResource* real_allocate_handle() {
#ifdef ASSERT
    WispPostStealResource *src = (WispPostStealResource *) (UseMallocOnly ? internal_malloc_4(sizeof(WispPostStealResource)) : Amalloc_4(sizeof(WispPostStealResource)));
#else
    WispPostStealResource *src = (WispPostStealResource *) Amalloc_4(sizeof(WispPostStealResource));
#endif
    return src;
  }

  // thread steal support
  void wisp_post_steal_handles_do(JavaThread *real_thread);
};

inline void Coroutine::change_thread_for_wisp(JavaThread *thread) {
  this->wisp_post_steal_resource_area()->wisp_post_steal_handles_do(thread);
}

class ThreadInVMfromNative;
class ThreadInVMfromJavaNoAsyncException;
class HandleMarkCleaner;
class JavaThreadInObjectWaitState;
class ThreadBlockInVM;
class vframeStream;

class WispPostStealHandleUpdateMark: public StackObj {
private:
  WispResourceArea *_area;          // Resource area to stack allocate
  Chunk *_chunk;                // saved arena chunk
  char *_hwm, *_max;
  size_t _size_in_bytes;
  Coroutine *_coroutine;
  bool _success;

  bool check(JavaThread *current, bool sp = false);

  void initialize(JavaThread *thread, bool sp = false) {
    if (!check(thread, sp)) return;
    Coroutine *coroutine = thread->current_coroutine();
    _coroutine = coroutine;
    _area = coroutine->wisp_post_steal_resource_area();
    _chunk = _area->_chunk;
    _hwm = _area->_hwm;
    _max= _area->_max;
    _size_in_bytes = _area->size_in_bytes();
    debug_only(_area->_nesting++;)
    assert( _area->_nesting > 0, "must stack allocate RMs" );
  }
public:
  WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,   // constructor only for JavaCalls::call()
                    methodHandle & m1, methodHandle *& m2,
                    JavaCallWrapper & w);
  WispPostStealHandleUpdateMark(Thread *& th,                       // constructor for misc
                    methodHandle *m1 = NULL, methodHandle *m2 = NULL,
                    JavaCallWrapper *w = NULL);
  WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,   // constructor only for JVM_ENTRY
                    JNIEnv *& env, ThreadInVMfromNative & tiv, HandleMarkCleaner & hmc,
                    JavaThreadInObjectWaitState *jtios = NULL, vframeStream *f = NULL, methodHandle *m = NULL);
  WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,   // constructor only for JRT_ENTRY/IRT_ENTRY
                    ThreadInVMfromJavaNoAsyncException & tiva, HandleMarkCleaner & hmc);
  WispPostStealHandleUpdateMark(JavaThread *thread, ThreadBlockInVM & tbv);  // constructor is used inside objectMonitor call, which is also within EnableStealMark scope.
  WispPostStealHandleUpdateMark(JavaThread *& th);                  // this is a special one, used for a fix inside EnableStealMark

  ~WispPostStealHandleUpdateMark();


private:
  size_t size_in_bytes() { return _size_in_bytes; }
};

class WispClinitCounterMark : public StackObj {
public:
  WispClinitCounterMark(Thread* th);
  ~WispClinitCounterMark();
private:
  JavaThread*   _thread;
};

bool clear_interrupt_for_wisp(Thread *);

#endif // SHARE_VM_RUNTIME_COROUTINE_HPP
