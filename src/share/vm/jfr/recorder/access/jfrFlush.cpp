/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "jfr/recorder/access/jfrFlush.hpp"
#include "jfr/recorder/access/jfrThreadData.hpp"
#include "jfr/recorder/jfrEventSetting.inline.hpp"
#include "jfr/recorder/storage/jfrStorage.hpp"
#include "jfr/recorder/stacktrace/jfrStackTraceRepository.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/debug.hpp"


JfrFlush::JfrFlush(JfrStorage::Buffer* old, size_t used, size_t requested, Thread* t) :
  _result(JfrStorage::flush(old, used, requested, true, t)) {
}

template <typename T>
class LessThanHalfBufferSize : AllStatic {
public:
  static bool evaluate(T* t) {
    assert(t != NULL, "invariant");
    return t->free_size() < t->size() / 2;
  }
};

template <typename T>
class LessThanSize : AllStatic {
 public:
  static bool evaluate(T* t, size_t size) {
    assert(t != NULL, "invariant");
    return t->free_size() < size;
  }
};

bool jfr_is_event_enabled(TraceEventId id) {
  return JfrEventSetting::is_enabled(id);
}

bool jfr_has_stacktrace_enabled(TraceEventId id) {
  return JfrEventSetting::has_stacktrace(id);
}

void jfr_conditional_flush(TraceEventId id, size_t size, Thread* t) {
  assert(jfr_is_event_enabled(id), "invariant");
  if (t->trace_data()->has_native_buffer()) {
    JfrStorage::Buffer* const buffer = t->trace_data()->native_buffer();
    if (LessThanSize<JfrStorage::Buffer>::evaluate(buffer, size)) {
      JfrFlush f(buffer, 0, 0, t);
    }
  }
}

bool jfr_save_stacktrace(Thread* t, StackWalkMode mode) {
  JfrThreadData* const trace_data = t->trace_data();
  if (trace_data->has_cached_stack_trace()) {
    return false; // no ownership
  }
  trace_data->set_cached_stack_trace_id(JfrStackTraceRepository::record(t, 0, mode));
  return true;
}

void jfr_clear_stacktrace(Thread* t) {
  t->trace_data()->clear_cached_stack_trace();
}
