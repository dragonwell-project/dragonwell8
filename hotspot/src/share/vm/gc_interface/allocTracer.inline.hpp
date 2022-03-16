/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef SHARE_VM_GC_INTERFACE_ALLOCTRACER_INLINE_HPP
#define SHARE_VM_GC_INTERFACE_ALLOCTRACER_INLINE_HPP

#include "gc_implementation/shared/gcId.hpp"
#include "jfr/jfrEvents.hpp"
#include "runtime/handles.hpp"
#include "utilities/globalDefinitions.hpp"
#include "gc_interface/allocTracer.hpp"
#if INCLUDE_JFR
#include "jfr/recorder/service/jfrOptionSet.hpp"
#include "jfr/objectprofiler/objectProfiler.hpp"
#endif // INCLUDE_JFR

inline void AllocTracer::opto_slow_allocation_enter(bool is_array, Thread* thread) {
#if INCLUDE_JFR
  if (JfrOptionSet::sample_object_allocations() &&
      ObjectProfiler::enabled()) {
    assert(thread != NULL, "Invariant");
    assert(thread->is_Java_thread(), "Invariant");
    thread->jfr_thread_local()->incr_alloc_count(1);
    if (is_array) {
      thread->jfr_thread_local()->set_cached_event_id(JfrOptoArrayObjectAllocationEvent);
    } else {
      thread->jfr_thread_local()->set_cached_event_id(JfrOptoInstanceObjectAllocationEvent);
    }
  }
#endif // INCLUDE_JFR
}

inline void AllocTracer::opto_slow_allocation_leave(bool is_array, Thread* thread) {
#if INCLUDE_JFR
#ifndef PRODUCT
  if (JfrOptionSet::sample_object_allocations()) {
    JfrThreadLocal* jfr_thread_local = thread->jfr_thread_local();
    assert(!jfr_thread_local->has_cached_event_id(), "Invariant");
    assert(jfr_thread_local->alloc_count_until_sample() >= jfr_thread_local->alloc_count(), "Invariant");
  }
#endif
#endif // INCLUDE_JFR
}

inline void AllocTracer::send_opto_array_allocation_event(KlassHandle klass, oop obj, size_t alloc_size, Thread* thread) {
  EventOptoArrayObjectAllocation event;
  if (event.should_commit()) {
    event.set_objectClass(klass());
    event.set_address(cast_from_oop<u8>(obj));
    event.set_allocationSize(alloc_size);
    event.commit();
  }
}

inline void AllocTracer::send_opto_instance_allocation_event(KlassHandle klass, oop obj, Thread* thread) {
  EventOptoInstanceObjectAllocation event;
  if (event.should_commit()) {
    event.set_objectClass(klass());
    event.set_address(cast_from_oop<u8>(obj));
    event.commit();
  }
}

inline void AllocTracer::send_slow_allocation_event(KlassHandle klass, oop obj, size_t alloc_size, Thread* thread) {
#if INCLUDE_JFR
  if (JfrOptionSet::sample_object_allocations()) {
    assert(thread != NULL, "Illegal parameter: thread is NULL");
    assert(thread == Thread::current(), "Invariant");
    if (thread->jfr_thread_local()->has_cached_event_id()) {
      assert(thread->is_Java_thread(), "Only allow to be called from java thread");
      jlong alloc_count = thread->jfr_thread_local()->alloc_count();
      jlong alloc_count_until_sample = thread->jfr_thread_local()->alloc_count_until_sample();
      assert(alloc_count > 0 || alloc_count <= alloc_count_until_sample, "Invariant");
      if (alloc_count == alloc_count_until_sample) {
        JfrEventId event_id = thread->jfr_thread_local()->cached_event_id();
        if (event_id ==JfrOptoArrayObjectAllocationEvent) {
          send_opto_array_allocation_event(klass, obj, alloc_size, thread);
        } else if(event_id == JfrOptoInstanceObjectAllocationEvent) {
          send_opto_instance_allocation_event(klass, obj, thread);
        } else {
            ShouldNotReachHere();
        }
        jlong interval = JfrOptionSet::object_allocations_sampling_interval();
        thread->jfr_thread_local()->incr_alloc_count_until_sample(interval);
      }
      thread->jfr_thread_local()->clear_cached_event_id();
    }
  }
#endif // INCLUDE_JFR
}

inline void AllocTracer::send_opto_fast_allocation_event(KlassHandle klass, oop obj, size_t alloc_size, Thread* thread) {
#if INCLUDE_JFR
  assert(JfrOptionSet::sample_object_allocations(), "Invariant");
  assert(thread != NULL, "Invariant");
  assert(thread->is_Java_thread(), "Invariant");
  assert(!thread->jfr_thread_local()->has_cached_event_id(), "Invariant");

  Klass* k = klass();
  if (k->oop_is_array()) {
    send_opto_array_allocation_event(klass, obj, alloc_size, thread);
  } else {
    send_opto_instance_allocation_event(klass, obj, thread);
  }
  jlong interval = JfrOptionSet::object_allocations_sampling_interval();
  thread->jfr_thread_local()->incr_alloc_count_until_sample(interval);
#endif // INCLUDE_JFR
}

#endif /* SHARE_VM_GC_INTERFACE_ALLOCTRACER_INLINE_HPP */
