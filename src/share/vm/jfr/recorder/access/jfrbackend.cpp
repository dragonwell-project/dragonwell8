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
#include "jfr/jfr.hpp"
#include "jfr/periodic/sampling/jfrThreadSampler.hpp"
#include "jfr/recorder/access/jfrbackend.hpp"
#include "jfr/recorder/jfrEventSetting.inline.hpp"

void JfrBackend::on_javathread_exit(JavaThread *thread) {
  Jfr::on_thread_exit(thread);
}

void JfrBackend::on_thread_destruct(Thread* thread) {
  // vm thread shouldn't call Jfr::on_thread_destruct(), ref jdk11.
  if (thread == VMThread::vm_thread()) {
    return;
  }
  Jfr::on_thread_destruct(thread);
}

void JfrBackend::on_javathread_suspend(JavaThread* thread) {
  JfrThreadSampler::on_javathread_suspend(thread);
}

bool JfrBackend::enabled() {
  return Jfr::is_enabled();
}

bool JfrBackend::is_event_enabled(TraceEventId event_id) {
  return JfrEventSetting::is_enabled(event_id);
}

bool JfrBackend::is_stacktrace_enabled(TraceEventId event_id) {
  return JfrEventSetting::has_stacktrace(event_id);
}

jlong JfrBackend::threshold(TraceEventId event_id) {
  return JfrEventSetting::threshold(event_id);
}

void JfrBackend::on_unloading_classes() {
  Jfr::on_unloading_classes();
}

JfrTraceTime JfrBackend::time() {
  return JfrTraceTime::now();
}
