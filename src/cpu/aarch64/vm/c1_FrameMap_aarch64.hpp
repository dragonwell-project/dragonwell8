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

#ifndef CPU_AARCH64_VM_C1_FRAMEMAP_AARCH64_HPP
#define CPU_AARCH64_VM_C1_FRAMEMAP_AARCH64_HPP

//  On i486 the frame looks as follows:
//
//  +-----------------------------+---------+----------------------------------------+----------------+-----------
//  | size_arguments-nof_reg_args | 2 words | size_locals-size_arguments+numreg_args | _size_monitors | spilling .
//  +-----------------------------+---------+----------------------------------------+----------------+-----------
//
//  The FPU registers are mapped with their offset from TOS; therefore the
//  status of FPU stack must be updated during code emission.

 public:
  static const int pd_c_runtime_reserved_arg_size;

  enum {
    first_available_sp_in_frame = 0,
    frame_pad_in_bytes = 16,
    nof_reg_args = 6
  };

 private:
 public:
  static LIR_Opr r0_opr;

  static LIR_Opr as_long_opr(Register r) {
    Unimplemented();
    return r0_opr;
  }
  static LIR_Opr as_pointer_opr(Register r) {
    Unimplemented();
    return r0_opr;
  }

  // VMReg name for spilled physical FPU stack slot n
  static VMReg fpu_regname (int n);

  static bool is_caller_save_register (LIR_Opr opr) { return true; }
  static bool is_caller_save_register (Register r) { return true; }

static int nof_caller_save_cpu_regs() { Unimplemented(); return 0; }
static int last_cpu_reg()             { Unimplemented(); return 0 ; }
static int last_byte_reg()            { Unimplemented(); return 0; }

#endif // CPU_AARCH64_VM_C1_FRAMEMAP_AARCH64_HPP

