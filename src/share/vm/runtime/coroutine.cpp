/*
 * Copyright 2001-2010 Sun Microsystems, Inc.  All Rights Reserved.
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

#include "precompiled.hpp"
#include "prims/privilegedStack.hpp"
#include "runtime/coroutine.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/objectMonitor.inline.hpp"
#include "services/threadService.hpp"
#ifdef TARGET_ARCH_x86
# include "vmreg_x86.inline.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "vmreg_sparc.inline.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "vmreg_zero.inline.hpp"
#endif


#ifdef _WINDOWS

LONG WINAPI topLevelExceptionFilter(struct _EXCEPTION_POINTERS* exceptionInfo);


void coroutine_start(Coroutine* coroutine, jobject coroutineObj) {
  coroutine->thread()->set_thread_state(_thread_in_vm);

  if (UseVectoredExceptions) {
    // If we are using vectored exception we don't need to set a SEH
    coroutine->run(coroutineObj);
  }
  else {
    // Install a win32 structured exception handler around every thread created
    // by VM, so VM can genrate error dump when an exception occurred in non-
    // Java thread (e.g. VM thread).
    __try {
       coroutine->run(coroutineObj);
    } __except(topLevelExceptionFilter((_EXCEPTION_POINTERS*)_exception_info())) {
    }
  }

  ShouldNotReachHere();
}
#endif

#if defined(LINUX) || defined(_ALLBSD_SOURCE)

void coroutine_start(Coroutine* coroutine, jobject coroutineObj) {
  coroutine->thread()->set_thread_state(_thread_in_vm);

  coroutine->run(coroutineObj);
  ShouldNotReachHere();
}
#endif

void Coroutine::run(jobject coroutine) {

  // do not call JavaThread::current() here!

  _thread->set_resource_area(new (mtThread) ResourceArea(32));
  _thread->set_handle_area(new (mtThread) HandleArea(NULL, 32));
  _thread->set_metadata_handles(new (ResourceObj::C_HEAP, mtClass) GrowableArray<Metadata*>(30, true));

  {
    HandleMark hm(_thread);
    HandleMark hm2(_thread);
    Handle obj(_thread, JNIHandles::resolve(coroutine));
    JNIHandles::destroy_global(coroutine);
    JavaValue result(T_VOID);
    JavaCalls::call_virtual(&result,
                            obj,
                            KlassHandle(_thread, SystemDictionary::java_dyn_CoroutineBase_klass()),
                            vmSymbols::startInternal_method_name(),
                            vmSymbols::void_method_signature(),
                            _thread);
  }
}

Coroutine* Coroutine::create_thread_coroutine(JavaThread* thread, CoroutineStack* stack) {
  Coroutine* coro = new Coroutine();
  if (coro == NULL)
    return NULL;

  coro->_state = _current;
  coro->_is_thread_coroutine = true;
  coro->_thread = thread;
  coro->_stack = stack;
  coro->_resource_area = NULL;
  coro->_handle_area = NULL;
  coro->_last_handle_mark = NULL;
  coro->_active_handles = thread->active_handles();
  coro->_metadata_handles = NULL;
  coro->_thread_status = java_lang_Thread::RUNNABLE;
  coro->_java_call_counter = 0;
  coro->_last_native_call_counter = 0;
  coro->_native_call_counter = 0;
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  coro->_privileged_stack_top = NULL;
  coro->_wisp_thread  = UseWispMonitor ? new WispThread(coro) : NULL;
  coro->_wisp_engine  = NULL;
  coro->_wisp_task    = NULL;
  coro->_wisp_task_id = WISP_ID_NOT_SET;
  coro->_coroutine    = NULL;
  coro->_enable_steal_count = 1;
  coro->_clinit_call_counter = 0;
  coro->_wisp_post_steal_resource_area = NULL;
  coro->_is_yielding  = false;
  thread->set_current_coroutine(coro);
  return coro;
}

/**
 * The initial value for corountine' active handles, which will be replaced with a real one
 * when the coroutine invokes call_virtual (before running any real logic).
 */
static JNIHandleBlock* shared_empty_JNIHandleBlock = JNIHandleBlock::allocate_block();

Coroutine* Coroutine::create_coroutine(JavaThread* thread, CoroutineStack* stack, oop coroutineObj) {
  Coroutine* coro = new Coroutine();
  if (coro == NULL) {
    return NULL;
  }

  jobject obj = JNIHandles::make_global(coroutineObj);

  intptr_t** d = (intptr_t**)stack->stack_base();
  set_coroutine_base(d, thread, obj, coro, coroutineObj, (address)coroutine_start);

  stack->set_last_sp((address) d);

  coro->_state = _created;
  coro->_is_thread_coroutine = false;
  coro->_thread = thread;
  coro->_stack = stack;
  coro->_resource_area = NULL;
  coro->_handle_area = NULL;
  coro->_last_handle_mark = NULL;
  coro->_active_handles = shared_empty_JNIHandleBlock;
  coro->_metadata_handles = NULL;
  coro->_thread_status = java_lang_Thread::RUNNABLE;
  coro->_java_call_counter = 0;
  coro->_last_native_call_counter = 0;
  coro->_native_call_counter = 0;
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  coro->_privileged_stack_top = NULL;
  coro->_wisp_thread  = UseWispMonitor ? new WispThread(coro) : NULL;
  coro->_wisp_engine  = NULL;
  coro->_wisp_task    = NULL;
  coro->_wisp_task_id = WISP_ID_NOT_SET;
  // oop address may change during JNIHandles::make_global due to lock contention
  coro->_coroutine = JNIHandles::resolve_non_null(obj);
  // if coro->_enable_steal_count == coro->_java_call_counter is true, we can do work steal.
  // when a coroutine starts, the `_java_call_counter` is 0,
  // then it will call java method `Coroutine.startInternal()` and then `_java_call_counter` is 1.
  // so we set `_enable_steal_count` to 1 which means this coroutine can be stolen when it starts.
  coro->_enable_steal_count = 1;
  coro->_clinit_call_counter = 0;
  coro->_wisp_post_steal_resource_area = new (mtWisp) WispResourceArea(coro, 32);
  coro->_is_yielding  = false;
  return coro;
}

