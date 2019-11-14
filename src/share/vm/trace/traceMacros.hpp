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

#ifndef SHARE_VM_TRACE_TRACEMACROS_HPP
#define SHARE_VM_TRACE_TRACEMACROS_HPP

#include "utilities/macros.hpp"

typedef u8 traceid;

#if INCLUDE_TRACE

#define JDK_JFR_EVENT_SUBKLASS 16
#define JDK_JFR_EVENT_KLASS 32
#define EVENT_HOST_KLASS 64
#define IS_EVENT_KLASS(ptr) (((ptr)->trace_id() & (JDK_JFR_EVENT_KLASS | JDK_JFR_EVENT_SUBKLASS)) != 0)
#define IS_JDK_JFR_EVENT_SUBKLASS(ptr) (((ptr)->trace_id() & (JDK_JFR_EVENT_SUBKLASS)) != 0)

#define USED_BIT 1
#define METHOD_USED_BIT (USED_BIT << 2)
#define EPOCH_1_SHIFT 0
#define EPOCH_2_SHIFT 1
#define LEAKP_SHIFT 8

#define USED_EPOCH_1_BIT (USED_BIT << EPOCH_1_SHIFT)
#define USED_EPOCH_2_BIT (USED_BIT << EPOCH_2_SHIFT)
#define LEAKP_USED_EPOCH_1_BIT (USED_EPOCH_1_BIT << LEAKP_SHIFT)
#define LEAKP_USED_EPOCH_2_BIT (USED_EPOCH_2_BIT << LEAKP_SHIFT)
#define METHOD_USED_EPOCH_1_BIT (METHOD_USED_BIT << EPOCH_1_SHIFT)
#define METHOD_USED_EPOCH_2_BIT (METHOD_USED_BIT << EPOCH_2_SHIFT)
#define METHOD_AND_CLASS_IN_USE_BITS (METHOD_USED_BIT | USED_BIT)
#define METHOD_AND_CLASS_IN_USE_EPOCH_1_BITS (METHOD_AND_CLASS_IN_USE_BITS << EPOCH_1_SHIFT)
#define METHOD_AND_CLASS_IN_USE_EPOCH_2_BITS (METHOD_AND_CLASS_IN_USE_BITS << EPOCH_2_SHIFT)

#define ARRAY_OBJECT_SIZE_PLACE_HOLDER 0x1111baba

#define ANY_USED_BITS (USED_EPOCH_2_BIT         | \
                       USED_EPOCH_1_BIT         | \
                       METHOD_USED_EPOCH_2_BIT  | \
                       METHOD_USED_EPOCH_1_BIT  | \
                       LEAKP_USED_EPOCH_2_BIT   | \
                       LEAKP_USED_EPOCH_1_BIT)

#define TRACE_ID_META_BITS (EVENT_HOST_KLASS | JDK_JFR_EVENT_KLASS | JDK_JFR_EVENT_SUBKLASS | ANY_USED_BITS)
#define EVENT_THREAD_EXIT(thread) do { if (EnableJFR) JfrBackend::on_javathread_exit(thread); } while(0)
#define EVENT_THREAD_DESTRUCT(thread) do { if (EnableJFR) JfrBackend::on_thread_destruct(thread); } while(0)
#define TRACE_SUSPEND_THREAD(thread) do { if (EnableJFR) JfrBackend::on_javathread_suspend(thread); } while(0)
#define TRACE_INIT_ID(data) do { if (EnableJFR) JfrTraceId::assign(data); } while(0)
#define TRACE_REMOVE_ID(k) do { if (EnableJFR) JfrTraceId::remove(k); } while(0)
#define TRACE_RESTORE_ID(k) do { if (EnableJFR) JfrTraceId::restore(k); } while(0)
#define TRACE_KLASS_CREATION(k, p, t)  do { if (EnableJFR && k() != NULL && IS_EVENT_KLASS(k)) JfrEventClassTransformer::on_klass_creation(k, p, t); } while(0)
#define THREAD_TRACE_ID(thread) thread->trace_data()->thread_id()
#define TRACE_DEFINE_TRACE_ID_FIELD mutable traceid _trace_id

#define TRACE_DEFINE_TRACE_ID_METHODS \
  traceid trace_id() const { return _trace_id; } \
  traceid* const trace_id_addr() const { return &_trace_id; } \
  void set_trace_id(traceid id) const { _trace_id = id;}

class TraceFlag {
 private:
  mutable jbyte _flags;
 public:
  TraceFlag() : _flags(0) {}
  explicit TraceFlag(jbyte flags) : _flags(flags) {}
  void set_flag(jbyte flag) const {
    _flags |= flag;
  }
  void clear_flag(jbyte flag) const {
    _flags &= (~flag);
  }
  jbyte flags() const { return _flags; }
  bool is_set(jbyte flag) const {
    return (_flags & flag) != 0;
  }
  jbyte* const flags_addr() const{
    return &_flags;
  }
};

