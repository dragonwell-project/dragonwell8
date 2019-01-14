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
#include "jfr/leakprofiler/leakProfiler.hpp"
#include "jfr/leakprofiler/utilities/objectSampleAssistance.hpp"
#include "jfr/recorder/access/jfrThreadData.hpp"
#include "runtime/thread.hpp"

ObjectSampleAssistance::ObjectSampleAssistance(HeapWord* obj, size_t alloc_size, Thread* thread) : _trace_data(NULL) {
  if (LeakProfiler::is_running()) {
    assert(thread->is_Java_thread(), "invariant");
    _trace_data = thread->trace_data();
    LeakProfiler::sample(obj, alloc_size, (JavaThread*)thread);
  }
}

ObjectSampleAssistance::~ObjectSampleAssistance() {
  if (_trace_data != NULL) {
    _trace_data->clear_cached_stack_trace();
  }
}
