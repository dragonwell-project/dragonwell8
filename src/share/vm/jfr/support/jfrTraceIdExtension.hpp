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

#ifndef SHARE_VM_JFR_SUPPORT_JFRTRACEIDEXTENSION_HPP
#define SHARE_VM_JFR_SUPPORT_JFRTRACEIDEXTENSION_HPP

#include "jfr/recorder/checkpoint/types/traceid/jfrTraceId.hpp"

#define DEFINE_TRACE_ID_FIELD mutable traceid _trace_id

#define DEFINE_TRACE_ID_METHODS \
  traceid trace_id() const { return _trace_id; } \
  traceid* const trace_id_addr() const { return &_trace_id; } \
  void set_trace_id(traceid id) const { _trace_id = id; }

#define DEFINE_TRACE_ID_SIZE \
  static size_t trace_id_size() { return sizeof(traceid); }

#define INIT_ID(data) JfrTraceId::assign(data)
#define REMOVE_ID(k) JfrTraceId::remove(k);
#define RESTORE_ID(k) JfrTraceId::restore(k);

class JfrTraceFlag {
 private:
  mutable jshort _flags;
 public:
  JfrTraceFlag() : _flags(0) {}
  bool is_set(jshort flag) const {
    return (_flags & flag) != 0;
  }

  jshort flags() const {
    return _flags;
  }

  void set_flags(jshort flags) const {
    _flags = flags;
  }

  jbyte* flags_addr() const {
    return (jbyte*)&_flags;
  }

  jbyte* meta_addr() const {
#ifdef VM_LITTLE_ENDIAN
    return ((jbyte*)&_flags) + 1;
#else
    return (jbyte*)&_flags;
#endif
  }
};

#define DEFINE_TRACE_FLAG mutable JfrTraceFlag _trace_flags

#define DEFINE_TRACE_FLAG_ACCESSOR                 \
  bool is_trace_flag_set(jshort flag) const {      \
    return _trace_flags.is_set(flag);              \
  }                                                \
  jshort trace_flags() const {                     \
    return _trace_flags.flags();                   \
  }                                                \
  void set_trace_flags(jshort flags) const {       \
    _trace_flags.set_flags(flags);                 \
  }                                                \
  jbyte* trace_flags_addr() const {                \
    return _trace_flags.flags_addr();              \
  }                                                \
  jbyte* trace_meta_addr() const {                 \
    return _trace_flags.meta_addr();               \
  }

#define ARRAY_OBJECT_SIZE_PLACE_HOLDER 0x1111baba

#define TRACE_OPTO_SLOW_ALLOCATION_ENTER(is_array, thread) \
  AllocTracer::opto_slow_allocation_enter(is_array, thread)

#define TRACE_OPTO_SLOW_ALLOCATION_LEAVE(is_array, thread) \
  AllocTracer::opto_slow_allocation_leave(is_array, thread)

#define TRACE_SLOW_ALLOCATION(klass, obj, alloc_size, thread) \
  AllocTracer::send_slow_allocation_event(klass, obj, alloc_size, thread)

#define TRACE_DEFINE_THREAD_ALLOC_COUNT_OFFSET \
  static ByteSize alloc_count_offset() { return in_ByteSize(offset_of(JfrThreadLocal, _alloc_count)); }
#define TRACE_THREAD_ALLOC_COUNT_OFFSET \
  (JfrThreadLocal::alloc_count_offset() + Thread::jfr_thread_local_offset())
#define TRACE_DEFINE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET \
  static ByteSize alloc_count_until_sample_offset() { return in_ByteSize(offset_of(JfrThreadLocal, _alloc_count_until_sample)); }
#define TRACE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET \
  (JfrThreadLocal::alloc_count_until_sample_offset() + Thread::jfr_thread_local_offset())

#endif // SHARE_VM_JFR_SUPPORT_JFRTRACEIDEXTENSION_HPP