#define TRACE_DEFINE_FLAG mutable TraceFlag _trace_flags
#define TRACE_DEFINE_FLAG_ACCESSOR                 \
  void set_trace_flag(jbyte flag) const {          \
    _trace_flags.set_flag(flag);                   \
  }                                                \
  jbyte trace_flags() const {                      \
    return _trace_flags.flags();                   \
  }                                                \
  bool is_trace_flag_set(jbyte flag) const {       \
    return _trace_flags.is_set(flag);              \
  }                                                \
  jbyte* const trace_flags_addr() const {          \
    return _trace_flags.flags_addr();              \
  }

#define TRACE_FLUSH(E, thread) JfrEventConditionalFlush<E> flush(thread);
#define TRACE_STACKTRACE_MARK(thread) JfrStackTraceMark mark(thread);
#define TRACE_FLUSH_WITH_STACKTRACE(E, thread) JfrEventConditionalFlushWithStacktrace<E> flush(thread);

#define TRACE_DATA JfrThreadData
#define TRACE_START() EnableJFR ? Jfr::on_vm_start() : JNI_OK
#define TRACE_INITIALIZE() EnableJFR ? Jfr::on_vm_init() : JNI_OK
#define TRACE_ALLOCATION(obj, size, thread) ObjectSampleAssistance osa(obj, size, thread);

#define TRACE_WEAK_OOPS_DO(is_alive, f) do { if (EnableJFR) LeakProfiler::oops_do(is_alive, f); } while (0)

#define TRACE_VM_EXIT() do { if (EnableJFR) Jfr::on_vm_shutdown(); } while(0)
#define TRACE_VM_ERROR() do { if (EnableJFR) Jfr::on_vm_shutdown(true); } while(0)
#define TRACE_DEFINE_KLASS_TRACE_ID_OFFSET \
  static ByteSize trace_id_offset() { return in_ByteSize(offset_of(InstanceKlass, _trace_id)); }
#define TRACE_KLASS_TRACE_ID_OFFSET InstanceKlass::trace_id_offset()
#define TRACE_DEFINE_THREAD_TRACE_DATA_OFFSET \
  static ByteSize trace_data_offset() { return in_ByteSize(offset_of(Thread, _trace_data)) ;}
#define TRACE_THREAD_TRACE_DATA_OFFSET Thread::trace_data_offset()
#define TRACE_DEFINE_THREAD_DATA_WRITER_OFFSET \
  static ByteSize java_event_writer_offset() { return in_ByteSize(offset_of(TRACE_DATA, _java_event_writer)); }
#define TRACE_THREAD_DATA_WRITER_OFFSET \
  TRACE_DATA::java_event_writer_offset() + TRACE_THREAD_TRACE_DATA_OFFSET
#define TRACE_DEFINE_THREAD_ID_OFFSET \
  static ByteSize trace_id_offset() { return in_ByteSize(offset_of(TRACE_DATA, _trace_id)); }
#define TRACE_DEFINE_THREAD_ID_SIZE \
  static size_t trace_id_size() { return sizeof(traceid); }
#define TRACE_THREAD_TRACE_ID_OFFSET TRACE_DATA::trace_id_offset()
#define TRACE_THREAD_TRACE_ID_SHIFT 8
#define TRACE_THREAD_TRACE_ID_SIZE TRACE_DATA::trace_id_size()
#define TRACE_TIME_METHOD JfrTraceTime::time_function()
#define TRACE_TEMPLATES(template) \
   template(jdk_jfr_internal_JVM,          "jdk/jfr/internal/JVM")

#define TRACE_INTRINSICS(do_intrinsic, do_class, do_name, do_signature, do_alias)                            \
  do_intrinsic(_counterTime,        jdk_jfr_internal_JVM, counterTime_name, void_long_signature, F_SN)       \
    do_name(     counterTime_name,                             "counterTime")                                \
  do_intrinsic(_getClassId,         jdk_jfr_internal_JVM, getClassId_name, class_long_signature, F_SN)       \
    do_name(     getClassId_name,                              "getClassId")                                 \
  do_intrinsic(_getEventWriter,   jdk_jfr_internal_JVM, getEventWriter_name, void_object_signature, F_SN)    \
    do_name(     getEventWriter_name,                          "getEventWriter")                             \

#define TRACE_HAVE_INTRINSICS

extern "C" void JNICALL trace_register_natives(JNIEnv*, jclass);
#define TRACE_REGISTER_NATIVES ((void*)((address_word)(&trace_register_natives)))

