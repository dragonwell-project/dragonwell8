/*
 * Copyright (c) 1999, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/assembler.hpp"
#include "assembler_aarch64.inline.hpp"
#include "runtime/os.hpp"
#include "runtime/threadLocalStorage.hpp"

#if 0
void MacroAssembler::int3() {
  fixme()
}
#endif

// get_thread can be called anywhere inside generated code so we need
// to save whatever non-callee save context might get clobbered by the
// call to the C thread_local lookup call or, indeed, the call setup
// code. x86 appears to save C arg registers. aarch64 probably has
// enough scratch registers not to care about that so long as we
// ensure all usages of get_thread don't make any assumption that they
// will remain valid

void MacroAssembler::get_thread(Register dst) {
  // call pthread_getspecific
  // void * pthread_getspecific(pthread_key_t key);

  // save anything C regards as volatile that we might want to retain
  // save arg registers? not for now
  // save scratch registers? (r_scratch1/2) not for now
  // save method-local scratch registers
  push(r_cpool);
  push(r_monitors);
  push(r_locals);
  push(r_method);
  push(r_bcp);
  push(r_esp);
  mov(r_esp, sp);
  push(r_esp);
  andr(sp, r_esp, ~0xfL);
  mov(c_rarg0, ThreadLocalStorage::thread_index());
  mov(r_esp, CAST_FROM_FN_PTR(address, pthread_getspecific));
  call(r_esp);
  // restore pushed registers
  pop(sp);
  pop(r_esp);
  pop(r_bcp);
  pop(r_method);
  pop(r_locals);
  pop(r_monitors);
  pop(r_cpool);
  if (dst != c_rarg0) {
    mov(dst, c_rarg0);
  }
}