Coroutine::~Coroutine() {
  remove_from_list(_thread->coroutine_list());
  if (_wisp_thread != NULL) {
    delete _wisp_thread;
  }
  delete _wisp_post_steal_resource_area;
  if (!_is_thread_coroutine && _state != Coroutine::_created) {
    assert(_resource_area != NULL, "_resource_area is NULL");
    assert(_handle_area != NULL, "_handle_area is NULL");
    assert(_metadata_handles != NULL, "_metadata_handles is NULL");
    delete _resource_area;
    delete _handle_area;
    delete _metadata_handles;
    JNIHandleBlock::release_block(active_handles(), _thread);
    //allocated from JavaCalls::call_virtual during coroutine's first running
  }
}

void Coroutine::frames_do(FrameClosure* fc) {
  switch (_state) {
    case Coroutine::_created:
      // the coroutine has never been run
      break;
    case Coroutine::_current:
      // the contents of this coroutine have already been visited
      break;
    case Coroutine::_onstack:
      _stack->frames_do(fc);
      break;
    case Coroutine::_dead:
      // coroutine is dead, ignore
      break;
  }
}

bool Coroutine::is_coroutine_frame(javaVFrame* jvf) {
  ResourceMark resMark;
  const char* k_name = jvf->method()->method_holder()->name()->as_C_string();
  return strstr(k_name, "com/alibaba/wisp/engine/") != 0 || strstr(k_name, "java/dyn/") != 0;
}
/* a typical wisp stack looks like:
  at java.dyn.CoroutineSupport.unsafeSymmetricYieldTo(CoroutineSupport.java:134)
  - parking to wait for  <0x00000007303e1c28> (a java.util.concurrent.locks.AbstractQueuedSynchronizer$ConditionObject)
  at com.alibaba.wisp.engine.WispTask.switchTo(WispTask.java:254)
  at com.alibaba.wisp.engine.WispEngine.yieldTo(WispEngine.java:613)
  at com.alibaba.wisp.engine.Wisp2Engine.yieldToNext(Wisp2Engine.java:211)
  at com.alibaba.wisp.engine.WispEngine.yieldOnBlocking(WispEngine.java:574)
  at com.alibaba.wisp.engine.WispTask.parkInternal(WispTask.java:350)
  at com.alibaba.wisp.engine.WispTask.jdkPark(WispTask.java:403)
  at com.alibaba.wisp.engine.WispEngine$4.park(WispEngine.java:224)
  at sun.misc.Unsafe.park(Unsafe.java:1029)
  at java.util.concurrent.locks.LockSupport.park(LockSupport.java:176)
  at java.util.concurrent.locks.AbstractQueuedSynchronizer$ConditionObject.await(AbstractQueuedSynchronizer.java:2047)
  at java.util.concurrent.ArrayBlockingQueue.take(ArrayBlockingQueue.java:403)
  at java.util.concurrent.ThreadPoolExecutor.getTask(ThreadPoolExecutor.java:1077)
  at java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1137)
  at java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:627)
  at java.lang.Thread.run(Thread.java:861)
  at com.alibaba.wisp.engine.WispTask.runOutsideWisp(WispTask.java:253)
  at com.alibaba.wisp.engine.WispTask.coroutineCacheLoop(WispTask.java:213)
  at com.alibaba.wisp.engine.WispTask.access$000(WispTask.java:33)
  at com.alibaba.wisp.engine.WispTask$CacheableCoroutine.run(WispTask.java:153)
  at java.dyn.CoroutineBase.startInternal(CoroutineBase.java:60)
  Preempt should only happened when we're executing the non-wisp part.
*/
bool Coroutine::in_critical(JavaThread* thread) {
  ResourceMark resMark;
  RegisterMap reg_map(thread);
  for (vframe* f = thread->last_java_vframe(&reg_map); f; f = f->sender()) {
    if (!f->is_java_frame()) {
      continue;
    }
    javaVFrame* jvf = javaVFrame::cast(f);
    assert(WispThread::runOutsideWispMethod != NULL, "runOutsideWispMethod resolved");
    /*
      Marked WispThread::runOutsideWispMethod as a seperator for wisp and non wisp code on
    stack,preempt is allowed under the only senarios when ther is no wisp frame upon
    WispThread::runOutsideWispMethod.
      We are using a more strict check because old preempt check rules leave out cases where
    wisp inner logic calls util classes, for example:
      - at java.util.concurrent.ConcurrentSkipListSet.add
      - at com.alibaba.wisp.engine.WispTask.returnTaskTocache
      - at com.alibaba.wisp.engine.WispTask.taskExit
      - at com.alibaba.wisp.engine.WispTask.coroutineCacheLoop
      Preempt this stack may cause crash during shutdown
    */
    if (jvf->method() == WispThread::runOutsideWispMethod) {
      return false;
    } else if (is_coroutine_frame(jvf)) {
      if (VerboseWisp) {
        tty->print_cr("[WISP] preempt was blocked, because wisp internal method on the stack");
      }
      return true;
    }
  }
  if (VerboseWisp) {
    tty->print_cr("[WISP] preempt was blocked, because wisp internal method on the stack");
  }
  return true;
}

class oops_do_Closure: public FrameClosure {
private:
  OopClosure* _f;
  CodeBlobClosure* _cf;
  CLDClosure* _cld_f;
public:
  oops_do_Closure(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf): _f(f), _cld_f(cld_f), _cf(cf) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->oops_do(_f, _cld_f, _cf, map); }
};

void Coroutine::oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf) {
  oops_do_Closure fc(f, cld_f, cf);
  frames_do(&fc);
  if (_state == _onstack) {
    assert(_handle_area != NULL, "_onstack coroutine should have _handle_area");
    DEBUG_CORO_ONLY(tty->print_cr("collecting handle area %08x", _handle_area));
    _handle_area->oops_do(f);
    _active_handles->oops_do(f);
    if (_privileged_stack_top != NULL) {
      _privileged_stack_top->oops_do(f);
    }
  }
  if (_wisp_task != NULL) {
    f->do_oop((oop*) &_wisp_engine);
    f->do_oop((oop*) &_wisp_task);
  }
  if (_coroutine != NULL) {
    f->do_oop((oop*) &_coroutine);
  }
}

