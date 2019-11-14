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

#ifndef SHARE_VM_JFR_RECORDER_SERVICE_JFREVENT_HPP
#define SHARE_VM_JFR_RECORDER_SERVICE_JFREVENT_HPP

#include "jfr/recorder/jfrEventSetting.inline.hpp"
#include "jfr/recorder/jfrEventSetting.hpp"
#include "jfr/recorder/stacktrace/jfrStackTraceRepository.hpp"
#include "jfr/utilities/jfrTraceTime.hpp"
#include "jfr/writers/jfrNativeEventWriter.hpp"
#include "runtime/thread.hpp"
#include "trace/traceEvent.hpp"
#include "utilities/ticks.hpp"
#ifdef ASSERT
#include "utilities/bitMap.hpp"
#endif

#ifdef ASSERT
class JfrTraceEventVerifier {
  template <typename>
  friend class JfrTraceEvent;
 private:
  // verification of event fields
  BitMap::bm_word_t _verification_storage[1];
  BitMap _verification_bit_map;

  JfrTraceEventVerifier();
  void check(BitMap::idx_t field_idx) const;
  void set_field_bit(size_t field_idx);
  bool verify_field_bit(size_t field_idx) const;
};

#endif // ASSERT

template <typename T>
class JfrTraceEvent : public TraceEvent<T> {
 protected:
  JfrTraceEvent(EventStartTime timing=TIMED) : TraceEvent<T>(timing)
#ifdef ASSERT
   , _verifier()
#endif
   { }

 public:
  void set_starttime(const JfrTraceTime& time) {
    this->_startTime = time.value();
  }

  void set_endtime(const JfrTraceTime& time) {
    this->_endTime = time.value();
  }

  void set_starttime(const Ticks& time) {
    this->_startTime = JfrTraceTime::is_ft_enabled() ? time.ft_value() : time.value();
  }

  void set_endtime(const Ticks& time) {
    this->_endTime = JfrTraceTime::is_ft_enabled() ? time.ft_value() : time.value();
  }

 private:
  bool should_write() {
    if (T::isInstant || T::isRequestable || T::hasCutoff) {
      return true;
    }
    return (this->_endTime - this->_startTime) >= JfrEventSetting::threshold(T::eventId);
  }

  void writeEvent() {
    DEBUG_ONLY(assert_precondition();)
    Thread* const event_thread = Thread::current();
    JfrThreadData* const trace_data = event_thread->trace_data();
    JfrBuffer* const buffer = trace_data->native_buffer();
    if (buffer == NULL) {
      // most likely a pending OOM
      return;
    }
    JfrNativeEventWriter writer(buffer, event_thread);
    writer.write<u8>(T::eventId);
    assert(this->_startTime != 0, "invariant");
    writer.write(this->_startTime);
    if (!(T::isInstant || T::isRequestable) || T::hasCutoff) {
      assert(this->_endTime != 0, "invariant");
      writer.write(this->_endTime - this->_startTime);
    }
    if (T::hasThread) {
      writer.write(trace_data->thread_id());
    }
    if (T::hasStackTrace) {
      if (JfrEventSetting::has_stacktrace(T::eventId)) {
        if (trace_data->has_cached_stack_trace()) {
          writer.write(trace_data->cached_stack_trace_id());
        } else {
          writer.write(JfrStackTraceRepository::record(event_thread, 0, JfrEventSetting::stack_walk_mode(T::eventId)));
        }
      } else {
        writer.write<traceid>(0);
      }
    }
    // payload
    static_cast<T*>(this)->writeData(writer);
  }

#ifdef ASSERT
 private:
  // verification of event fields
  JfrTraceEventVerifier _verifier;

  void assert_precondition() {
    assert(T::eventId >= (TraceEventId)NUM_RESERVED_EVENTS, "event id underflow invariant");
    assert(T::eventId < MaxTraceEventId, "event id overflow invariant");
    DEBUG_ONLY(static_cast<T*>(this)->verify());
  }

 protected:
  void set_field_bit(size_t field_idx) {
    _verifier.set_field_bit(field_idx);
    // it is ok to reuse an already committed event
    // granted you provide new informational content
    this->_committed = false;
  }
  bool verify_field_bit(size_t field_idx) const {
    return _verifier.verify_field_bit(field_idx);
  }
#endif // ASSERT

  template <typename>
  friend class TraceEvent;
};

#endif // SHARE_VM_JFR_RECORDER_SERVICE_JFREVENT_HPP
