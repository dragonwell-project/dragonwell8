/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 2002, 2010, Oracle and/or its affiliates.
 * All rights reserved.
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

#ifndef CPU_AARCH64_VM_JAVAFRAMEANCHOR_AARCH64_HPP
#define CPU_AARCH64_VM_JAVAFRAMEANCHOR_AARCH64_HPP

private:

  // FP value associated with _last_Java_sp:
  intptr_t* volatile        _last_Java_fp;           // pointer is volatile not what it points to

public:
  // Each arch must define reset, save, restore
  // These are used by objects that only care about:
  //  1 - initializing a new state (thread creation, javaCalls)
  //  2 - saving a current state (javaCalls)
  //  3 - restoring an old state (javaCalls)

  void clear(void) {
    // clearing _last_Java_sp must be first
    _last_Java_sp = NULL;
    OrderAccess::release();
    _last_Java_fp = NULL;
    _last_Java_pc = NULL;
  }

  void copy(JavaFrameAnchor* src) {
    // n.b. the writes to fp and pc do not require any preceding
    // release(). when copying into the thread anchor, which only
    // happens under ~JavaCallWrapper(), sp will have been NULLed by a
    // call to zap() and the NULL write will have been published by a
    // fence in the state transition to in_vm. contrariwise, when
    // copying into the wrapper anchor, which only happens under
    // JavaCallWrapper(), there is no ordering requirement at all
    // since that object is thread local until the subsequent entry
    // into java. JavaCallWrapper() call clear() after copy() thus
    // ensuring that all 3 writes are visible() before the wrapper is
    // accessible to other threads.
    _last_Java_fp = src->_last_Java_fp;
    _last_Java_pc = src->_last_Java_pc;
    // Must be last so profiler will always see valid frame if
    // has_last_frame() is true
    OrderAccess::release();
    _last_Java_sp = src->_last_Java_sp;
  }

  bool walkable(void)                            { return _last_Java_sp != NULL && _last_Java_pc != NULL; }
  void make_walkable(JavaThread* thread);
  void capture_last_Java_pc(void);

  intptr_t* last_Java_sp(void) const             { return _last_Java_sp; }

  address last_Java_pc(void)                     { return _last_Java_pc; }

private:

  static ByteSize last_Java_fp_offset()          { return byte_offset_of(JavaFrameAnchor, _last_Java_fp); }

public:

  // n.b. set_last_Java_sp and set_last_Java_fp are never called
  // (which is good because they would need a preceding or following
  // call to OrderAccess::release() to make sure the writes are
  // visible in the correct order).
void set_last_Java_sp(intptr_t* sp)            { assert(false, "should not be called"); _last_Java_sp = sp; }

  intptr_t*   last_Java_fp(void)                     { return _last_Java_fp; }
  // Assert (last_Java_sp == NULL || fp == NULL)
  void set_last_Java_fp(intptr_t* fp)                { assert(false, "should not be called"); _last_Java_fp = fp; }

#endif // CPU_AARCH64_VM_JAVAFRAMEANCHOR_AARCH64_HPP
