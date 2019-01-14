/*
 * Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "jfr/periodic/jfrThreadCPULoadEvent.hpp"
#include "jfr/jni/jfrJavaSupport.hpp"
#include "jfr/recorder/access/jfrbackend.hpp"
#include "jfr/recorder/access/jfrOptionSet.hpp"
#include "jfr/recorder/access/jfrThreadData.hpp"
#include "jfr/recorder/checkpoint/jfrCheckpointManager.hpp"
#include "jfr/recorder/checkpoint/constant/traceid/jfrTraceId.inline.hpp"
#include "jfr/recorder/storage/jfrStorage.hpp"
#include "jfr/recorder/stacktrace/jfrStackTraceRepository.hpp"
#include "memory/allocation.inline.hpp"
#include "runtime/os.hpp"
#include "runtime/thread.inline.hpp"

/* This data structure is per thread and only accessed by the thread itself, no locking required */
JfrThreadData::JfrThreadData() :
  _java_event_writer(NULL),
  _java_buffer(NULL),
  _native_buffer(NULL),
  _shelved_buffer(NULL),
  _stackframes(NULL),
  _trace_id(JfrTraceId::assign_thread_id()),
  _thread_cp(),
  _data_lost(0),
  _stack_trace_id(max_julong),
  _user_time(0),
  _cpu_time(0),
  _wallclock_time(os::javaTimeNanos()),
  _stack_trace_hash(0),
  _stackdepth(0),
  _entering_suspend_flag(0) {}

u8 JfrThreadData::add_data_lost(u8 value) {
  _data_lost += value;
  return _data_lost;
}

bool JfrThreadData::has_thread_checkpoint() const {
  return _thread_cp.valid();
}

void JfrThreadData::set_thread_checkpoint(const JfrCheckpointBlobHandle& ref) {
  assert(!_thread_cp.valid(), "invariant");
  _thread_cp = ref;
}

const JfrCheckpointBlobHandle& JfrThreadData::thread_checkpoint() const {
  return _thread_cp;
}

void JfrThreadData::on_exit(JavaThread* thread) {
  JfrCheckpointManager::write_thread_checkpoint(thread);
  JfrThreadCPULoadEvent::send_event_for_thread(thread);
}

void JfrThreadData::on_destruct(Thread* thread) {
  JfrThreadData* const thread_data = thread->trace_data();
  if (thread_data->has_native_buffer()) {
    release(thread_data->native_buffer(), thread);
  }
  if (thread_data->has_java_buffer()) {
    release(thread_data->java_buffer(), thread);
  }
  assert(thread_data->shelved_buffer() == NULL, "invariant");
  if (thread->trace_data()->has_java_event_writer()) {
    JfrJavaSupport::destroy_global_jni_handle(thread_data->java_event_writer());
  }
  destroy_stackframes(thread);
}

JfrBuffer* JfrThreadData::acquire(Thread* thread, size_t size) {
  return JfrStorage::acquire_thread_local(thread, size);
}

void JfrThreadData::release(JfrBuffer* buffer, Thread* thread) {
  assert(buffer != NULL, "invariant");
  JfrStorage::release_thread_local(buffer, thread);
}

JfrBuffer* JfrThreadData::install_native_buffer() const {
  assert(!has_native_buffer(), "invariant");
  _native_buffer = acquire(Thread::current());
  return _native_buffer;
}

JfrBuffer* JfrThreadData::install_java_buffer() const {
  assert(!has_java_buffer(), "invariant");
  assert(!has_java_event_writer(), "invariant");
  _java_buffer = acquire(Thread::current());
  return _java_buffer;
}

JfrStackFrame* JfrThreadData::install_stackframes() const {
  assert(_stackframes == NULL, "invariant");
  _stackdepth = (u4)JfrOptionSet::stackdepth();
  guarantee(_stackdepth > 0, "Stackdepth must be > 0");
  _stackframes = NEW_C_HEAP_ARRAY(JfrStackFrame, _stackdepth, mtTracing);
  return _stackframes;
}

void JfrThreadData::destroy_stackframes(Thread* thread) {
  assert(thread != NULL, "invariant");
  JfrStackFrame* frames = thread->trace_data()->stackframes();
  if (frames != NULL) {
    FREE_C_HEAP_ARRAY(JfrStackFrame, frames, mtTracing);
    thread->trace_data()->set_stackframes(NULL);
  }
}