#define TRACE_OPTO_SLOW_ALLOCATION_ENTER(is_array, thread) \
  AllocTracer::opto_slow_allocation_enter(is_array, thread)

#define TRACE_OPTO_SLOW_ALLOCATION_LEAVE(is_array, thread) \
  AllocTracer::opto_slow_allocation_leave(is_array, thread)

#define TRACE_SLOW_ALLOCATION(klass, obj, alloc_size, thread) \
  AllocTracer::send_slow_allocation_event(klass, obj, alloc_size, thread)

#define TRACE_DEFINE_THREAD_ALLOC_COUNT_OFFSET \
  static ByteSize alloc_count_offset() { return in_ByteSize(offset_of(TRACE_DATA, _alloc_count)); }
#define TRACE_THREAD_ALLOC_COUNT_OFFSET \
  (TRACE_DATA::alloc_count_offset() + TRACE_THREAD_TRACE_DATA_OFFSET)
#define TRACE_DEFINE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET \
  static ByteSize alloc_count_until_sample_offset() { return in_ByteSize(offset_of(TRACE_DATA, _alloc_count_until_sample)); }
#define TRACE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET \
  (TRACE_DATA::alloc_count_until_sample_offset() + TRACE_THREAD_TRACE_DATA_OFFSET)

#else // !INCLUDE_TRACE

#define EVENT_THREAD_EXIT(thread)
#define EVENT_THREAD_DESTRUCT(thread)
#define TRACE_KLASS_CREATION(k, p, t)
#define TRACE_KLASS_DEFINITION(k, t)

#define TRACE_INIT_ID(k)
#define TRACE_REMOVE_ID(k)
#define TRACE_RESTORE_ID(k)

#define TRACE_DATA TraceThreadData

#define THREAD_TRACE_ID(thread) ((traceid)thread->osthread()->thread_id())

#define TRACE_START() JNI_OK
#define TRACE_INITIALIZE() JNI_OK
#define TRACE_ALLOCATION(obj, size, thread)
#define TRACE_WEAK_OOPS_DO(is_alive, f)
#define TRACE_VM_EXIT()
#define TRACE_VM_ERROR()

#define TRACE_DEFINE_TRACE_ID_METHODS typedef int ___IGNORED_hs_trace_type1
#define TRACE_DEFINE_TRACE_ID_FIELD typedef int ___IGNORED_hs_trace_type2
#define TRACE_DEFINE_KLASS_TRACE_ID_OFFSET typedef int ___IGNORED_hs_trace_type3
#define TRACE_KLASS_TRACE_ID_OFFSET in_ByteSize(0); ShouldNotReachHere()
#define TRACE_DEFINE_THREAD_TRACE_DATA_OFFSET typedef int ___IGNORED_hs_trace_type4
#define TRACE_THREAD_TRACE_DATA_OFFSET in_ByteSize(0); ShouldNotReachHere()
#define TRACE_DEFINE_THREAD_TRACE_ID_OFFSET typedef int ___IGNORED_hs_trace_type5
#define TRACE_THREAD_TRACE_ID_OFFSET in_ByteSize(0); ShouldNotReachHere()
#define TRACE_THREAD_TRACE_ID_SHIFT
#define TRACE_DEFINE_THREAD_ID_SIZE typedef int ___IGNORED_hs_trace_type6
#define TRACE_DEFINE_THREAD_DATA_WRITER_OFFSET typedef int ___IGNORED_hs_trace_type7
#define TRACE_THREAD_DATA_WRITER_OFFSET in_ByteSize(0); ShouldNotReachHere()
#define TRACE_DEFINE_FLAG typedef int ___IGNORED_hs_trace_type8
#define TRACE_DEFINE_FLAG_ACCESSOR typedef int ___IGNORED_hs_trace_type9
#define TRACE_TEMPLATES(template)
#define TRACE_INTRINSICS(do_intrinsic, do_class, do_name, do_signature, do_alias)

#define TRACE_OPTO_SLOW_ALLOCATION_ENTER(is_array, thread)
#define TRACE_OPTO_SLOW_ALLOCATION_LEAVE(is_array, thread)
#define TRACE_SLOW_ALLOCATION(klass, obj, alloc_size, thread)
#define TRACE_DEFINE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET
#define TRACE_THREAD_ALLOC_COUNT_UNTIL_SAMPLE_OFFSET
#define TRACE_DEFINE_THREAD_ALLOC_COUNT_OFFSET
#define TRACE_THREAD_ALLOC_COUNT_OFFSET

#endif // INCLUDE_TRACE
#endif // SHARE_VM_TRACE_TRACEMACROS_HPP
