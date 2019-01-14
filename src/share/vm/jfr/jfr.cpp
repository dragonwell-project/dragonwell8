/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "jfr/recorder/access/jfrThreadData.hpp"
#include "jfr/recorder/jfrRecorder.hpp"
#include "jfr/recorder/checkpoint/jfrCheckpointManager.hpp"
#include "jfr/recorder/repository/jfrEmergencyDump.hpp"

bool Jfr::is_enabled() {
  return JfrRecorder::is_enabled();
}

bool Jfr::is_disabled() {
  return JfrRecorder::is_disabled();
}

bool Jfr::is_recording() {
  return JfrRecorder::is_recording();
}

jint Jfr::on_vm_init() {
  return JfrRecorder::on_vm_init() ? JNI_OK : JNI_ERR;
}

jint Jfr::on_vm_start() {
  return JfrRecorder::on_vm_start() ? JNI_OK : JNI_ERR;
}

void Jfr::on_unloading_classes() {
  if (JfrRecorder::is_created()) {
    JfrCheckpointManager::write_constant_tag_set_for_unloaded_classes();
  }
}

void Jfr::on_thread_exit(JavaThread* thread) {
  if (JfrRecorder::is_recording()) {
    JfrThreadData::on_exit(thread);
  }
}

void Jfr::on_thread_destruct(Thread* thread) {
  if (JfrRecorder::is_created()) {
    JfrThreadData::on_destruct(thread);
  }
}

void Jfr::on_vm_shutdown(bool exception_handler) {
  if (JfrRecorder::is_recording()) {
    JfrEmergencyDump::on_vm_shutdown(exception_handler);
  }
}