class nmethods_do_Closure: public FrameClosure {
private:
  CodeBlobClosure* _cf;
public:
  nmethods_do_Closure(CodeBlobClosure* cf): _cf(cf) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->nmethods_do(_cf); }
};

void Coroutine::nmethods_do(CodeBlobClosure* cf) {
  nmethods_do_Closure fc(cf);
  frames_do(&fc);
}
class metadata_do_Closure: public FrameClosure {
private:
  void (*_f)(Metadata*);
public:
  metadata_do_Closure(void f(Metadata*)): _f(f) { }
  void frames_do(frame* fr, RegisterMap* map) { fr->metadata_do(_f); }
};

void Coroutine::metadata_do(void f(Metadata*)) {
  if (metadata_handles() != NULL) {
    for (int i = 0; i< metadata_handles()->length(); i++) {
      f(metadata_handles()->at(i));
    }
  }
  metadata_do_Closure fc(f);
  frames_do(&fc);
}

class frames_do_Closure: public FrameClosure {
private:
  void (*_f)(frame*, const RegisterMap*);
public:
  frames_do_Closure(void f(frame*, const RegisterMap*)): _f(f) { }
  void frames_do(frame* fr, RegisterMap* map) { _f(fr, map); }
};

void Coroutine::frames_do(void f(frame*, const RegisterMap* map)) {
  frames_do_Closure fc(f);
  frames_do(&fc);
}

bool Coroutine::is_disposable() {
  // _handle_area == NULL indicates this coroutine has not been initialized,
  // we should delete it directly.
  return _handle_area == NULL;
}

void Coroutine::set_wisp_engine(oop x) {
  _wisp_engine = x;
}


CoroutineStack* CoroutineStack::create_thread_stack(JavaThread* thread) {
  CoroutineStack* stack = new CoroutineStack(0);
  if (stack == NULL)
    return NULL;

  stack->_thread = thread;
  stack->_is_thread_stack = true;
  stack->_stack_base = thread->stack_base();
  stack->_stack_size = thread->stack_size();
  stack->_last_sp = NULL;
  stack->_default_size = false;
  return stack;
}

CoroutineStack* CoroutineStack::create_stack(JavaThread* thread, intptr_t size/* = -1*/) {
  bool default_size = false;
  if (size <= 0) {
    size = DefaultCoroutineStackSize;
    default_size = true;
  }

  uint reserved_pages = StackShadowPages + StackRedPages + StackYellowPages;
  uintx real_stack_size = size + (reserved_pages * os::vm_page_size());
  uintx reserved_size = align_size_up(real_stack_size, os::vm_allocation_granularity());

  CoroutineStack* stack = new CoroutineStack(reserved_size);
  if (stack == NULL)
    return NULL;
  if (!stack->_virtual_space.initialize(stack->_reserved_space, real_stack_size)) {
    stack->_reserved_space.release();
    delete stack;
    return NULL;
  }

  stack->_thread = thread;
  stack->_is_thread_stack = false;
  stack->_stack_base = (address)stack->_virtual_space.high();
  stack->_stack_size = stack->_virtual_space.committed_size();
  stack->_last_sp = NULL;
  stack->_default_size = default_size;

  if (os::uses_stack_guard_pages()) {
    address low_addr = stack->stack_base() - stack->stack_size();
    size_t len = (StackYellowPages + StackRedPages) * os::vm_page_size();

    bool allocate = os::allocate_stack_guard_pages();

    if (!os::guard_memory((char *) low_addr, len)) {
      warning("Attempt to protect stack guard pages failed.");
      if (os::uncommit_memory((char *) low_addr, len)) {
        warning("Attempt to deallocate stack guard pages failed.");
      }
    }
  }

  DEBUG_CORO_ONLY(tty->print("created coroutine stack at %08x with stack size %i (real size: %i)\n", stack->_stack_base, size, stack->_stack_size));
  return stack;
}

void CoroutineStack::free_stack(CoroutineStack* stack, JavaThread* thread) {
  if (stack->is_thread_stack()) {
    delete stack;
    return;
  }

  if (stack->_reserved_space.size() > 0) {
    stack->_virtual_space.release();
    stack->_reserved_space.release();
  }
  delete stack;
}

void CoroutineStack::frames_do(FrameClosure* fc) {
  assert(_last_sp != NULL, "CoroutineStack with NULL last_sp");

  DEBUG_CORO_ONLY(tty->print_cr("frames_do stack %08x", _stack_base));

  intptr_t* fp = ((intptr_t**)_last_sp)[0];
  address pc = ((address*)_last_sp)[1];
  intptr_t* sp = ((intptr_t*)_last_sp) + 2;

  frame fr(sp, fp, pc);

#ifdef ASSERT
  // if fp == NULL, it must be that frame pointer register is used as a general register
  // in compiled code and the frame pointer register is written to 0 when the code's running.
  if (fp == NULL) {
    assert(fr.is_first_frame() || (!PreserveFramePointer && !fr.is_interpreted_frame() && CodeCache::find_blob(pc)), "FP is NULL only if sender frame is a JavaCalls::call() frame, or a compiler frame && -XX:-PreserveFramePointer");
  }
#endif

  // the `fp` here is useful only if the sender is an interpreter/native frame.
  // see comments in `frame::sender()`
  // if the sender is a compiler frame, we cannot assume its value.
  StackFrameStream fst(_thread, fr);
  fst.register_map()->set_location(get_fp_reg()->as_VMReg(), (address)_last_sp);
  fst.register_map()->set_include_argument_oops(false);
  for(; !fst.is_done(); fst.next()) {
    fc->frames_do(fst.current(), fst.register_map());
  }
}

