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

  T* last() const   { return _last; }
  T* next() const   { return _next; }
};

class FrameClosure: public StackObj {
public:
  virtual void frames_do(frame* fr, RegisterMap* map) = 0;
};


class Coroutine: public CHeapObj<mtThread>, public DoublyLinkedList<Coroutine> {
public:
  enum CoroutineState {
    _onstack    = 0x00000001,
    _current    = 0x00000002,
    _dead       = 0x00000003,      // TODO is this really needed?
    _dummy      = 0xffffffff
  };

private:
  CoroutineState  _state;

  bool            _is_thread_coroutine;

  JavaThread*     _thread;
  CoroutineStack* _stack;

  ResourceArea*   _resource_area;
  HandleArea*     _handle_area;
  HandleMark*     _last_handle_mark;
#ifdef ASSERT
  int             _java_call_counter;
#endif

#ifdef _LP64
  intptr_t        _storage[2];
#endif

  // objects of this type can only be created via static functions
  Coroutine() { }
  virtual ~Coroutine() { }

  void frames_do(FrameClosure* fc);

public:
  void run(jobject coroutine);

  static Coroutine* create_thread_coroutine(JavaThread* thread, CoroutineStack* stack);
  static Coroutine* create_coroutine(JavaThread* thread, CoroutineStack* stack, oop coroutineObj);
  static void free_coroutine(Coroutine* coroutine, JavaThread* thread);

  CoroutineState state() const      { return _state; }
  void set_state(CoroutineState x)  { _state = x; }

  bool is_thread_coroutine() const  { return _is_thread_coroutine; }

  JavaThread* thread() const        { return _thread; }
  void set_thread(JavaThread* x)    { _thread = x; }

  CoroutineStack* stack() const     { return _stack; }

  ResourceArea* resource_area() const     { return _resource_area; }
  void set_resource_area(ResourceArea* x) { _resource_area = x; }

  HandleArea* handle_area() const         { return _handle_area; }
  void set_handle_area(HandleArea* x)     { _handle_area = x; }

  HandleMark* last_handle_mark() const    { return _last_handle_mark; }
  void set_last_handle_mark(HandleMark* x){ _last_handle_mark = x; }

#ifdef ASSERT
  int java_call_counter() const           { return _java_call_counter; }
  void set_java_call_counter(int x)       { _java_call_counter = x; }
#endif

  bool is_disposable();

  // GC support
  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);
  void nmethods_do(CodeBlobClosure* cf);
  void metadata_do(void f(Metadata*));
  void frames_do(void f(frame*, const RegisterMap* map));

  static ByteSize state_offset()              { return byte_offset_of(Coroutine, _state); }
  static ByteSize stack_offset()              { return byte_offset_of(Coroutine, _stack); }

  static ByteSize resource_area_offset()      { return byte_offset_of(Coroutine, _resource_area); }
  static ByteSize handle_area_offset()        { return byte_offset_of(Coroutine, _handle_area); }
  static ByteSize last_handle_mark_offset()   { return byte_offset_of(Coroutine, _last_handle_mark); }
#ifdef ASSERT
  static ByteSize java_call_counter_offset()  { return byte_offset_of(Coroutine, _java_call_counter); }
#endif

#ifdef _LP64
  static ByteSize storage_offset()            { return byte_offset_of(Coroutine, _storage); }
#endif

#if defined(_WINDOWS)
private:
  address _last_SEH;
public:
  static ByteSize last_SEH_offset()           { return byte_offset_of(Coroutine, _last_SEH); }
#endif
};

class CoroutineStack: public CHeapObj<mtThread>, public DoublyLinkedList<CoroutineStack> {
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

#endif // SHARE_VM_RUNTIME_COROUTINE_HPP
