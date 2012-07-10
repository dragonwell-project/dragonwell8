/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "assembler_aarch64.inline.hpp"
#include "runtime/icache.hpp"

extern void aarch64TestHook();

#define __ _masm->

int _flush_icache_stub_dummy(address addr, int lines, int magic)
{
  return magic;
}

void ICacheStubGenerator::generate_icache_flush(ICache::flush_icache_stub_t* flush_icache_stub) {

  aarch64TestHook();

#if 0
  // TODO -- this is just dummy code to get us bootstrapped far enough
  // to be able to test the Assembler.

  // we have to put come code into the buffer because the flush routine checks
  // that the first flush is for the current start address
  // the mark here ensures that the flush routine gets called on the way out

  StubCodeMark mark(this, "ICache", "flush_icache_stub");

  address start = __ pc();
  // generate more than 8 nops and then copy 8 words of the dummy x86 code
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();
  __ nop();

  address dummy = (address)_flush_icache_stub_dummy;
  memcpy(start, dummy, 8 * BytesPerWord);

  *flush_icache_stub =  (ICache::flush_icache_stub_t)start;

#endif
  Unimplemented();
}

#undef __
