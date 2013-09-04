/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates.
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
#include "memory/resourceArea.hpp"
#include "nativeInst_aarch64.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/ostream.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#endif

void NativeInstruction::wrote(int offset) {
  // FIXME: Native needs ISB here
; }


void NativeCall::verify() { ; }

address NativeCall::destination() const {
  return instruction_address() + displacement();
}

void NativeCall::print() { Unimplemented(); }

// Inserts a native call instruction at a given pc
void NativeCall::insert(address code_pos, address entry) { Unimplemented(); }

// MT-safe patching of a call instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeCall::replace_mt_safe(address instr_addr, address code_buffer) { Unimplemented(); }


void NativeMovConstReg::verify() {
  // make sure code pattern is actually mov reg64, imm64 instructions
}

intptr_t NativeMovConstReg::data() const {
  // das(uint64_t(instruction_address()),2);
  address addr = MacroAssembler::pd_call_destination(instruction_address());
  if (maybe_cpool_ref(instruction_address())) {
    return *(intptr_t*)addr;
  } else {
    return (intptr_t)addr;
  }
}

void NativeMovConstReg::set_data(intptr_t x) {
  if (maybe_cpool_ref(instruction_address())) {
    address addr = MacroAssembler::pd_call_destination(instruction_address());
    *(intptr_t*)addr = x;
  } else {
    MacroAssembler::pd_patch_instruction(instruction_address(), (address)x);
  }
};


void NativeMovConstReg::print() {
  tty->print_cr(PTR_FORMAT ": mov reg, " INTPTR_FORMAT,
                instruction_address(), data());
}

//-------------------------------------------------------------------

int NativeMovRegMem::instruction_start() const { Unimplemented(); return 0; }

address NativeMovRegMem::instruction_address() const      { return addr_at(instruction_offset); }

address NativeMovRegMem::next_instruction_address() const { Unimplemented(); return 0; }

int NativeMovRegMem::offset() const  {
  address pc = instruction_address();
  unsigned insn = *(unsigned*)pc;
  if (Instruction_aarch64::extract(insn, 28, 24) == 0b10000) {
    address addr = MacroAssembler::pd_call_destination(pc);
    return *addr;
  } else {
    return (int)(intptr_t)MacroAssembler::pd_call_destination(instruction_address());
  }
}

void NativeMovRegMem::set_offset(int x) {
  address pc = instruction_address();
  unsigned insn = *(unsigned*)pc;
  if (maybe_cpool_ref(pc)) {
    address addr = MacroAssembler::pd_call_destination(pc);
    *(long*)addr = x;
  } else {
    MacroAssembler::pd_patch_instruction(pc, (address)intptr_t(x));
  }
}

void NativeMovRegMem::verify() {
#ifdef ASSERT
  address dest = MacroAssembler::pd_call_destination(instruction_address());
#endif
}


void NativeMovRegMem::print() { Unimplemented(); }

//-------------------------------------------------------------------

void NativeLoadAddress::verify() { Unimplemented(); }


void NativeLoadAddress::print() { Unimplemented(); }

//--------------------------------------------------------------------------------

void NativeJump::verify() { ; }


void NativeJump::insert(address code_pos, address entry) { Unimplemented(); }

void NativeJump::check_verified_entry_alignment(address entry, address verified_entry) {
  // Patching to not_entrant can happen while activations of the method are
  // in use. The patching in that instance must happen only when certain
  // alignment restrictions are true. These guarantees check those
  // conditions.
  const int linesize = 64;

  // Must be wordSize aligned
  guarantee(((uintptr_t) verified_entry & (wordSize -1)) == 0,
            "illegal address for code patching 2");
  // First 5 bytes must be within the same cache line - 4827828
  guarantee((uintptr_t) verified_entry / linesize ==
            ((uintptr_t) verified_entry + 4) / linesize,
            "illegal address for code patching 3");
}


address NativeJump::jump_destination() const          {
  address dest = MacroAssembler::pd_call_destination(instruction_address());

  // We use jump to self as the unresolved address which the inline
  // cache code (and relocs) know about

  // return -1 if jump to self
  dest = (dest == (address) this) ? (address) -1 : dest;
  return dest;
}

void NativeJump::set_jump_destination(address dest) {
  // We use jump to self as the unresolved address which the inline
  // cache code (and relocs) know about
  if (dest == (address) -1)
    dest = instruction_address();

  MacroAssembler::pd_patch_instruction(instruction_address(), dest);
};

bool NativeInstruction::is_safepoint_poll() {
  /* We expect a safepoint poll to be either

     adrp(reg, polling_page);
     ldr(reg, offset);

     or

     mov(reg, polling_page);
     ldr(reg, Address(reg, 0));
  */

  if (is_adrp_at(addr_at(-4))) {
    address addr = addr_at(-4);
    return os::is_poll_address(MacroAssembler::pd_call_destination(addr));
  } else {
    return os::is_poll_address(MacroAssembler::pd_call_destination(addr_at(-16)));
  }
}

bool NativeInstruction::is_adrp_at(address instr) {
  unsigned insn = *(unsigned*)instr;
  return (Instruction_aarch64::extract(insn, 31, 24) & 0b10011111) == 0b10010000;
}

bool NativeInstruction::is_ldr_literal_at(address instr) {
  unsigned insn = *(unsigned*)instr;
  return (Instruction_aarch64::extract(insn, 29, 24) & 0b011011) == 0b00011000;
}

// MT safe inserting of a jump over an unknown instruction sequence (used by nmethod::makeZombie)

void NativeJump::patch_verified_entry(address entry, address verified_entry, address dest) {
  ptrdiff_t disp = dest - verified_entry;
  guarantee(disp < 1 << 27 && disp > - (1 << 27), "branch overflow");

  unsigned int insn = (0b000101 << 26) | ((disp >> 2) & 0x3ffffff);

  *(unsigned int*)verified_entry = insn;
  ICache::invalidate_range(verified_entry, instruction_size);
}


void NativePopReg::insert(address code_pos, Register reg) { Unimplemented(); }


void NativeIllegalInstruction::insert(address code_pos) { Unimplemented(); }

void NativeGeneralJump::verify() {  }


void NativeGeneralJump::insert_unconditional(address code_pos, address entry) {
  ptrdiff_t disp = entry - code_pos;
  guarantee(disp < 1 << 27 && disp > - (1 << 27), "branch overflow");

  unsigned int insn = (0b000101 << 26) | ((disp >> 2) & 0x3ffffff);

  *(unsigned int*)code_pos = insn;
  ICache::invalidate_range(code_pos, instruction_size);
}

// MT-safe patching of a long jump instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
//
// FIXME: I don't think that this can be done on AArch64.  The memory
// is not coherent, so it does no matter what order we patch things
// in.  The only way to do it AFAIK is to have:
//
//    ldr rscratch, 0f
//    b rscratch
// 0: absolute address
//
void NativeGeneralJump::replace_mt_safe(address instr_addr, address code_buffer) {
  uint32_t instr = *(uint32_t*)code_buffer;
  *(uint32_t*)instr_addr = instr;
}

bool NativeInstruction::is_dtrace_trap() { return false; }
