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
#include "jfr/jni/jfrJavaSupport.hpp"
#include "jfr/utilities/jfrJavaLog.hpp"
#include "jfr/utilities/jfrLog.hpp"

void JfrJavaLog::subscribe_log_level(jobject log_tag, jint id, TRAPS) {
// jdk8 doesn't support log level, so do nothing
}

void JfrJavaLog::log(jint tag_set, jint level, jstring message, TRAPS) {
  DEBUG_ONLY(JfrJavaSupport::check_java_thread_in_vm(THREAD));
  if (message == NULL) {
    return;
  }
  ResourceMark rm(THREAD);
  const char* const s = JfrJavaSupport::c_str(message, CHECK);
  assert(s != NULL, "invariant");

  switch(level) {
  case LogLevel::Off:
    break;
  case LogLevel::Trace:
    log_trace(jfr)("%s", s);
    break;
  case LogLevel::Debug:
    log_debug(jfr)("%s", s);
    break;
  case LogLevel::Info:
    log_info(jfr)("%s", s);
    break;
  case LogLevel::Warning:
    log_warning(jfr)("%s", s);
    break;
  case LogLevel::Error:
    log_error(jfr)("%s", s);
    break;
  default:
    break;
  }
}
