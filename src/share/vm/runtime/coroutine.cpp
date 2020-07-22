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
#include "runtime/coroutine.hpp"
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
#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
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

  intptr_t** d = (intptr_t**)stack->stack_base();
  // Make the 16 bytes(original is 8*5=40 bytes) alignation which is required by some instructions like movaps otherwise we will incur a crash.
  *(--d) = NULL;
  *(--d) = NULL;
  jobject obj = JNIHandles::make_global(coroutineObj);
  *(--d) = (intptr_t*)obj;
  *(--d) = (intptr_t*)coro;
  *(--d) = NULL;
  *(--d) = (intptr_t*)coroutine_start;
  *(--d) = NULL;

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
#ifdef ASSERT
  coro->_java_call_counter = 0;
#endif
#if defined(_WINDOWS)
  coro->_last_SEH = NULL;
#endif
  return coro;
}

Coroutine::~Coroutine() {
  remove_from_list(_thread->coroutine_list());
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
  if (_state == _onstack &&_handle_area != NULL) {
    DEBUG_CORO_ONLY(tty->print_cr("collecting handle area %08x", _handle_area));
    _handle_area->oops_do(f);
    _active_handles->oops_do(f);
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
  //_handle_area == NULL indicates this coroutine has not been initialized,
  //we should delete it directly.
  return _handle_area == NULL;
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

  ThreadLocalStorage::add_coroutine_stack(thread, stack->stack_base(), stack->stack_size());
  DEBUG_CORO_ONLY(tty->print("created coroutine stack at %08x with stack size %i (real size: %i)\n", stack->_stack_base, size, stack->_stack_size));
  return stack;
}

void CoroutineStack::free_stack(CoroutineStack* stack, JavaThread* thread) {
  if (stack->is_thread_stack()) {
    delete stack;
    return;
  }
  ThreadLocalStorage::remove_coroutine_stack(thread, stack->stack_base(), stack->stack_size());

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
  if (fp != NULL) {
    address pc = ((address*)_last_sp)[1];
    intptr_t* sp = ((intptr_t*)_last_sp) + 2;

    frame fr(sp, fp, pc);
    StackFrameStream fst(_thread, fr);
    fst.register_map()->set_location(rbp->as_VMReg(), (address)_last_sp);
    fst.register_map()->set_include_argument_oops(false);
    for(; !fst.is_done(); fst.next()) {
      fc->frames_do(fst.current(), fst.register_map());
    }
  }
}

frame CoroutineStack::last_frame(Coroutine* coro, RegisterMap& map) const {
  DEBUG_CORO_ONLY(tty->print_cr("last_frame CoroutineStack"));

  intptr_t* fp = ((intptr_t**)_last_sp)[0];
  assert(fp != NULL, "coroutine with NULL fp");

  address pc = ((address*)_last_sp)[1];
  intptr_t* sp = ((intptr_t*)_last_sp) + 2;

  return frame(sp, fp, pc);
}
