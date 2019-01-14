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

#ifndef SHARE_VM_JFR_CHECKPOINT_CONSTANT_TRACEID_JFRTRACEIDEPOCH_HPP
#define SHARE_VM_JFR_CHECKPOINT_CONSTANT_TRACEID_JFRTRACEIDEPOCH_HPP

#include "memory/allocation.hpp"
#include "jfr/utilities/jfrTypes.hpp"
#include "trace/traceMacros.hpp"

class JfrTraceIdEpoch : AllStatic {
  friend class JfrCheckpointManager;
 private:
  static bool _epoch_state;
  static void shift_epoch();

 public:
  static bool epoch() {
    return _epoch_state;
  }

  static jlong epoch_address() {
    return (jlong)&_epoch_state;
  }

  static u1 current() {
    return _epoch_state ? (u1)1 : (u1)0;
  }

  static u1 previous() {
    return _epoch_state ? (u1)0 : (u1)1;
  }

  static traceid in_use_this_epoch_bit() {
    return _epoch_state ? USED_EPOCH_2_BIT : USED_EPOCH_1_BIT;
  }

  static traceid in_use_prev_epoch_bit() {
    return _epoch_state ? USED_EPOCH_1_BIT : USED_EPOCH_2_BIT;
  }

  static traceid leakp_in_use_this_epoch_bit() {
    return _epoch_state ? LEAKP_USED_EPOCH_2_BIT : LEAKP_USED_EPOCH_1_BIT;
  }

  static traceid leakp_in_use_prev_epoch_bit() {
    return _epoch_state ? LEAKP_USED_EPOCH_1_BIT : LEAKP_USED_EPOCH_2_BIT;
  }

  static traceid method_in_use_this_epoch_bit() {
    return _epoch_state ? METHOD_USED_EPOCH_2_BIT : METHOD_USED_EPOCH_1_BIT;
  }

  static traceid method_in_use_prev_epoch_bit() {
    return _epoch_state ? METHOD_USED_EPOCH_1_BIT : METHOD_USED_EPOCH_2_BIT;
  }

  static traceid method_and_class_in_use_this_epoch_bits() {
    return _epoch_state ? METHOD_AND_CLASS_IN_USE_EPOCH_2_BITS : METHOD_AND_CLASS_IN_USE_EPOCH_1_BITS;
  }

  static traceid method_and_class_in_use_prev_epoch_bits() {
    return _epoch_state ? METHOD_AND_CLASS_IN_USE_EPOCH_1_BITS :  METHOD_AND_CLASS_IN_USE_EPOCH_2_BITS;
  }
};

#endif // SHARE_VM_JFR_CHECKPOINT_CONSTANT_TRACEID_JFRTRACEIDEPOCH_HPP
