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

void NativeInstruction::wrote(int offset) { Unimplemented(); }


void NativeCall::verify() { Unimplemented(); }

address NativeCall::destination() const { Unimplemented(); return 0; }

void NativeCall::print() { Unimplemented(); }

// Inserts a native call instruction at a given pc
void NativeCall::insert(address code_pos, address entry) { Unimplemented(); }

// MT-safe patching of a call instruction.
// First patches first word of instruction to two jmp's that jmps to them
// selfs (spinlock). Then patches the last byte, and then atomicly replaces
// the jmp's with the first 4 byte of the new instruction.
void NativeCall::replace_mt_safe(address instr_addr, address code_buffer) { Unimplemented(); }


<<<<<<< HEAD
// Similar to replace_mt_safe, but just changes the destination.  The
// important thing is that free-running threads are able to execute this
// call instruction at all times.  If the displacement field is aligned
// we can simply rely on atomicity of 32-bit writes to make sure other threads
// will see no intermediate states.  Otherwise, the first two bytes of the
// call are guaranteed to be aligned, and can be atomically patched to a
// self-loop to guard the instruction while we change the other bytes.

// We cannot rely on locks here, since the free-running threads must run at
// full speed.
//
// Used in the runtime linkage of calls; see class CompiledIC.
// (Cf. 4506997 and 4479829, where threads witnessed garbage displacements.)
void NativeCall::set_destination_mt_safe(address dest) { Unimplemented(); }


void NativeMovConstReg::verify() { Unimplemented(); }
=======
void NativeMovConstReg::verify() {
  // make sure code pattern is actually mov reg64, imm64 instructions
}

intptr_t NativeMovConstReg::data() const {
  return (intptr_t)MacroAssembler::pd_call_destination(instruction_address());
}

void NativeMovConstReg::set_data(intptr_t x) {
  MacroAssembler::pd_patch_instruction(instruction_address(), (address)x);
};

>>>>>>> c3651e4... NativeInst support


void NativeMovConstReg::print() { Unimplemented(); }

//-------------------------------------------------------------------

int NativeMovRegMem::instruction_start() const { Unimplemented(); return 0; }

address NativeMovRegMem::instruction_address() const      { return addr_at(instruction_offset); }

address NativeMovRegMem::next_instruction_address() const { Unimplemented(); return 0; }

int NativeMovRegMem::offset() const  {
  return (int)(intptr_t)MacroAssembler::pd_call_destination(instruction_address());
}

void NativeMovRegMem::set_offset(int x) {
  // FIXME: This assumes that the offset is moved into rscratch1 with
  // a sequence of four MOV instructions.
  MacroAssembler::pd_patch_instruction(instruction_address(), (address)intptr_t(x));
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
  MacroAssembler::pd_patch_instruction(instruction_address(), dest);
};

bool NativeInstruction::is_safepoint_poll() {
  address addr = addr_at(-4);
  return os::is_poll_address(MacroAssembler::pd_call_destination(addr));
}

bool NativeInstruction::is_adrp_at(address instr) {
  unsigned insn = *(unsigned*)instr;
  return (Instruction_aarch64::extract(insn, 31, 24) & 0b10011111) == 0b10010000;
}

// MT safe inserting of a jump over an unknown instruction sequence (used by nmethod::makeZombie)

void NativeJump::patch_verified_entry(address entry, address verified_entry, address dest) { Unimplemented(); }


void NativePopReg::insert(address code_pos, Register reg) { Unimplemented(); }


void NativeIllegalInstruction::insert(address code_pos) { Unimplemented(); }

void NativeGeneralJump::verify() {  }


void NativeGeneralJump::insert_unconditional(address code_pos, address entry) {
  ptrdiff_t disp = entry - code_pos;
#ifdef AMD64
  guarantee(((int)disp << 4) >> 4 == disp, "must be 26-bit offset");
#endif // AMD64

  unsigned int insn = (0b000101 << 26) | ((disp >> 2) & 0x3fff);

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
