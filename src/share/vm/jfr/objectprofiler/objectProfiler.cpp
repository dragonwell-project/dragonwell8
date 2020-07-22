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
#include "precompiled.hpp"
#include "runtime/vmThread.hpp"
#include "jfr/objectprofiler/objectProfiler.hpp"
#include "jfr/utilities/jfrTryLock.hpp"
#include "jfrfiles/jfrEventClasses.hpp"

volatile jint ObjectProfiler::_enabled = JNI_FALSE;
bool ObjectProfiler::_sample_instance_obj_alloc  = false;
bool ObjectProfiler::_sample_array_obj_alloc  = false;
#ifndef PRODUCT
volatile int ObjectProfiler::_try_lock = 0;
#endif

void ObjectProfiler::start(jlong event_id) {
#ifndef PRODUCT
  JfrTryLock try_lock(&_try_lock);
  assert(try_lock.has_lock(), "Not allow contention");
#endif
  if (EventOptoInstanceObjectAllocation::eventId == event_id) {
    if (!_sample_instance_obj_alloc) {
      _sample_instance_obj_alloc = true;
    }
  } else if (EventOptoArrayObjectAllocation::eventId == event_id) {
    if (!_sample_array_obj_alloc) {
      _sample_array_obj_alloc = true;
    }
  } else {
    ShouldNotReachHere();
  }
  if (enabled() == JNI_TRUE) {
    return;
  }
  OrderAccess::release_store((volatile jint*)&_enabled, JNI_TRUE);
}

void ObjectProfiler::stop(jlong event_id) {
#ifndef PRODUCT
  JfrTryLock try_lock(&_try_lock);
  assert(try_lock.has_lock(), "Not allow contention");
#endif
  if (enabled() == JNI_FALSE) {
    assert(!_sample_array_obj_alloc && !_sample_instance_obj_alloc, "Invariant");
    return;
  }
  if (EventOptoInstanceObjectAllocation::eventId == event_id) {
    if (_sample_instance_obj_alloc) {
      _sample_instance_obj_alloc = false;
    }
  } else if (EventOptoArrayObjectAllocation::eventId == event_id) {
    if (_sample_array_obj_alloc) {
      _sample_array_obj_alloc = false;
    }
  } else {
    ShouldNotReachHere();
  }
  bool should_enable = _sample_array_obj_alloc || _sample_instance_obj_alloc;
  if (should_enable) {
    return;
  }
  OrderAccess::release_store(&_enabled, JNI_FALSE);
}

jint ObjectProfiler::enabled() {
  return OrderAccess::load_acquire((volatile jint*)&_enabled);
}

void* ObjectProfiler::enabled_flag_address() {
  return (void*)&_enabled;
}