frame CoroutineStack::last_frame(Coroutine* coro, RegisterMap& map) const {
  DEBUG_CORO_ONLY(tty->print_cr("last_frame CoroutineStack"));

  intptr_t* fp = ((intptr_t**)_last_sp)[0];
  address pc = ((address*)_last_sp)[1];
  intptr_t* sp = ((intptr_t*)_last_sp) + 2;
  map.set_location(get_fp_reg()->as_VMReg(), (address)_last_sp);
  map.set_include_argument_oops(false);

  frame f(sp, fp, pc);
#ifdef ASSERT
  if (fp == NULL) {
    assert(f.is_first_frame() || (!PreserveFramePointer && !f.is_interpreted_frame() && CodeCache::find_blob(pc)), "FP is NULL only if sender frame is a JavaCalls::call() frame, or a compiler frame && -XX:-PreserveFramePointer");
  }
#endif

  return f;
}

oop Coroutine::print_stack_header_on(outputStream* st) {
  oop thread_obj = NULL;
  st->print("\n - Coroutine [%p]", this);
  if (_wisp_task != NULL) {
    thread_obj = com_alibaba_wisp_engine_WispTask::get_threadWrapper(_wisp_task);
    char buf[128] = "<cached>";
    if (thread_obj != NULL) {
      oop name = java_lang_Thread::name(thread_obj);
      if (name != NULL) {
        java_lang_String::as_utf8_string(name, buf, sizeof(buf));
      }
    }
    st->print(" \"%s\" #%d active=%d steal=%d steal_fail=%d preempt=%d park=%d/%d", buf,
        com_alibaba_wisp_engine_WispTask::get_id(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_activeCount(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_stealCount(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_stealFailureCount(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_preemptCount(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_jvmParkStatus(_wisp_task),
        com_alibaba_wisp_engine_WispTask::get_jdkParkStatus(_wisp_task));
  }
  if (_wisp_thread && PrintThreadCoroutineInfo) {
    st->print(" monitor_park_stage=%s", WispThread::print_blocking_status(_wisp_thread->_unpark_status));
    if (_wisp_thread->_os_park_reason != WispThread::_not_os_park) {
      st->print(" os_park_reason=%s", WispThread::print_os_park_reason(_wisp_thread->_os_park_reason));
    }
    if (_wisp_thread->_unpark_coroutine) {
      st->print(" unpark_thread=%p", _wisp_thread->_unpark_coroutine);
      if (_wisp_thread->_has_exception) {
        st->print(" (unpark has exception)");
      }
    }
  } // else, we're only using the JKU part
  return thread_obj;
}

void Coroutine::print_stack_on(outputStream* st) {
  if (_state == Coroutine::_onstack) {
    oop thread_obj = print_stack_header_on(st);
    st->print("\n");

    ResourceMark rm;
    RegisterMap reg_map(_thread);
    frame last_frame = _stack->last_frame(this, reg_map);

    int count = 0;
    JavaThread* t = UseWispMonitor ? _wisp_thread : _thread;
    for (vframe* vf = vframe::new_vframe(&last_frame, &reg_map, t); vf; vf = vf->sender()) {
      if (vf->is_java_frame()) {
        javaVFrame* jvf = javaVFrame::cast(vf);
        java_lang_Throwable::print_stack_element(st, jvf->method(), jvf->bci());

        if (UseWispMonitor && JavaMonitorsInStackTrace) {
          // t is WispThread
          t->set_threadObj(thread_obj);
          // ensure thread()->current_park_blocker() fetch the correct thread_obj
          jvf->print_lock_info_on(st, count++);
        }
      }
    }
  }
}


//  ---------- lock support -----------
bool WispThread::_wisp_booted = false;
Method* WispThread::parkMethod = NULL;
Method* WispThread::unparkMethod = NULL;
Method* WispThread::runOutsideWispMethod = NULL;
GrowableArray<int>* WispThread::_proxy_unpark = NULL;

void WispThread::set_wisp_booted(Thread* thread) {
  // In JDK11, it introduces metaspace compact. Storing the method isn't safe.
  // The flow should be changed.
  CallInfo callinfo;
  KlassHandle kh = KlassHandle(thread, SystemDictionary::com_alibaba_wisp_engine_WispTask_klass());
  LinkResolver::resolve_static_call(callinfo, kh,
      vmSymbols::park_name(), vmSymbols::long_void_signature(), KlassHandle(), false, true, thread);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");
  parkMethod = method();

  LinkResolver::resolve_static_call(callinfo, kh,
      vmSymbols::unparkById_name(), vmSymbols::int_void_signature(), KlassHandle(), false, true, thread);
  method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");
  unparkMethod = method();

  LinkResolver::resolve_static_call(callinfo, kh,
      vmSymbols::runOutsideWisp_name(), vmSymbols::runnable_void_signature(), KlassHandle(), false, true, thread);
  method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");
  runOutsideWispMethod = method();
  if (UseWispMonitor) {
    _proxy_unpark = new (ResourceObj::C_HEAP, mtWisp) GrowableArray<int>(30, true);
    if (!AlwaysLockClassLoader) {
      SystemDictionary::system_dict_lock_change(thread);
    }
  }

  _wisp_booted = true; // set after parkMethod != null
}

/*
 * Avoid coroutine switch in the following scenarios:
 *
 * - _wisp_booted:
 *   We guarantee the classes referenced by WispTask.park(called in WispThread::park in native)
 *   are already loaded after _wisp_booted is set(as true). Otherwise it might result in loading class during execution of WispTask.park.
 *   Coroutine switch caused by object monitors in class loading might lead to recursive deadlock.
 *
 * - !com_alibaba_wisp_engine_WispCarrier::is_critical(_coroutine->wisp_engine()):
 *   If the program is already running in kernel code of wisp engine(marked by WispEngine.isInCritical at Java level),  we don't expect
 *   the switch while coroutine running into 'synchronized' block which is heavily used by Java NIO library.
 *   Otherwise, it might lead to potential recursive deadlock.
 *
 * - monitor->object() != java_lang_ref_Reference::pending_list_lock():
 *   pending_list_lock(PLL) is special ObjectMonitor used in GC vm operation.
 *   if we treated it as normal monitor(T10965418)
 *   - 'dead lock' in jni_critical case:
 *     Given coroutine A, B running in the same thread,
 *     1. coroutine A called jni_ReleasePrimitiveArrayCritical, and triggered GC
 *     1.1. VM_GC_Operation::doit_prologue() -> acquired PLL and yielded to coroutine B(because of contending)
 *     2. If coroutine B called jni_GetPrimitiveArrayCritical at this moment, it would be blocked by GC flag(jni_lock)
 *     3. Unfortunately, A doesn't have any chance to release PLL(acquired in 1.1). As a result, the process is suspended.
 *   - if we disable switch in InstanceRefKlass::acquire_pending_list_lock(ObjectSynchronizer::fast_enter)
 *     Given coroutine A, B running in the same thread,
 *     1. coroutine A attempted to acquire PLL and yielded out (because of contending)
 *     2. coroutine B in VM_GC_Operation::doit_prologue() attempted to acquire PLL(which is triggered by allocation failure),  gets blocked
 *       as we disable the switch in InstanceRefKlass::acquire_pending_list_lock.
 *     3. Another thread unparked A, but unfortunately the engine is already blocked.
 *   So , totally disable the switch in PLL is better solution(monitor->object() != java_lang_ref_Reference::pending_list_lock()).
 *
*/

bool WispThread::need_wisp_park(ObjectMonitor* monitor, ObjectWaiter* ow, bool is_sd_lock) {
  _os_park_reason = _not_os_park;
  _unpark_status = _no_park;

  if (!_wisp_booted) {
    _os_park_reason = _wisp_not_booted;
    return false;
  }
  if (_coroutine->wisp_task_id() == Coroutine::WISP_ID_NOT_SET) {
    // For java threads, including JvmtiAgentThread, ServiceThread,
    // SurrogateLockerThread, CompilerThread which are only used in JVM,
    // because we don't initialize co-routine stuff for them, the initial
    // value of _coroutine->wisp_task_id() for them should be Coroutine::WISP_ID_NOT_SET.
    _os_park_reason = _wisp_id_not_set;
    return false;
  }
  if (com_alibaba_wisp_engine_WispCarrier::in_critical(_coroutine->wisp_engine())) {
    _os_park_reason = _wispengine_in_critical;
    return false;
  }
  if (monitor->object() == java_lang_ref_Reference::pending_list_lock()) {
    _os_park_reason = _monitor_is_pending_lock;
    return false;
  }
  if (_thread->in_critical()) {
    _os_park_reason = _thread_in_critical;
    return false;
  }
  if (Compile_lock->owner() == _thread) {
    // case holding Compile_lock,  we will park for lock contention
    // not for monitor->wait()
    // so we'll be unparked immediately. It's safe to block the JavaThread
    _os_park_reason = _thread_owned_compile_lock;
    return false;
  }
  if (is_sd_lock && ow->TState != ObjectWaiter::TS_WAIT) {
    // case parking for fetching SystemDictionary_lock fails, DO NOT schedule
    // otherwise we'll encounter DEADLOCK because of fetching CompilerQeueu_lock in java
    _os_park_reason = _is_sd_lock;
    return false;
  }
  return true;
}

void WispThread::before_enqueue(ObjectMonitor* monitor, ObjectWaiter* ow) {
  JavaThreadState st = _thread->_thread_state;
  if (st != _thread_in_vm) {
    assert(st == _thread_blocked, "_thread_state should be _thread_blocked");
    ThreadStateTransition::transition(_thread, st, _thread_in_vm);
  }
  bool is_sd_lock = monitor->object() == SystemDictionary_lock->obj();
  if (need_wisp_park(monitor, ow, is_sd_lock)) {
    ow->_using_wisp_park = true;
    ow->_park_wisp_id = _coroutine->wisp_task_id();
    com_alibaba_wisp_engine_WispTask::set_jvmParkStatus(_coroutine->wisp_task(), 0); //reset
  } else {
    ow->_using_wisp_park = false;
    ow->_park_wisp_id = Coroutine::WISP_ID_NOT_SET;
  }
  // when we're operating on SystemDictionary_lock, we're running jvm codes,
  // if the unpark was lazy dispatched, the thread may block on a jvm Monitor
  // before the unpark disptach.
  // So we proxy all unpark operations on the SystemDictionary_lock.
  ow->_proxy_wisp_unpark = is_sd_lock;
  ow->_timeout = is_sd_lock && ow->TState != ObjectWaiter::TS_WAIT ? 1 : -1;
  if (st != _thread_in_vm) {
    ThreadStateTransition::transition(_thread, _thread_in_vm, st);
  }
}

void WispThread::park(long millis, const ObjectWaiter* ow) {
  assert(ow->_thread->is_Wisp_thread(), "not wisp thread");
  JavaThread* jt = ((WispThread*) ow->_thread)->thread();
  assert(jt == Thread::current(), "current");

  if (ow->_timeout > 0) {
    millis = ow->_timeout;
    assert(!ow->_using_wisp_park, "invariant");
  }
  WispThread* wisp_thread = (WispThread*) ow->_thread;
  if (ow->_using_wisp_park) {
    assert(parkMethod != NULL, "parkMethod should be resolved in set_wisp_booted");

    ThreadStateTransition::transition(jt, _thread_blocked, _thread_in_vm);

    JavaValue result(T_VOID);
    JavaCallArguments args;
    args.push_long(millis * 1000000); // to nanos

    // thread steal support
    WispPostStealHandleUpdateMark w(jt);   // special one, because park() is inside an EnableStealMark, so the _enable_steal_count counter has been added one.
    wisp_thread->_unpark_status = _wisp_parking;
    JavaCalls::call(&result, methodHandle(parkMethod), &args, jt);

    // the runtime can not handle the exception on monitorenter bci
    // we need clear it to prevent jvm crash
    if (jt->has_pending_exception()) {
      assert(jt->pending_exception()->klass() == SystemDictionary::ThreadDeath_klass(),
          "thread_death expected");
      jt->clear_pending_exception();
    }

    ThreadStateTransition::transition(jt, _thread_in_vm, _thread_blocked);
  } else {
    wisp_thread->_unpark_status = _os_parking;
    if (millis <= 0) {
      ow->_event->park();
    } else {
      ow->_event->park(millis);
    }
  }
}

void WispThread::unpark(int task_id, bool using_wisp_park, bool proxy_unpark, ParkEvent* event, WispThread* wisp_thread, TRAPS) {
  wisp_thread->_has_exception = false;
  wisp_thread->_unpark_coroutine = NULL;
  if (!using_wisp_park) {
    wisp_thread->_unpark_status = _os_unpark;
    event->unpark();
    return;
  }

  JavaThread* jt = THREAD->is_Wisp_thread() ? ((WispThread*) THREAD)->thread() : (JavaThread*) THREAD;

  assert(UseWispMonitor, "UseWispMonitor must be true here");
  bool proxy_unpark_special_case = false;
  WispThread* wt = WispThread::current(THREAD);
  proxy_unpark_special_case = wt->is_proxy_unpark();
  wt->clear_proxy_unpark_flag();
  wisp_thread->_unpark_coroutine = jt->current_coroutine();

  if (proxy_unpark || proxy_unpark_special_case ||
      jt->is_Compiler_thread() ||
      jt->is_hidden_from_external_view() ||
      // SurrogateLockerThread and ServiceThread are "is_hidden_from_external_view()"
      jt->is_jvmti_agent_thread()) { // not normal java therad
    // proxy_unpark mechanism:
    // After we change SystemDictionary_lock from Mutex* to objectMonitor,
    // we need to face the reality that some non-JavaThread may produce
    // wisp unpark, but they can not invoke java methods.
    // So we introduce a unpark dispatch java thread, then append the unpark requests
    // to a list, the dispatch thread will fetch and dispatch unparks.

    // due to the fact that we modify the priority of Wisp_lock from `non-leaf` to `special`,
    // so we'd use `MutexLockerEx` and `_no_safepoint_check_flag` to make our program run
    MutexLockerEx mu(Wisp_lock, Mutex::_no_safepoint_check_flag);
    wisp_thread->_unpark_status = WispThread::_proxy_unpark_begin;
    _proxy_unpark->append(task_id);
    Wisp_lock->notify(); // only one consumer
    wisp_thread->_unpark_status = WispThread::_proxy_unpark_done;
    return;
  }

  wisp_thread->_unpark_status = WispThread::_wisp_unpark_begin;
  JavaValue result(T_VOID);
  JavaCallArguments args;
  args.push_int(task_id);
  bool in_java = false;
  oop pending_excep = NULL;
  const char* pending_file;
  int pending_line;
  // Class not found exception may appear here.
  // If there exists an exception, the java call could not succeed.
  // So must record and clear excpetions here.
  if (jt->has_pending_exception()) {
    pending_excep = jt->pending_exception();
    pending_file  = jt->exception_file();
    pending_line  = jt->exception_line();
    jt->clear_pending_exception();
  }
  if (jt->thread_state() == _thread_in_Java) {
    in_java = true;
    ThreadStateTransition::transition_from_java(jt, _thread_in_vm);
  }
  // Do java calls to perform unpark work.
  {
    assert(unparkMethod != NULL, "unparkMethod should be resolved in set_wisp_booted");
    wisp_thread->_unpark_status = WispThread::_wisp_unpark_before_call_java;
    JavaCalls::call(&result, methodHandle(unparkMethod), &args, jt);
    wisp_thread->_unpark_status = WispThread::_wisp_unpark_after_call_java;
  }
  if (jt->has_pending_exception()) {
    assert(jt->pending_exception()->klass() == SystemDictionary::ThreadDeath_klass(),
        "thread_death excepted");
    wisp_thread->_has_exception = true;
    jt->clear_pending_exception();
  }
  if (in_java) {
    ThreadStateTransition::transition(jt, _thread_in_vm, _thread_in_Java);
  }
  if (pending_excep != NULL) {
    jt->set_pending_exception(pending_excep, pending_file, pending_line);
  }
  wisp_thread->_unpark_status = WispThread::_wisp_unpark_done;
}

int WispThread::get_proxy_unpark(jintArray res) {
  MutexLockerEx mu(Wisp_lock, Mutex::_no_safepoint_check_flag);
  while (_proxy_unpark == NULL || _proxy_unpark->is_empty()) {
    Wisp_lock->wait();
  }
  typeArrayOop a = typeArrayOop(JNIHandles::resolve_non_null(res));
  if (a == NULL) {
    return 0;
  }
  int copy_cnt = a->length() < _proxy_unpark->length() ? a->length() : _proxy_unpark->length();
  int copy_start = _proxy_unpark->length() - copy_cnt < 0 ? 0 : _proxy_unpark->length() - copy_cnt;
  memcpy(a->int_at_addr(0), _proxy_unpark->adr_at(copy_start), copy_cnt * sizeof(int));
  _proxy_unpark->trunc_to(copy_start);

  return copy_cnt;
}


bool WispThread::is_interrupted(bool clear_interrupted) {
  if (_coroutine->wisp_task() == NULL) {
    // wisp is not initialized
    return Thread::is_interrupted(_thread, clear_interrupted);
  }
  int interrupted = com_alibaba_wisp_engine_WispTask::get_interrupted(_coroutine->wisp_task());
  if (interrupted && clear_interrupted) {
    com_alibaba_wisp_engine_WispTask::set_interrupted(_coroutine->wisp_task(), 0);
  }
  return interrupted != 0;
}

void WispThread::interrupt(int task_id, TRAPS) {
  assert(UseWispMonitor, "sanity");
  assert(((JavaThread*) THREAD)->thread_state() == _thread_in_vm, "thread state");
  JavaValue result(T_VOID);
  JavaCallArguments args;
  args.push_int(task_id);
  JavaCalls::call_static(&result,
      KlassHandle(THREAD, SystemDictionary::com_alibaba_wisp_engine_WispTask_klass()),
      vmSymbols::interruptById_name(),
      vmSymbols::int_void_signature(),
      &args,
      THREAD);

}

const char* WispThread::print_os_park_reason(int reason) {
  switch(reason) {
  case _not_os_park:
    return "NOT_OS_PARK";
  case _wisp_not_booted:
    return "WISP_NOT_BOOOTED";
  case _wisp_id_not_set:
    return "WISP_ID_NOT_SET";
  case _wispengine_in_critical:
    return "WISPENGINE_IN_CRITICAL";
  case _monitor_is_pending_lock:
    return "MONITOR_IS_PENDING_LOCK";
  case _thread_in_critical:
    return "THREAD_IN_CRITICAL";
  case _thread_owned_compile_lock:
    return "THREAD_OWNED_COMPILE_LOCK";
  case _is_sd_lock:
    return "IS_SD_LOCK";
  default:
    return "";
  }
}

const char* WispThread::print_blocking_status(int status) {
  switch(status) {
  case _no_park:
    return "NO_PARK";
  case _wisp_parking:
    return "WISP_PARKING";
  case _os_parking:
    return "OS_PARKING";
  case _wisp_unpark_begin:
    return "WISP_UNPARK_BEGIN";
  case _wisp_unpark_before_call_java:
    return "WISP_UNPARK_BEFORE_CALL_JAVA";
  case _wisp_unpark_after_call_java:
    return "WISP_UNPARK_AFTER_CALL_JAVA";
  case _wisp_unpark_done:
    return "WISP_UNPARK_DONE";
  case _proxy_unpark_begin:
    return "PROXY_UNPARK_BEGIN";
  case _proxy_unpark_done:
    return "PROXY_UNPARK_DONE";
  case _os_unpark:
    return "OS_UNPARK";
  default:
    return "";
  }
}

void Coroutine::after_safepoint(JavaThread* thread) {
  assert(Thread::current() == thread, "sanity check");
  guarantee(thread->safepoint_state()->is_running(), "safepoint should finish");

  Coroutine* coroutine = thread->current_coroutine();
  if (thread->thread_state() != _thread_in_Java ||
      // indicates we're inside compiled code or interpreter.
      // rather than thread state transition.
      coroutine->_is_yielding || !thread->wisp_preempted() ||
      thread->has_pending_exception() || thread->has_async_condition() ||
      coroutine->in_critical(thread)) {
    return;
  }

  oop wisp_task = thread->current_coroutine()->_wisp_task;
  if (wisp_task != NULL) { // expose to perfCount and jstack
    int cnt = com_alibaba_wisp_engine_WispTask::get_preemptCount(wisp_task);
    com_alibaba_wisp_engine_WispTask::set_preemptCount(wisp_task, cnt + 1);
  } else {
    assert(!WispThread::wisp_booted(), "wisp_task should be non-null when wisp is enabled");
  }

  coroutine->_is_yielding = true;
  // "yield" will immediately switch context to execute other coroutines.
  // After all the runnable coroutines has been executed, we'll switch back.
  //
  // - The preempt mechanism should be disabled when current coroutine is calling "yield"
  // - The preempt mechanism should be enabled during "other" coroutines are executing

  thread->set_wisp_preempted(false);
  ThreadInVMfromJava tiv(thread);
  JavaValue result(T_VOID);
  JavaCallArguments args;
  JavaCalls::call_static(&result,
        KlassHandle(thread, SystemDictionary::Thread_klass()),
        vmSymbols::yield_name(),
        vmSymbols::void_method_signature(),
        &args,
        thread);
  coroutine->_is_yielding = false;

  if (thread->has_pending_exception()
  && (thread->pending_exception()->klass() == SystemDictionary::OutOfMemoryError_klass()
   || thread->pending_exception()->klass() == SystemDictionary::StackOverflowError_klass())) {
      // throw expected vm error
      return;
  }

  if (thread->has_pending_exception() || thread->has_async_condition()) {
    guarantee(thread->pending_exception()->klass() == SystemDictionary::ThreadDeath_klass(),
        "thread_death expected");
    thread->clear_pending_exception();
  }
}

EnableStealMark::EnableStealMark(Thread* thread) {
  assert(JavaThread::current() == thread, "current thread check");
  if (EnableCoroutine) {
    if (thread->is_Java_thread()) {
      _thread = (JavaThread*) thread;
      _coroutine = _thread->current_coroutine();
      _enable_steal_count = _coroutine->add_enable_steal_count();
    } else {
      _coroutine = NULL;
    }
  }
}

EnableStealMark::~EnableStealMark() {
  if (EnableCoroutine && _coroutine != NULL) {
    assert(_coroutine->thread() == JavaThread::current(), "must be");
    bool eq = _enable_steal_count == _coroutine->dec_enable_steal_count();
    assert(eq, "enable_steal_count not balanced");
  }
}

static uintx chunk_wisp_post_steal_handles_do(Chunk *chunk, char *chunk_top, JavaThread *real_thread) {
  WispPostStealResource *bottom = (WispPostStealResource *) chunk->bottom();
  WispPostStealResource *top    = (WispPostStealResource *) chunk_top;
  uintx handles_visited = top - bottom;
  assert(top >= bottom && top <= (WispPostStealResource *) chunk->top(), "just checking");
  while (bottom < top) {
    // we overwrite our real thread or jnienv to every slot
    if (bottom->type == WispPostStealResource::ThreadRef) {
      bottom->update_thread_ref(real_thread);
    } else if (bottom->type == WispPostStealResource::JNIEnvRef) {
      bottom->update_jnienv_ref(real_thread->jni_environment());
    } else {
      ShouldNotReachHere();
    }
    bottom++;
  }
  return handles_visited;
}

void WispResourceArea::wisp_post_steal_handles_do(JavaThread *real_thread) {
  uintx handles_visited = 0;
  // First handle the current chunk. It is filled to the high water mark.
  handles_visited += chunk_wisp_post_steal_handles_do(_chunk, _hwm, real_thread);
  // Then handle all previous chunks. They are completely filled.
  Chunk *k = _first;
  while (k != _chunk) {
    handles_visited += chunk_wisp_post_steal_handles_do(k, k->top(), real_thread);
    k = k->next();
  }
}

class WispPostStealHandle : public StackObj {
private:
  void allocate_handle(WispResourceArea *area, Thread **t) {
    WispPostStealResource *src = area->real_allocate_handle();
    src->type = WispPostStealResource::ThreadRef;
    src->u.thread_ref = t;
  }
  void allocate_handle(WispResourceArea *area, JNIEnv **t) {
    WispPostStealResource *src = area->real_allocate_handle();
    src->type = WispPostStealResource::JNIEnvRef;
    src->u.jnienv_ref = t;
  }
public:
  template <typename T> explicit WispPostStealHandle(T* stackObj) {
    Thread *& ref = stackObj->thread_ref();
    JavaThread *thread = ((JavaThread *)ref);
    assert(thread->is_Java_thread(), "must be");
    allocate_handle(thread->current_coroutine()->wisp_post_steal_resource_area(), &ref);
  }
  explicit WispPostStealHandle(Thread ** thread_ptr) {
    JavaThread *thread = (JavaThread *)*thread_ptr;
    assert((*thread_ptr)->is_Java_thread(), "must be");
    allocate_handle(thread->current_coroutine()->wisp_post_steal_resource_area(), thread_ptr);
  }
  explicit WispPostStealHandle(JNIEnv ** jnienv_ptr) {
    JavaThread *thread = JavaThread::thread_from_jni_environment(*jnienv_ptr);
    assert(thread->is_Java_thread(), "must be");
    allocate_handle(thread->current_coroutine()->wisp_post_steal_resource_area(), jnienv_ptr);
  }
};

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,
        methodHandle & m1, methodHandle *& m2,
        JavaCallWrapper & w)
{
  initialize(th1);

  if (!_success)  return;
  WispPostStealHandle h((Thread **)&th1);
  WispPostStealHandle h1(&th2);
  WispPostStealHandle h2(&m1);
  WispPostStealHandle h3(m2);
  WispPostStealHandle h4(&w);
}

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(Thread *& th,
        methodHandle *m1, methodHandle *m2,
        JavaCallWrapper *w)
{
  initialize(*((JavaThread **)&th));

  if (!_success)  return;
  WispPostStealHandle h(&th);
  if (m1)  { WispPostStealHandle h(m1); }
  if (m2)  { WispPostStealHandle h(m2); }
  if (w)   { WispPostStealHandle h(w); }
}

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,
        JNIEnv *& env, ThreadInVMfromNative & tiv, HandleMarkCleaner & hmc,
        JavaThreadInObjectWaitState *jtios, vframeStream *f, methodHandle *m)
{
  initialize(th1);

  if (!_success)  return;
  WispPostStealHandle h((Thread **)&th1);
  WispPostStealHandle h1(&th2);
  WispPostStealHandle h2(&env);
  WispPostStealHandle h3(&tiv);
  WispPostStealHandle h4(&hmc);
  if (jtios) { WispPostStealHandle h(jtios); }
  if (f)     { WispPostStealHandle h(f); }
  if (m)     { WispPostStealHandle h(m); }
}

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(JavaThread *& th1, Thread *& th2,
        ThreadInVMfromJavaNoAsyncException & tiva, HandleMarkCleaner & hmc)
{
  initialize(th1);

  if (!_success)  return;
  WispPostStealHandle h((Thread **)&th1);
  WispPostStealHandle h1(&th2);
  WispPostStealHandle h2(&tiva);
  WispPostStealHandle h3(&hmc);
}

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(JavaThread *thread, ThreadBlockInVM & tbv)
{
  initialize(thread, true);

  if (!_success)  return;
  WispPostStealHandle h(&tbv);
}

WispPostStealHandleUpdateMark::WispPostStealHandleUpdateMark(JavaThread *&th)    // this is a special one
{
  initialize(th, true);

  if (!_success)  return;
  WispPostStealHandle h((Thread **)&th);
}

bool WispPostStealHandleUpdateMark::check(JavaThread *current, bool sp) {
  assert(current == JavaThread::current(), "must be");
  Coroutine *coroutine = current->current_coroutine();
  if (!EnableCoroutine ||
      coroutine == current->coroutine_list() ||
      // if we won't steal it, then no need to allocate handles.
      coroutine->enable_steal_count() != (sp ? current->java_call_counter()+1 : current->java_call_counter())) {
    _success = false;
    return false;
  }
  _success = true;
  return true;
}

WispPostStealHandleUpdateMark::~WispPostStealHandleUpdateMark() {
  if (!_success)  return;
  WispResourceArea* area = _area;   // help compilers with poor alias analysis
  assert(area == _coroutine->wisp_post_steal_resource_area(), "sanity check");
  assert(area->_nesting > 0, "must stack allocate HandleMarks" );
  debug_only(area->_nesting--);

  // Delete later chunks
  if( _chunk->next() ) {
    // reset arena size before delete chunks. Otherwise, the total
    // arena size could exceed total chunk size
    assert(area->size_in_bytes() > size_in_bytes(), "Sanity check");
    area->set_size_in_bytes(size_in_bytes());
    _chunk->next_chop();
  } else {
    assert(area->size_in_bytes() == size_in_bytes(), "Sanity check");
  }
  // Roll back arena to saved top markers
  area->_chunk = _chunk;
  area->_hwm = _hwm;
  area->_max = _max;
#ifdef ASSERT
  // clear out first chunk (to detect allocation bugs)
  if (ZapVMHandleArea) {
    memset(_hwm, badHandleValue, _max - _hwm);
  }
#endif

}

void Coroutine::initialize_coroutine_support(JavaThread* thread) {
  assert(EnableCoroutine, "Coroutine is disabled");
  guarantee(thread == JavaThread::current(), "sanity check");
  if (UseWispMonitor) {
    assert(thread->parker(), "sanity check");
    java_lang_Thread::set_park_event(
        thread->threadObj(), (uintptr_t) thread->parker());
  }

  EXCEPTION_MARK
  HandleMark hm(thread);
  Handle obj(thread, thread->threadObj());
  JavaValue result(T_VOID);

  JavaCalls::call_virtual(&result,
      obj,
      KlassHandle(thread, SystemDictionary::Thread_klass()),
      vmSymbols::initializeCoroutineSupport_method_name(),
      vmSymbols::void_method_signature(),
      thread);
}

WispClinitCounterMark::WispClinitCounterMark(Thread* th) {
  _thread = (JavaThread*)th;
  if (EnableCoroutine) {
    _thread->current_coroutine()->inc_clinit_call_count();
  }
}

WispClinitCounterMark::~WispClinitCounterMark() {
  if (EnableCoroutine) {
    _thread->current_coroutine()->dec_clinit_call_count();
  }
}
