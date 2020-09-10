/*
 * Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JFR_SUPPORT_JFRTHREADLOCAL_HPP
#define SHARE_VM_JFR_SUPPORT_JFRTHREADLOCAL_HPP

#include "jfr/utilities/jfrBlob.hpp"
#include "jfr/utilities/jfrTypes.hpp"

class JavaThread;
class JfrBuffer;
class JfrStackFrame;
class Thread;

class JfrThreadLocal {
 private:
  jobject _java_event_writer;
  mutable JfrBuffer* _java_buffer;
  mutable JfrBuffer* _native_buffer;
  JfrBuffer* _shelved_buffer;
  mutable JfrStackFrame* _stackframes;
  mutable traceid _trace_id;
  JfrBlobHandle _thread;
  u8 _data_lost;
  traceid _stack_trace_id;
  jlong _user_time;
  jlong _cpu_time;
  jlong _wallclock_time;
  unsigned int _stack_trace_hash;
  mutable u4 _stackdepth;
  volatile jint _entering_suspend_flag;
  bool _excluded;
  bool _dead;
  traceid _parent_trace_id;
  // Jfr callstack collection relies on vframeStream.
  // But the bci of top frame can not be determined by vframeStream in some scenarios.
  // For example, in the opto CallLeafNode runtime call of
  // OptoRuntime::jfr_fast_object_alloc_C, the top frame bci
  // returned by vframeStream is always invalid. This is largely due to the oopmap that
  // is not correctly granted ( refer to PhaseMacroExpand::expand_allocate_common to get more details ).
  // The opto fast path object allocation tracing occurs in the opto CallLeafNode,
  // which has been broken by invalid top frame bci.
  // To fix this, we get the top frame bci in opto compilation phase
  // and pass it as parameter to runtime call. Our implementation will replace the invalid top
  // frame bci with cached_top_frame_bci.
  jint _cached_top_frame_bci;
  jlong _alloc_count;
  jlong _alloc_count_until_sample;
  // This field is used to help to distinguish the object allocation request source.
  // For example, for object allocation slow path, we trace it in CollectedHeap::obj_allocate.
  // But in CollectedHeap::obj_allocate, it is impossible to determine where the allocation request
  // is from,  which could be from c1, opto, or even interpreter.
  // We save this infomation in _event_id, which later can be retrieved in
  // CollecetedHeap::obj_allocate to identify the real allocation request source.
  JfrEventId _cached_event_id;

  JfrBuffer* install_native_buffer() const;
  JfrBuffer* install_java_buffer() const;
  JfrStackFrame* install_stackframes() const;
  void release(Thread* t);
  static void release(JfrThreadLocal* tl, Thread* t);

 public:
  JfrThreadLocal();

  JfrBuffer* native_buffer() const {
    return _native_buffer != NULL ? _native_buffer : install_native_buffer();
  }

  bool has_native_buffer() const {
    return _native_buffer != NULL;
  }

  void set_native_buffer(JfrBuffer* buffer) {
    _native_buffer = buffer;
  }

  JfrBuffer* java_buffer() const {
    return _java_buffer != NULL ? _java_buffer : install_java_buffer();
  }

  bool has_java_buffer() const {
    return _java_buffer != NULL;
  }

  void set_java_buffer(JfrBuffer* buffer) {
    _java_buffer = buffer;
  }

  JfrBuffer* shelved_buffer() const {
    return _shelved_buffer;
  }

  void shelve_buffer(JfrBuffer* buffer) {
    _shelved_buffer = buffer;
  }

  bool has_java_event_writer() const {
    return _java_event_writer != NULL;
  }

  jobject java_event_writer() {
    return _java_event_writer;
  }

  void set_java_event_writer(jobject java_event_writer) {
    _java_event_writer = java_event_writer;
  }

  JfrStackFrame* stackframes() const {
    return _stackframes != NULL ? _stackframes : install_stackframes();
  }

  void set_stackframes(JfrStackFrame* frames) {
    _stackframes = frames;
  }

  u4 stackdepth() const;

  void set_stackdepth(u4 depth) {
    _stackdepth = depth;
  }

  traceid thread_id() const {
    return _trace_id;
  }

  void set_thread_id(traceid thread_id) {
    _trace_id = thread_id;
  }

  traceid parent_thread_id() const {
    return _parent_trace_id;
  }

  void set_cached_stack_trace_id(traceid id, unsigned int hash = 0) {
    _stack_trace_id = id;
    _stack_trace_hash = hash;
  }

  bool has_cached_stack_trace() const {
    return _stack_trace_id != max_julong;
  }

  void clear_cached_stack_trace() {
    _stack_trace_id = max_julong;
    _stack_trace_hash = 0;
  }

  traceid cached_stack_trace_id() const {
    return _stack_trace_id;
  }

  unsigned int cached_stack_trace_hash() const {
    return _stack_trace_hash;
  }

  void set_trace_block() {
    _entering_suspend_flag = 1;
  }

  void clear_trace_block() {
    _entering_suspend_flag = 0;
  }

  bool is_trace_block() const {
    return _entering_suspend_flag != 0;
  }

  u8 data_lost() const {
    return _data_lost;
  }

  u8 add_data_lost(u8 value);

  jlong get_user_time() const {
    return _user_time;
  }

  void set_user_time(jlong user_time) {
    _user_time = user_time;
  }

  jlong get_cpu_time() const {
    return _cpu_time;
  }

  void set_cpu_time(jlong cpu_time) {
    _cpu_time = cpu_time;
  }

  jlong get_wallclock_time() const {
    return _wallclock_time;
  }

  void set_wallclock_time(jlong wallclock_time) {
    _wallclock_time = wallclock_time;
  }

  traceid trace_id() const {
    return _trace_id;
  }

  traceid* const trace_id_addr() const {
    return &_trace_id;
  }

  void set_trace_id(traceid id) const {
    _trace_id = id;
  }

  bool is_excluded() const {
    return _excluded;
  }

  bool is_dead() const {
    return _dead;
  }

  void set_cached_top_frame_bci(jint bci) {
    _cached_top_frame_bci = bci;
  }

  bool has_cached_top_frame_bci() const {
    return _cached_top_frame_bci != max_jint;
  }

  jint cached_top_frame_bci() const {
    return _cached_top_frame_bci;
  }

  void clear_cached_top_frame_bci() {
    _cached_top_frame_bci = max_jint;
  }

  jlong alloc_count() const {
    return _alloc_count;
  }

  void incr_alloc_count(jlong delta) {
    _alloc_count += delta;
  }

  jlong alloc_count_until_sample() const {
    return _alloc_count_until_sample;
  }

  void incr_alloc_count_until_sample(jlong delta) {
    _alloc_count_until_sample += delta;
  }

  void set_cached_event_id(JfrEventId event_id) {
    _cached_event_id = event_id;
  }

  JfrEventId cached_event_id() const {
    return _cached_event_id;
  }

  bool has_cached_event_id() const {
    return _cached_event_id != MaxJfrEventId;
  }

  void clear_cached_event_id() {
    _cached_event_id = MaxJfrEventId;
  }

  bool has_thread_blob() const;
  void set_thread_blob(const JfrBlobHandle& handle);
  const JfrBlobHandle& thread_blob() const;

  static void exclude(Thread* t);
  static void include(Thread* t);

  static void on_start(Thread* t);
  static void on_exit(Thread* t);

  // Code generation
  static ByteSize trace_id_offset();
  static ByteSize java_event_writer_offset();

  TRACE_DEFINE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET;
  TRACE_DEFINE_THREAD_ALLOC_COUNT_OFFSET;
};

#endif // SHARE_VM_JFR_SUPPORT_JFRTHREADLOCAL_HPP
