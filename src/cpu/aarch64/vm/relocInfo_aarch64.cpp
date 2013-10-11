/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1998, 2011, Oracle and/or its affiliates.
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

void poll_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
  // fprintf(stderr, "Try to fix poll reloc at %p to %p\n", addr(), dest);
  if (NativeInstruction::maybe_cpool_ref(addr())) {
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
  if (NativeInstruction::maybe_cpool_ref(addr())) {
    address old_addr = old_addr_for(addr(), src, dest);
    MacroAssembler::pd_patch_instruction(addr(), pd_call_destination(old_addr));
  }
}

void metadata_Relocation::pd_fix_value(address x) {
}

// We have a relocation that points to a pair of instructions that
// load a constant from the constant pool.  These are
// ARDP; LDR reg [reg, #ofs].  However, until the constant is resolved
// the first instruction may be a branch to a resolver stub, and the
// resolver stub contains a copy of the ADRP that will replace the
// branch instruction.
//
// So, when we relocate this code we have to adjust the offset in the
// LDR instruction and the page offset in the copy of the ADRP
// instruction that will overwrite the branch instruction.  This is
// done by Runtime1::patch_code_aarch64.

void section_word_Relocation::fix_relocation_after_move(const CodeBuffer* src, CodeBuffer* dest) {
  unsigned insn1 = *(unsigned*)addr();
  if (! (Instruction_aarch64::extract(insn1, 30, 26) == 0b00101)) {
    // Unconditional branch (immediate)
    internal_word_Relocation::fix_relocation_after_move(src, dest);
    return;
  }

  address new_address = target();
#ifdef ASSERT
  // Make sure this really is a cpool address
  address old_cpool_start = const_cast<CodeBuffer*>(src)->consts()->start();
  address old_cpool_end = const_cast<CodeBuffer*>(src)->consts()->end();
  address new_cpool_start =  const_cast<CodeBuffer*>(dest)->consts()->start();
  address new_cpool_end =  const_cast<CodeBuffer*>(dest)->consts()->end();
  address old_address = old_addr_for(target(), src, dest);
  assert(new_address >= new_cpool_start
	 && new_address < new_cpool_end,
	 "should be");
  assert(old_address >= old_cpool_start
	 && old_address < old_cpool_end,
	 "should be");
#endif

  address stub_location = pd_call_destination(addr());
  unsigned char* byte_count = (unsigned char*) (stub_location - 1);
  unsigned char* byte_skip = (unsigned char*) (stub_location - 2);
  address copy_buff = stub_location - *byte_skip - *byte_count;
  unsigned insn3 = *(unsigned*)copy_buff;

  if (NearCpool) {
    int offset = new_address - addr();
    Instruction_aarch64::spatch(copy_buff, 23, 5, offset >> 2);
  } else {
    // Unconditional branch (immediate)
    unsigned insn2 = ((unsigned*)addr())[1];
    if (Instruction_aarch64::extract(insn2, 29, 24) == 0b111001) {
      // Load/store register (unsigned immediate)
      unsigned size = Instruction_aarch64::extract(insn2, 31, 30);

      // Offset of address in a 4k page
      uint64_t new_offset = (uint64_t)target() & ((1<<12) - 1);
      // Fix the LDR instruction's offset
      Instruction_aarch64::patch(addr() + sizeof (unsigned),
				 21, 10, new_offset >> size);

      assert(Instruction_aarch64::extract(insn3, 28, 24) == 0b10000
	     && Instruction_aarch64::extract(insn3, 31, 31),
	     "instruction should be an ADRP");

      uint64_t insn_page = (uint64_t)addr() >> 12;
      uint64_t target_page = (uint64_t)target() >> 12;
      int page_offset = target_page - insn_page;
      int page_offset_lo = page_offset & 3;
      page_offset >>= 2;
      Instruction_aarch64::spatch(copy_buff, 23, 5, page_offset);
      Instruction_aarch64::patch(copy_buff, 30, 29, page_offset_lo);

      // Phew.
    }
  }
}
