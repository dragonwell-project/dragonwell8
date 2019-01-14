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

#ifndef SHARE_VM_JFR_UTILITIES_JFRTRACETIME_HPP
#define SHARE_VM_JFR_UTILITIES_JFRTRACETIME_HPP

#include "jni.h"
#include "utilities/macros.hpp"

#if INCLUDE_TRACE

class JfrTraceTime {
 private:
  static bool _ft_enabled;
  jlong _ticks;
  JfrTraceTime();
 public:
  static bool initialize();
  static bool is_ft_enabled() { return _ft_enabled; }
  static bool is_ft_supported();
  static jlong frequency();
  static JfrTraceTime now();
  static const void* time_function();

  JfrTraceTime(jlong ticks) : _ticks(ticks) {}
  jlong value() const { return _ticks; }
  void stamp();

  bool operator==(const JfrTraceTime& rhs) const {
    return _ticks == rhs._ticks;
  }
  bool operator!=(const JfrTraceTime& rhs) {
    return !operator==(rhs);
  }
  void operator+(jlong rhs_value) {
    _ticks += rhs_value;
  }
  bool operator>(const JfrTraceTime& rhs) const {
    return _ticks > rhs._ticks;
  }
  operator jlong() const {
    return _ticks;
  }
};

#else // !INCLUDE_TRACE

typedef jlong JfrTraceTime;

#endif // INCLUDE_TRACE
#endif // SHARE_VM_JFR_UTILITIES_JFRTRACETIME_HPP
