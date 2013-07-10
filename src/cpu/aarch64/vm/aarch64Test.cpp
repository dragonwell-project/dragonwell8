/*
 * Copyright (c) 2013, Red Hat Inc.
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
 */

#include <stdlib.h>

#include "precompiled.hpp"
#include "code/codeBlob.hpp"
#include "asm/macroAssembler.hpp"

// hook routine called during JVM bootstrap to test AArch64 assembler

extern "C" void entry(CodeBuffer*);

void aarch64TestHook()
{
  BufferBlob* b = BufferBlob::create("aarch64Test", 500000);
  CodeBuffer code(b);
  MacroAssembler _masm(&code);
  entry(&code);
  // dive now before we hit all the Unimplemented() calls
  // exit(0);

#if 0
  // old test code to compute sum of squares
  enum { r0, r1, r2, r3, r4, LR = 30 };

  address entry = __ pc();

  __ _mov_imm(r0, 100);
  address loop = __ pc();
  __ _sub_imm(r0, r0, 1);
  __ _cbnz(r0, loop);
  // __ _br(LR);

  char stack[4096];
  unsigned long memory[100];

  __ _mov_imm(r0, 1);
  __ _mov_imm(r4, 100);
  loop = __ pc();
  __ _mov(r1, r0);
  __ _mul(r2, r1, r1);
  __ _str_post(r2, r3, 8);
  __ _add_imm(r0, r0, 1);
  __ _sub_imm(r4, r4, 1);
  __ _cbnz(r4, loop);
  __ _br(LR);
  
  Disassembler::decode(entry, __ pc());

  sim.init((u_int64_t)entry, (u_int64_t)stack + sizeof stack,
	   (u_int64_t)stack);
  sim.getCPUState().xreg((GReg)r3, 0) = (u_int64_t)memory;
  sim.run();
  printf("Table of squares:\n");
  for (int i = 0; i < 100; i++)
    printf("  %d\n", memory[i]);
#endif
}
