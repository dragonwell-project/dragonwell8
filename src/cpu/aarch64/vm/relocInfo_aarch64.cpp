/*
 * Copyright (c) 1998, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/macroAssembler.hpp"
#include "code/relocInfo.hpp"
#include "nativeInst_aarch64.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/safepoint.hpp"


void Relocation::pd_set_data_value(address x, intptr_t o, bool verify_only) {
  MacroAssembler::pd_patch_instruction(addr(), x);
}


address Relocation::pd_call_destination(address orig_addr) {
  if (orig_addr != NULL) {
    return MacroAssembler::pd_call_destination(orig_addr);
  }
  return MacroAssembler::pd_call_destination(addr());
}


void Relocation::pd_set_call_destination(address x) {
  MacroAssembler::pd_patch_instruction(addr(), x);
}

address* Relocation::pd_address_in_code() {
  return (address*)(addr() + 8);
}


address Relocation::pd_get_address_from_code() {
  return MacroAssembler::pd_call_destination(addr());
}

int Relocation::pd_breakpoint_size() { Unimplemented(); return 0; }

void Relocation::pd_swap_in_breakpoint(address x, short* instrs, int instrlen) { Unimplemented(); }


void Relocation::pd_swap_out_breakpoint(address x, short* instrs, int instrlen) { Unimplemented(); }

void poll_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
  // fprintf(stderr, "Try to fix poll reloc at %p to %p\n", addr(), dest);
  if (NativeInstruction::is_adrp_at(addr())) {
    address old_addr = old_addr_for(addr(), src, dest);
    if (! os::is_poll_address(pd_call_destination(old_addr))) {
      fprintf(stderr, "  bollocks!\n");
      old_addr = old_addr_for(addr(), src, dest);
    }
    MacroAssembler::pd_patch_instruction(addr(), pd_call_destination(old_addr));
    if (! os::is_poll_address(pd_call_destination(addr()))) {
      fprintf(stderr, "  result at %p is %p\n", addr(), pd_call_destination(addr()));
      MacroAssembler::pd_patch_instruction(addr(), pd_call_destination(old_addr));
    }
  } else {
  }
}

void poll_return_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest)  {
  if (NativeInstruction::is_adrp_at(addr())) {
    address old_addr = old_addr_for(addr(), src, dest);
    MacroAssembler::pd_patch_instruction(addr(), pd_call_destination(old_addr));
  }
}

void metadata_Relocation::pd_fix_value(address x) {
}
