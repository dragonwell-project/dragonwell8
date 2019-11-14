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

#ifndef SHARE_VM_JFR_RECORDER_ACCESS_JFRFLUSH_HPP
#define SHARE_VM_JFR_RECORDER_ACCESS_JFRFLUSH_HPP

#include "jfr/recorder/storage/jfrBuffer.hpp"
#include "jfr/recorder/jfrEventSetting.hpp"
#include "jfr/recorder/stacktrace/jfrStackTraceRepository.hpp"
#include "memory/allocation.hpp"
#include "tracefiles/traceEventIds.hpp"

class Thread;

class JfrFlush : public StackObj {
 public:
  typedef JfrBuffer Type;
  JfrFlush(Type* old, size_t used, size_t requested, Thread* t);
  Type* result() const { return _result; }
 private:
  Type* _result;
};

void jfr_conditional_flush(TraceEventId id, size_t size, Thread* t);
bool jfr_is_event_enabled(TraceEventId id);
bool jfr_has_stacktrace_enabled(TraceEventId id);
bool jfr_save_stacktrace(Thread* t, StackWalkMode mode);
void jfr_clear_stacktrace(Thread* t);

template <typename Event>
class JfrEventConditionalFlush {
 public:
  typedef JfrBuffer Type;
  JfrEventConditionalFlush(Thread* t) {
    if (jfr_is_event_enabled(Event::eventId)) {
      jfr_conditional_flush(Event::eventId, sizeof(Event), t);
    }
  }
};

template <typename Event>
class JfrEventConditionalFlushWithStacktrace : public JfrEventConditionalFlush<Event> {
  Thread* _t;
  bool _owner;
 public:
  JfrEventConditionalFlushWithStacktrace(Thread* t) : JfrEventConditionalFlush<Event>(t), _t(t), _owner(false) {
    if (Event::hasStackTrace && jfr_has_stacktrace_enabled(Event::eventId)) {
      _owner = jfr_save_stacktrace(t, JfrEventSetting::stack_walk_mode(Event::eventId));
    }
  }
  ~JfrEventConditionalFlushWithStacktrace() {
    if (_owner) {
      jfr_clear_stacktrace(_t);
    }
  }
};

#endif // SHARE_VM_JFR_RECORDER_ACCESS_JFRFLUSH_HPP
