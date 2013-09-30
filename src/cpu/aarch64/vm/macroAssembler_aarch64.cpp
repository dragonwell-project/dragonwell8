/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates.
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

#include <sys/types.h>

#include "precompiled.hpp"
#include "asm/assembler.hpp"
#include "asm/assembler.inline.hpp"
#include "interpreter/interpreter.hpp"

#include "compiler/disassembler.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/sharedRuntime.hpp"

// #include "gc_interface/collectedHeap.inline.hpp"
// #include "interpreter/interpreter.hpp"
// #include "memory/cardTableModRefBS.hpp"
// #include "prims/methodHandles.hpp"
// #include "runtime/biasedLocking.hpp"
// #include "runtime/interfaceSupport.hpp"
// #include "runtime/objectMonitor.hpp"
// #include "runtime/os.hpp"
// #include "runtime/sharedRuntime.hpp"
// #include "runtime/stubRoutines.hpp"
// #if INCLUDE_ALL_GCS
// #include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
// #include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
// #include "gc_implementation/g1/heapRegion.hpp"
// #endif

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#define STOP(error) stop(error)
#else
#define BLOCK_COMMENT(str) block_comment(str)
#define STOP(error) block_comment(error); stop(error)
#endif

#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

void MacroAssembler::pd_patch_instruction(address branch, address target) {
  long offset = (target - branch) >> 2;
  unsigned insn = *(unsigned*)branch;
  if ((Instruction_aarch64::extract(insn, 29, 24) & 0b111011) == 0b011000) {
    // Load register (literal)
    Instruction_aarch64::spatch(branch, 23, 5, offset);
  } else if (Instruction_aarch64::extract(insn, 30, 26) == 0b00101) {
    // Unconditional branch (immediate)
    Instruction_aarch64::spatch(branch, 25, 0, offset);
  } else if (Instruction_aarch64::extract(insn, 31, 25) == 0b0101010) {
    // Conditional branch (immediate)
    Instruction_aarch64::spatch(branch, 23, 5, offset);
  } else if (Instruction_aarch64::extract(insn, 30, 25) == 0b011010) {
    // Compare & branch (immediate)
    Instruction_aarch64::spatch(branch, 23, 5, offset);
  } else if (Instruction_aarch64::extract(insn, 30, 25) == 0b011011) {
    // Test & branch (immediate)
    Instruction_aarch64::spatch(branch, 18, 5, offset);
  } else if (Instruction_aarch64::extract(insn, 28, 24) == 0b10000) {
    // PC-rel. addressing
    offset = target-branch;
    int shift = Instruction_aarch64::extract(insn, 31, 31);
    if (shift) {
      u_int64_t dest = (u_int64_t)target;
      uint64_t pc_page = (uint64_t)branch >> 12;
      uint64_t adr_page = (uint64_t)target >> 12;
      unsigned offset_lo = dest & 0xfff;
      offset = adr_page - pc_page;

      unsigned insn2 = ((unsigned*)branch)[1];
      if (Instruction_aarch64::extract(insn2, 29, 24) == 0b111001) {
	// Load/store register (unsigned immediate)
	unsigned size = Instruction_aarch64::extract(insn2, 31, 30);
	Instruction_aarch64::patch(branch + sizeof (unsigned),
				    21, 10, offset_lo >> size);
	guarantee(((dest >> size) << size) == dest, "misaligned target");
	guarantee(Instruction_aarch64::extract(insn, 4, 0)
		  == Instruction_aarch64::extract(insn2, 9, 5),
		  "Registers should be the same");
      } else if (Instruction_aarch64::extract(insn2, 31, 29) == 0b100
		 && Instruction_aarch64::extract(insn2, 23, 22) == 0b00) {
	// add (immediate)
	Instruction_aarch64::patch(branch + sizeof (unsigned),
				   21, 10, offset_lo);
	guarantee(Instruction_aarch64::extract(insn, 4, 0)
		  == Instruction_aarch64::extract(insn2, 4, 0),
		  "Registers should be the same");
      } else {
	ShouldNotReachHere();
      }
    }
    int offset_lo = offset & 3;
    offset >>= 2;
    Instruction_aarch64::spatch(branch, 23, 5, offset);
    Instruction_aarch64::patch(branch, 30, 29, offset_lo);
  } else if (Instruction_aarch64::extract(insn, 31, 23) == 0b110100101) {
    // Move wide constant
    u_int64_t dest = (u_int64_t)target;
    Instruction_aarch64::patch(branch, 20, 5, dest & 0xffff);
    Instruction_aarch64::patch(branch += 4, 20, 5, (dest >>= 16) & 0xffff);
    Instruction_aarch64::patch(branch += 4, 20, 5, (dest >>= 16) & 0xffff);
    Instruction_aarch64::patch(branch += 4, 20, 5, (dest >>= 16));
  } else if (Instruction_aarch64::extract(insn, 31, 22) == 0b1011100101 &&
             Instruction_aarch64::extract(insn, 4, 0) == 0b11111) {
    // nothing to do
    assert(target == 0, "did not expect to relocate target for polling page load");
  } else {
    ShouldNotReachHere();
  }
}

address MacroAssembler::target_addr_for_insn(address insn_addr, unsigned insn) {
  long offset = 0;
  if ((Instruction_aarch64::extract(insn, 29, 24) & 0b011011) == 0b00011000) {
    // Load register (literal)
    offset = Instruction_aarch64::sextract(insn, 23, 5);
    return address(((uint64_t)insn_addr + (offset << 2)));
  } else if (Instruction_aarch64::extract(insn, 30, 26) == 0b00101) {
    // Unconditional branch (immediate)
    offset = Instruction_aarch64::sextract(insn, 25, 0);
  } else if (Instruction_aarch64::extract(insn, 31, 25) == 0b0101010) {
    // Conditional branch (immediate)
    offset = Instruction_aarch64::sextract(insn, 23, 5);
  } else if (Instruction_aarch64::extract(insn, 30, 25) == 0b011010) {
    // Compare & branch (immediate)
    offset = Instruction_aarch64::sextract(insn, 23, 5);
   } else if (Instruction_aarch64::extract(insn, 30, 25) == 0b011011) {
    // Test & branch (immediate)
    offset = Instruction_aarch64::sextract(insn, 18, 5);
  } else if (Instruction_aarch64::extract(insn, 28, 24) == 0b10000) {
    // PC-rel. addressing
    offset = Instruction_aarch64::extract(insn, 30, 29);
    offset |= Instruction_aarch64::sextract(insn, 23, 5) << 2;
    int shift = Instruction_aarch64::extract(insn, 31, 31) ? 12 : 0;
    if (shift) {
      offset <<= shift;
      uint64_t target_page = ((uint64_t)insn_addr) + offset;
      target_page &= ((uint64_t)-1) << shift;
      unsigned insn2 = ((unsigned*)insn_addr)[1];
      if (Instruction_aarch64::extract(insn2, 29, 24) == 0b111001) {
	// Load/store register (unsigned immediate)
	unsigned int byte_offset = Instruction_aarch64::extract(insn2, 21, 10);
	unsigned int size = Instruction_aarch64::extract(insn2, 31, 30);
	return address(target_page + (byte_offset << size));
      } else {
	ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else if (Instruction_aarch64::extract(insn, 31, 23) == 0b110100101) {
    // Move wide constant
    // FIXME: We assume these instructions are movz, movk, movk, movk.
    // We don't assert this; we should.
    u_int32_t *insns = (u_int32_t *)insn_addr;
    return address(u_int64_t(Instruction_aarch64::extract(insns[0], 20, 5))
		   + (u_int64_t(Instruction_aarch64::extract(insns[1], 20, 5)) << 16)
		   + (u_int64_t(Instruction_aarch64::extract(insns[2], 20, 5)) << 32)
		   + (u_int64_t(Instruction_aarch64::extract(insns[3], 20, 5)) << 48));
  } else if (Instruction_aarch64::extract(insn, 31, 22) == 0b1011100101 &&
             Instruction_aarch64::extract(insn, 4, 0) == 0b11111) {
    return 0;
  } else {
    ShouldNotReachHere();
  }
  return address(((uint64_t)insn_addr + (offset << 2)));
}

void MacroAssembler::serialize_memory(Register thread, Register tmp) {
  dsb(Assembler::SY);
}


void MacroAssembler::reset_last_Java_frame(bool clear_fp,
                                           bool clear_pc) {
  // we must set sp to zero to clear frame
  str(zr, Address(rthread, JavaThread::last_Java_sp_offset()));
  // must clear fp, so that compiled frames are not confused; it is
  // possible that we need it only for debugging
  if (clear_fp) {
    str(zr, Address(rthread, JavaThread::last_Java_fp_offset()));
  }

  if (clear_pc) {
    str(zr, Address(rthread, JavaThread::last_Java_pc_offset()));
  }
}

// Calls to C land
//
// When entering C land, the rfp, & resp of the last Java frame have to be recorded
// in the (thread-local) JavaThread object. When leaving C land, the last Java fp
// has to be reset to 0. This is required to allow proper stack traversal.
void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         Register last_java_pc,
					 Register scratch) {

  if (last_java_pc->is_valid()) {
      str(last_java_pc, Address(rthread,
				JavaThread::frame_anchor_offset()
				+ JavaFrameAnchor::last_Java_pc_offset()));
    }

  // determine last_java_sp register
  if (last_java_sp == sp) {
    mov(scratch, sp);
    last_java_sp = scratch;
  } else if (!last_java_sp->is_valid()) {
    last_java_sp = esp;
  }

  str(last_java_sp, Address(rthread, JavaThread::last_Java_sp_offset()));

  // last_java_fp is optional
  if (last_java_fp->is_valid()) {
    str(last_java_fp, Address(rthread, JavaThread::last_Java_fp_offset()));
  }
}

void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         address  last_java_pc,
					 Register scratch) {
  if (last_java_pc != NULL) {
    adr(scratch, last_java_pc);
  } else {
    // FIXME: This is almost never correct.  We should delete all
    // cases of set_last_Java_frame with last_java_pc=NULL and use the
    // correct return address instead.
    adr(scratch, pc());
  }

  str(scratch, Address(rthread,
		       JavaThread::frame_anchor_offset()
		       + JavaFrameAnchor::last_Java_pc_offset()));

  set_last_Java_frame(last_java_sp, last_java_fp, noreg, scratch);
}

void MacroAssembler::set_last_Java_frame(Register last_java_sp,
                                         Register last_java_fp,
                                         Label &L,
					 Register scratch) {
  if (L.is_bound()) {
    set_last_Java_frame(last_java_sp, last_java_fp, target(L), scratch);
  } else {
    InstructionMark im(this);
    L.add_patch_at(code(), locator());
    set_last_Java_frame(last_java_sp, last_java_fp, (address)NULL, scratch);
  }
}

int MacroAssembler::biased_locking_enter(Register lock_reg,
                                         Register obj_reg,
                                         Register swap_reg,
                                         Register tmp_reg,
                                         bool swap_reg_contains_mark,
                                         Label& done,
                                         Label* slow_case,
                                         BiasedLockingCounters* counters) {
  Unimplemented();
}

void MacroAssembler::biased_locking_exit(Register obj_reg, Register temp_reg, Label& done) {
  Unimplemented();
}

// added to make this compile

REGISTER_DEFINITION(Register, noreg);

static void pass_arg0(MacroAssembler* masm, Register arg) {
  if (c_rarg0 != arg ) {
    masm->mov(c_rarg0, arg);
  }
}

static void pass_arg1(MacroAssembler* masm, Register arg) {
  if (c_rarg1 != arg ) {
    masm->mov(c_rarg1, arg);
  }
}

static void pass_arg2(MacroAssembler* masm, Register arg) {
  if (c_rarg2 != arg ) {
    masm->mov(c_rarg2, arg);
  }
}

static void pass_arg3(MacroAssembler* masm, Register arg) {
  if (c_rarg3 != arg ) {
    masm->mov(c_rarg3, arg);
  }
}

void MacroAssembler::call_VM_base(Register oop_result,
				  Register java_thread,
				  Register last_java_sp,
				  address  entry_point,
				  int      number_of_arguments,
				  bool     check_exceptions) {
   // determine java_thread register
  if (!java_thread->is_valid()) {
    java_thread = rthread;
  }

  // determine last_java_sp register
  if (!last_java_sp->is_valid()) {
    last_java_sp = esp;
  }

  // debugging support
  assert(number_of_arguments >= 0   , "cannot have negative number of arguments");
  assert(java_thread == rthread, "unexpected register");
#ifdef ASSERT
  // TraceBytecodes does not use r12 but saves it over the call, so don't verify
  // if ((UseCompressedOops || UseCompressedKlassPointers) && !TraceBytecodes) verify_heapbase("call_VM_base: heap base corrupted?");
#endif // ASSERT

  assert(java_thread != oop_result  , "cannot use the same register for java_thread & oop_result");
  assert(java_thread != last_java_sp, "cannot use the same register for java_thread & last_java_sp");

  // push java thread (becomes first argument of C function)

  mov(c_rarg0, java_thread);

  // set last Java frame before call
  assert(last_java_sp != rfp, "can't use rfp");

  Label l;
  set_last_Java_frame(last_java_sp, rfp, l, rscratch1);

  // do the call, remove parameters
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments, &l);

  // reset last Java frame
  // Only interpreter should have to clear fp
  reset_last_Java_frame(true, true);

   // C++ interp handles this in the interpreter
  check_and_handle_popframe(java_thread);
  check_and_handle_earlyret(java_thread);

  if (check_exceptions) {
    // check for pending exceptions (java_thread is set upon return)
    ldr(rscratch1, Address(java_thread, in_bytes(Thread::pending_exception_offset())));
    Label ok;
    cbz(rscratch1, ok);
    lea(rscratch1, RuntimeAddress(StubRoutines::forward_exception_entry()));
    br(rscratch1);
    bind(ok);
  }

  // get oop result if there is one and reset the value in the thread
  if (oop_result->is_valid()) {
    get_vm_result(oop_result, java_thread);
  }
}

void MacroAssembler::call_VM_helper(Register oop_result, address entry_point, int number_of_arguments, bool check_exceptions) {
  call_VM_base(oop_result, noreg, noreg, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call(Address entry) {
  if (true // reachable(entry)
      ) {
    bl(entry);
  } else {
    lea(rscratch1, entry);
    blr(rscratch1);
  }
}

void MacroAssembler::ic_call(address entry) {
  RelocationHolder rh = virtual_call_Relocation::spec(pc());
  address const_ptr = long_constant((jlong)Universe::non_oop_word());
  unsigned long offset;
  ldr_constant(rscratch2, const_ptr);
  call(Address(entry, rh));
}

// Implementation of call_VM versions

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             bool check_exceptions) {
  call_VM_helper(oop_result, entry_point, 0, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  pass_arg1(this, arg_1);
  call_VM_helper(oop_result, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {
  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);
  pass_arg1(this, arg_1);
  call_VM_helper(oop_result, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  assert(arg_1 != c_rarg3, "smashed arg");
  assert(arg_2 != c_rarg3, "smashed arg");
  pass_arg3(this, arg_3);

  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);

  pass_arg1(this, arg_1);
  call_VM_helper(oop_result, entry_point, 3, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             int number_of_arguments,
                             bool check_exceptions) {
  call_VM_base(oop_result, rthread, last_java_sp, entry_point, number_of_arguments, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) {
  pass_arg1(this, arg_1);
  call_VM(oop_result, last_java_sp, entry_point, 1, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) {

  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);
  pass_arg1(this, arg_1);
  call_VM(oop_result, last_java_sp, entry_point, 2, check_exceptions);
}

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) {
  assert(arg_1 != c_rarg3, "smashed arg");
  assert(arg_2 != c_rarg3, "smashed arg");
  pass_arg3(this, arg_3);
  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);
  pass_arg1(this, arg_1);
  call_VM(oop_result, last_java_sp, entry_point, 3, check_exceptions);
}


void MacroAssembler::get_vm_result(Register oop_result, Register java_thread) {
  ldr(oop_result, Address(java_thread, JavaThread::vm_result_offset()));
  str(zr, Address(java_thread, JavaThread::vm_result_offset()));
  verify_oop(oop_result, "broken oop in call_VM_base");
}

void MacroAssembler::get_vm_result_2(Register metadata_result, Register java_thread) {
  ldr(metadata_result, Address(java_thread, JavaThread::vm_result_2_offset()));
  str(zr, Address(java_thread, JavaThread::vm_result_2_offset()));
}

void MacroAssembler::align(int modulus) {
  while (offset() % modulus != 0) nop();
}

// these are meant to be no-ops overridden by InterpreterMacroAssembler

void MacroAssembler::check_and_handle_earlyret(Register java_thread) { Unimplemented(); }

void MacroAssembler::check_and_handle_popframe(Register java_thread) { Unimplemented(); }

RegisterOrConstant MacroAssembler::delayed_value_impl(intptr_t* delayed_value_addr,
                                                      Register tmp,
                                                      int offset) { Unimplemented(); return RegisterOrConstant(r0); }

void MacroAssembler:: notify(int type) {
  if (type == bytecode_start) {
    // set_last_Java_frame(esp, rfp, (address)NULL);
    Assembler:: notify(type);
    // reset_last_Java_frame(true, false);
  }
  else
    Assembler:: notify(type);
}

// Look up the method for a megamorphic invokeinterface call.
// The target method is determined by <intf_klass, itable_index>.
// The receiver klass is in recv_klass.
// On success, the result will be in method_result, and execution falls through.
// On failure, execution transfers to the given label.
void MacroAssembler::lookup_interface_method(Register recv_klass,
                                             Register intf_klass,
                                             RegisterOrConstant itable_index,
                                             Register method_result,
                                             Register scan_temp,
                                             Label& L_no_such_interface) {
  assert_different_registers(recv_klass, intf_klass, method_result, scan_temp);
  assert(itable_index.is_constant() || itable_index.as_register() == method_result,
         "caller must use same register for non-constant itable index as for method");

  // Compute start of first itableOffsetEntry (which is at the end of the vtable)
  int vtable_base = InstanceKlass::vtable_start_offset() * wordSize;
  int itentry_off = itableMethodEntry::method_offset_in_bytes();
  int scan_step   = itableOffsetEntry::size() * wordSize;
  int vte_size    = vtableEntry::size() * wordSize;
  assert(vte_size == wordSize, "else adjust times_vte_scale");

  ldrw(scan_temp, Address(recv_klass, InstanceKlass::vtable_length_offset() * wordSize));

  // %%% Could store the aligned, prescaled offset in the klassoop.
  // lea(scan_temp, Address(recv_klass, scan_temp, times_vte_scale, vtable_base));
  lea(scan_temp, Address(recv_klass, scan_temp, Address::lsl(3)));
  add(scan_temp, scan_temp, vtable_base);
  if (HeapWordsPerLong > 1) {
    // Round up to align_object_offset boundary
    // see code for instanceKlass::start_of_itable!
    round_to(scan_temp, BytesPerLong);
  }

  // Adjust recv_klass by scaled itable_index, so we can free itable_index.
  assert(itableMethodEntry::size() * wordSize == wordSize, "adjust the scaling in the code below");
  // lea(recv_klass, Address(recv_klass, itable_index, Address::times_ptr, itentry_off));
  lea(recv_klass, Address(recv_klass, itable_index, Address::lsl(3)));
  if (itentry_off)
    add(recv_klass, recv_klass, itentry_off);

  // for (scan = klass->itable(); scan->interface() != NULL; scan += scan_step) {
  //   if (scan->interface() == intf) {
  //     result = (klass + scan->offset() + itable_index);
  //   }
  // }
  Label search, found_method;

  for (int peel = 1; peel >= 0; peel--) {
    ldr(method_result, Address(scan_temp, itableOffsetEntry::interface_offset_in_bytes()));
    cmp(intf_klass, method_result);

    if (peel) {
      br(Assembler::EQ, found_method);
    } else {
      br(Assembler::NE, search);
      // (invert the test to fall through to found_method...)
    }

    if (!peel)  break;

    bind(search);

    // Check that the previous entry is non-null.  A null entry means that
    // the receiver class doesn't implement the interface, and wasn't the
    // same as when the caller was compiled.
    cbz(method_result, L_no_such_interface);
    add(scan_temp, scan_temp, scan_step);
  }

  bind(found_method);

  // Got a hit.
  ldr(scan_temp, Address(scan_temp, itableOffsetEntry::offset_offset_in_bytes()));
  ldr(method_result, Address(recv_klass, scan_temp));
}

// virtual method calling
void MacroAssembler::lookup_virtual_method(Register recv_klass,
                                           RegisterOrConstant vtable_index,
                                           Register method_result) {
  const int base = InstanceKlass::vtable_start_offset() * wordSize;
  assert(vtableEntry::size() * wordSize == 8,
         "adjust the scaling in the code below");
  int vtable_offset_in_bytes = base + vtableEntry::method_offset_in_bytes();

  if (vtable_index.is_register()) {
    lea(method_result, Address(recv_klass,
			       vtable_index.as_register(),
			       Address::lsl(LogBytesPerWord)));
    ldr(method_result, Address(method_result, vtable_offset_in_bytes));
  } else {
    vtable_offset_in_bytes += vtable_index.as_constant() * wordSize;
    ldr(method_result, Address(recv_klass, vtable_offset_in_bytes));
  }
}

void MacroAssembler::check_klass_subtype(Register sub_klass,
                           Register super_klass,
                           Register temp_reg,
                           Label& L_success) {
  Label L_failure;
  check_klass_subtype_fast_path(sub_klass, super_klass, temp_reg,        &L_success, &L_failure, NULL);
  check_klass_subtype_slow_path(sub_klass, super_klass, temp_reg, noreg, &L_success, NULL);
  bind(L_failure);
}


void MacroAssembler::check_klass_subtype_fast_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   Label* L_slow_path,
                                        RegisterOrConstant super_check_offset) {
  assert_different_registers(sub_klass, super_klass, temp_reg);
  bool must_load_sco = (super_check_offset.constant_or_zero() == -1);
  if (super_check_offset.is_register()) {
    assert_different_registers(sub_klass, super_klass,
                               super_check_offset.as_register());
  } else if (must_load_sco) {
    assert(temp_reg != noreg, "supply either a temp or a register offset");
  }

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  if (L_slow_path == NULL) { L_slow_path = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  int sco_offset = in_bytes(Klass::super_check_offset_offset());
  Address super_check_offset_addr(super_klass, sco_offset);

  // Hacked jmp, which may only be used just before L_fallthrough.
#define final_jmp(label)                                                \
  if (&(label) == &L_fallthrough) { /*do nothing*/ }                    \
  else                            b(label)                /*omit semi*/

  // If the pointers are equal, we are done (e.g., String[] elements).
  // This self-check enables sharing of secondary supertype arrays among
  // non-primary types such as array-of-interface.  Otherwise, each such
  // type would need its own customized SSA.
  // We move this check to the front of the fast path because many
  // type checks are in fact trivially successful in this manner,
  // so we get a nicely predicted branch right at the start of the check.
  cmp(sub_klass, super_klass);
  br(Assembler::EQ, *L_success);

  // Check the supertype display:
  if (must_load_sco) {
    // Positive movl does right thing on LP64.
    ldrw(temp_reg, super_check_offset_addr);
    super_check_offset = RegisterOrConstant(temp_reg);
  }
  Address super_check_addr(sub_klass, super_check_offset);
  ldr(rscratch1, super_check_addr);
  cmp(super_klass, rscratch1); // load displayed supertype

  // This check has worked decisively for primary supers.
  // Secondary supers are sought in the super_cache ('super_cache_addr').
  // (Secondary supers are interfaces and very deeply nested subtypes.)
  // This works in the same check above because of a tricky aliasing
  // between the super_cache and the primary super display elements.
  // (The 'super_check_addr' can address either, as the case requires.)
  // Note that the cache is updated below if it does not help us find
  // what we need immediately.
  // So if it was a primary super, we can just fail immediately.
  // Otherwise, it's the slow path for us (no success at this point).

  if (super_check_offset.is_register()) {
    br(Assembler::EQ, *L_success);
    cmp(super_check_offset.as_register(), sc_offset);
    if (L_failure == &L_fallthrough) {
      br(Assembler::EQ, *L_slow_path);
    } else {
      br(Assembler::NE, *L_failure);
      final_jmp(*L_slow_path);
    }
  } else if (super_check_offset.as_constant() == sc_offset) {
    // Need a slow path; fast failure is impossible.
    if (L_slow_path == &L_fallthrough) {
      br(Assembler::EQ, *L_success);
    } else {
      br(Assembler::NE, *L_slow_path);
      final_jmp(*L_success);
    }
  } else {
    // No slow path; it's a fast decision.
    if (L_failure == &L_fallthrough) {
      br(Assembler::EQ, *L_success);
    } else {
      br(Assembler::NE, *L_failure);
      final_jmp(*L_success);
    }
  }

  bind(L_fallthrough);

#undef final_jmp
}

// These two are taken from x86, but they look generally useful

// scans count pointer sized words at [addr] for occurence of value,
// generic
void MacroAssembler::repne_scan(Register addr, Register value, Register count,
				Register scratch) {
  Label Lloop, Lexit;
  cbz(count, Lexit);
  bind(Lloop);
  ldr(scratch, post(addr, wordSize));
  cmp(value, scratch);
  br(EQ, Lexit);
  sub(count, count, 1);
  cbnz(count, Lloop);
  bind(Lexit);
}

// scans count 4 byte words at [addr] for occurence of value,
// generic
void MacroAssembler::repne_scanw(Register addr, Register value, Register count,
				Register scratch) {
  Label Lloop, Lexit;
  cbz(count, Lexit);
  bind(Lloop);
  ldrw(scratch, post(addr, wordSize));
  cmpw(value, scratch);
  br(EQ, Lexit);
  sub(count, count, 1);
  cbnz(count, Lloop);
  bind(Lexit);
}

void MacroAssembler::check_klass_subtype_slow_path(Register sub_klass,
                                                   Register super_klass,
                                                   Register temp_reg,
                                                   Register temp2_reg,
                                                   Label* L_success,
                                                   Label* L_failure,
                                                   bool set_cond_codes) {
  assert_different_registers(sub_klass, super_klass, temp_reg);
  if (temp2_reg != noreg)
    assert_different_registers(sub_klass, super_klass, temp_reg, temp2_reg, rscratch1);
#define IS_A_TEMP(reg) ((reg) == temp_reg || (reg) == temp2_reg)

  Label L_fallthrough;
  int label_nulls = 0;
  if (L_success == NULL)   { L_success   = &L_fallthrough; label_nulls++; }
  if (L_failure == NULL)   { L_failure   = &L_fallthrough; label_nulls++; }
  assert(label_nulls <= 1, "at most one NULL in the batch");

  // a couple of useful fields in sub_klass:
  int ss_offset = in_bytes(Klass::secondary_supers_offset());
  int sc_offset = in_bytes(Klass::secondary_super_cache_offset());
  Address secondary_supers_addr(sub_klass, ss_offset);
  Address super_cache_addr(     sub_klass, sc_offset);

  BLOCK_COMMENT("check_klass_subtype_slow_path");

  // Do a linear scan of the secondary super-klass chain.
  // This code is rarely used, so simplicity is a virtue here.
  // The repne_scan instruction uses fixed registers, which we must spill.
  // Don't worry too much about pre-existing connections with the input regs.

  assert(sub_klass != r0, "killed reg"); // killed by mov(r0, super)
  assert(sub_klass != r2, "killed reg"); // killed by lea(r2, &pst_counter)

  // Get super_klass value into r0 (even if it was in r5 or r2).
  bool pushed_r0 = false, pushed_r2 = !IS_A_TEMP(r2), pushed_r5 = !IS_A_TEMP(r5);

  if (super_klass != r0 || UseCompressedOops) {
    if (!IS_A_TEMP(r0))
      pushed_r0 = true;
  }

  push(r0->bit(pushed_r0) | r2->bit(pushed_r2) | r5->bit(pushed_r5), sp);

#ifndef PRODUCT
  mov(rscratch2, (address)&SharedRuntime::_partial_subtype_ctr);
  Address pst_counter_addr(rscratch2);
  ldr(rscratch1, pst_counter_addr);
  add(rscratch1, rscratch1, 1);
  str(rscratch1, pst_counter_addr);
#endif //PRODUCT

  // We will consult the secondary-super array.
  ldr(r5, secondary_supers_addr);
  // Load the array length.  (Positive movl does right thing on LP64.)
  ldr(r2, Address(r5, Array<Klass*>::length_offset_in_bytes()));
  // Skip to start of data.
  add(r5, r5, Array<Klass*>::base_offset_in_bytes());

  cmp(sp, zr); // Clear Z flag; SP is never zero
  // Scan R2 words at [R5] for an occurrence of R0.
  // Set NZ/Z based on last compare.
  repne_scan(r5, r0, r2, rscratch1);

  // Unspill the temp. registers:
  pop(r0->bit(pushed_r0) | r2->bit(pushed_r2) | r5->bit(pushed_r5), sp);

  br(Assembler::NE, *L_failure);

  // Success.  Cache the super we found and proceed in triumph.
  str(super_klass, super_cache_addr);

  if (L_success != &L_fallthrough) {
    b(*L_success);
  }

#undef IS_A_TEMP

  bind(L_fallthrough);
}


void MacroAssembler::verify_oop(Register reg, const char* s) {
  if (!VerifyOops) return;

  // Pass register number to verify_oop_subroutine
  const char* b = NULL;
  {
    ResourceMark rm;
    stringStream ss;
    ss.print("verify_oop: %s: %s", reg->name(), s);
    b = code_string(ss.as_string());
  }
  BLOCK_COMMENT("verify_oop {");

  stp(r0, rscratch1, Address(pre(sp, -2 * wordSize)));
  stp(rscratch2, lr, Address(pre(sp, -2 * wordSize)));

  mov(r0, reg);
  mov(rscratch1, (address)b);

  // call indirectly to solve generation ordering problem
  lea(rscratch2, ExternalAddress(StubRoutines::verify_oop_subroutine_entry_address()));
  ldr(rscratch2, Address(rscratch2));
  blr(rscratch2);

  ldp(rscratch2, lr, Address(post(sp, 2 * wordSize)));
  ldp(r0, rscratch1, Address(post(sp, 2 * wordSize)));

  BLOCK_COMMENT("} verify_oop");
}

void MacroAssembler::verify_oop_addr(Address addr, const char* s) {
  if (!VerifyOops) return;

  const char* b = NULL;
  {
    ResourceMark rm;
    stringStream ss;
    ss.print("verify_oop_addr: %s", s);
    b = code_string(ss.as_string());
  }
  BLOCK_COMMENT("verify_oop_addr {");

  stp(r0, rscratch1, Address(pre(sp, -2 * wordSize)));
  stp(rscratch2, lr, Address(pre(sp, -2 * wordSize)));

  // addr may contain sp so we will have to adjust it based on the
  // pushes that we just did.
  if (addr.uses(sp)) {
    lea(r0, addr);
    ldr(r0, Address(r0, 4 * wordSize));
  } else {
    ldr(r0, addr);
  }
  mov(rscratch1, (address)b);

  // call indirectly to solve generation ordering problem
  lea(rscratch2, ExternalAddress(StubRoutines::verify_oop_subroutine_entry_address()));
  ldr(rscratch2, Address(rscratch2));
  blr(rscratch2);

  ldp(rscratch2, lr, Address(post(sp, 2 * wordSize)));
  ldp(r0, rscratch1, Address(post(sp, 2 * wordSize)));

  BLOCK_COMMENT("} verify_oop_addr");
}

Address MacroAssembler::argument_address(RegisterOrConstant arg_slot,
                                         int extra_slot_offset) {
  // cf. TemplateTable::prepare_invoke(), if (load_receiver).
  int stackElementSize = Interpreter::stackElementSize;
  int offset = Interpreter::expr_offset_in_bytes(extra_slot_offset+0);
#ifdef ASSERT
  int offset1 = Interpreter::expr_offset_in_bytes(extra_slot_offset+1);
  assert(offset1 - offset == stackElementSize, "correct arithmetic");
#endif
  if (arg_slot.is_constant()) {
    return Address(esp, arg_slot.as_constant() * stackElementSize
		   + offset);
  } else {
    add(rscratch1, esp, arg_slot.as_register(),
	ext::uxtx, exact_log2(stackElementSize));
    return Address(rscratch1, offset);
  }
}

void MacroAssembler::call_VM_leaf_base(address entry_point,
                                       int number_of_arguments,
				       Label *retaddr) {
  call_VM_leaf_base1(entry_point, number_of_arguments, 0, ret_type_integral, retaddr);
}

void MacroAssembler::call_VM_leaf_base1(address entry_point,
					int number_of_gp_arguments,
					int number_of_fp_arguments,
					ret_type type,
					Label *retaddr) {
  Label E, L;

  stp(rscratch1, rmethod, Address(pre(sp, -2 * wordSize)));

  // We add 1 to number_of_arguments because the thread in arg0 is
  // not counted
  mov(rscratch1, entry_point);
  blrt(rscratch1, number_of_gp_arguments + 1, number_of_fp_arguments, type);
  if (retaddr)
    bind(*retaddr);

  ldp(rscratch1, rmethod, Address(post(sp, 2 * wordSize)));
}

void MacroAssembler::call_VM_leaf(address entry_point, int number_of_arguments) {
  call_VM_leaf_base(entry_point, number_of_arguments);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0) {
  pass_arg0(this, arg_0);
  call_VM_leaf_base(entry_point, 1);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1) {
  pass_arg0(this, arg_0);
  pass_arg1(this, arg_1);
  call_VM_leaf_base(entry_point, 2);
}

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0,
				  Register arg_1, Register arg_2) {
  pass_arg0(this, arg_0);
  pass_arg1(this, arg_1);
  pass_arg2(this, arg_2);
  call_VM_leaf_base(entry_point, 3);
}

void MacroAssembler::super_call_VM_leaf(address entry_point, Register arg_0) {
  pass_arg0(this, arg_0);
  MacroAssembler::call_VM_leaf_base(entry_point, 1);
}

void MacroAssembler::super_call_VM_leaf(address entry_point, Register arg_0, Register arg_1) {

  assert(arg_0 != c_rarg1, "smashed arg");
  pass_arg1(this, arg_1);
  pass_arg0(this, arg_0);
  MacroAssembler::call_VM_leaf_base(entry_point, 2);
}

void MacroAssembler::super_call_VM_leaf(address entry_point, Register arg_0, Register arg_1, Register arg_2) {
  assert(arg_0 != c_rarg2, "smashed arg");
  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);
  assert(arg_0 != c_rarg1, "smashed arg");
  pass_arg1(this, arg_1);
  pass_arg0(this, arg_0);
  MacroAssembler::call_VM_leaf_base(entry_point, 3);
}

void MacroAssembler::super_call_VM_leaf(address entry_point, Register arg_0, Register arg_1, Register arg_2, Register arg_3) {
  assert(arg_0 != c_rarg3, "smashed arg");
  assert(arg_1 != c_rarg3, "smashed arg");
  assert(arg_2 != c_rarg3, "smashed arg");
  pass_arg3(this, arg_3);
  assert(arg_0 != c_rarg2, "smashed arg");
  assert(arg_1 != c_rarg2, "smashed arg");
  pass_arg2(this, arg_2);
  assert(arg_0 != c_rarg1, "smashed arg");
  pass_arg1(this, arg_1);
  pass_arg0(this, arg_0);
  MacroAssembler::call_VM_leaf_base(entry_point, 4);
}

void MacroAssembler::null_check(Register reg, int offset) {
  if (needs_explicit_null_check(offset)) {
    // provoke OS NULL exception if reg = NULL by
    // accessing M[reg] w/o changing any registers
    // NOTE: this is plenty to provoke a segv
    ldr(zr, Address(reg));
  } else {
    // nothing to do, (later) access of M[reg + offset]
    // will provoke OS NULL exception if reg = NULL
  }
}

// MacroAssembler protected routines needed to implement
// public methods

void MacroAssembler::mov(Register r, Address dest) {
  InstructionMark im(this);
  code_section()->relocate(inst_mark(), dest.rspec());
  u_int64_t imm64 = (u_int64_t)dest.target();
  mov64(r, imm64);
}

void MacroAssembler::mov64(Register r, uintptr_t imm64) {
#ifndef PRODUCT
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "0x%"PRIX64, imm64);
    block_comment(buffer);
  }
#endif
  movz(r, imm64 & 0xffff);
  imm64 >>= 16;
  movk(r, imm64 & 0xffff, 16);
  imm64 >>= 16;
  movk(r, imm64 & 0xffff, 32);
  imm64 >>= 16;
  movk(r, imm64 & 0xffff, 48);
}

void MacroAssembler::mov_immediate64(Register dst, u_int64_t imm64)
{
#ifndef PRODUCT
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "0x%"PRIX64, imm64);
    block_comment(buffer);
  }
#endif
  if (operand_valid_for_logical_immediate(false, imm64)) {
    orr(dst, zr, imm64);
  } else {
    // we can use a combination of MOVZ or MOVN with
    // MOVK to build up the constant
    u_int64_t imm_h[4];
    int zero_count = 0;
    int neg_count = 0;
    int i;
    for (i = 0; i < 4; i++) {
      imm_h[i] = ((imm64 >> (i * 16)) & 0xffffL);
      if (imm_h[i] == 0) {
	zero_count++;
      } else if (imm_h[i] == 0xffffL) {
	neg_count++;
      }
    }
    if (zero_count == 4) {
      // one MOVZ will do
      movz(dst, 0);
    } else if (neg_count == 4) {
      // one MOVN will do
      movn(dst, 0);
    } else if (zero_count == 3) {
      for (i = 0; i < 4; i++) {
	if (imm_h[i] != 0L) {
	  movz(dst, (u_int32_t)imm_h[i], (i << 4));
	  break;
	}
      }
    } else if (neg_count == 3) {
      // one MOVN will do
      for (int i = 0; i < 4; i++) {
	if (imm_h[i] != 0xffffL) {
	  movn(dst, (u_int32_t)imm_h[i] ^ 0xffffL, (i << 4));
	  break;
	}
      }
    } else if (zero_count == 2) {
      // one MOVZ and one MOVK will do
      for (i = 0; i < 3; i++) {
	if (imm_h[i] != 0L) {
	  movz(dst, (u_int32_t)imm_h[i], (i << 4));
	  i++;
	  break;
	}
      }
      for (;i < 4; i++) {
	if (imm_h[i] != 0L) {
	  movk(dst, (u_int32_t)imm_h[i], (i << 4));
	}
      }
    } else if (neg_count == 2) {
      // one MOVN and one MOVK will do
      for (i = 0; i < 4; i++) {
	if (imm_h[i] != 0xffffL) {
	  movn(dst, (u_int32_t)imm_h[i] ^ 0xffffL, (i << 4));
	  i++;
	  break;
	}
      }
      for (;i < 4; i++) {
	if (imm_h[i] != 0xffffL) {
	  movk(dst, (u_int32_t)imm_h[i], (i << 4));
	}
      }
    } else if (zero_count == 1) {
      // one MOVZ and two MOVKs will do
      for (i = 0; i < 4; i++) {
	if (imm_h[i] != 0L) {
	  movz(dst, (u_int32_t)imm_h[i], (i << 4));
	  i++;
	  break;
	}
      }
      for (;i < 4; i++) {
	if (imm_h[i] != 0x0L) {
	  movk(dst, (u_int32_t)imm_h[i], (i << 4));
	}
      }
    } else if (neg_count == 1) {
      // one MOVN and two MOVKs will do
      for (i = 0; i < 4; i++) {
	if (imm_h[i] != 0xffffL) {
	  movn(dst, (u_int32_t)imm_h[i] ^ 0xffffL, (i << 4));
	  i++;
	  break;
	}
      }
      for (;i < 4; i++) {
	if (imm_h[i] != 0xffffL) {
	  movk(dst, (u_int32_t)imm_h[i], (i << 4));
	}
      }
    } else {
      // use a MOVZ and 3 MOVKs (makes it easier to debug)
      movz(dst, (u_int32_t)imm_h[0], 0);
      for (i = 1; i < 4; i++) {
	movk(dst, (u_int32_t)imm_h[i], (i << 4));
      }
    }
  }
}

void MacroAssembler::mov_immediate32(Register dst, u_int32_t imm32)
{
#ifndef PRODUCT
    {
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "0x%"PRIX32, imm32);
      block_comment(buffer);
    }
#endif
  if (operand_valid_for_logical_immediate(true, imm32)) {
    orrw(dst, zr, imm32);
  } else {
    // we can use MOVZ, MOVN or two calls to MOVK to build up the
    // constant
    u_int32_t imm_h[2];
    imm_h[0] = imm32 & 0xffff;
    imm_h[1] = ((imm32 >> 16) & 0xffff);
    if (imm_h[0] == 0) {
      movzw(dst, imm_h[1], 16);
    } else if (imm_h[0] == 0xffff) {
      movnw(dst, imm_h[1] ^ 0xffff, 16);
    } else if (imm_h[1] == 0) {
      movzw(dst, imm_h[0], 0);
    } else if (imm_h[1] == 0xffff) {
      movnw(dst, imm_h[0] ^ 0xffff, 0);
    } else {
      // use a MOVZ and MOVK (makes it easier to debug)
      movzw(dst, imm_h[0], 0);
      movkw(dst, imm_h[1], 16);
    }
  }
}

// Form an address from base + (offset << shift) in Rd.  Rd may or may
// not actually be used: you must use the Address that is returned.
// It is up to you to ensure that the shift provided matches the size
// of your data.
Address MacroAssembler::form_address(Register Rd, Register base, long byte_offset, int shift) {
  if (Address::offset_ok_for_immed(byte_offset, shift))
    // It fits; no need for any heroics
    return Address(base, byte_offset);

  // Don't do anything clever with negative or misaligned offsets
  unsigned mask = (1 << shift) - 1;
  if (byte_offset < 0 || byte_offset & mask) {
    mov(Rd, byte_offset);
    add(Rd, base, Rd);
    return Address(Rd);
  }

  // See if we can do this with two 12-bit offsets
  {
    unsigned long word_offset = byte_offset >> shift;
    unsigned long masked_offset = word_offset & 0xfff000;
    if (Address::offset_ok_for_immed(word_offset - masked_offset)
	&& Assembler::operand_valid_for_add_sub_immediate(masked_offset << shift)) {
      add(Rd, base, masked_offset << shift);
      word_offset -= masked_offset;
      return Address(Rd, word_offset << shift);
    }
  }

  // Do it the hard way
  mov(Rd, byte_offset << shift);
  add(Rd, base, Rd);
  return Address(Rd);
}

int MacroAssembler::corrected_idivl(Register result, Register ra, Register rb,
				    bool want_remainder, Register scratch)
{
  // Full implementation of Java idiv and irem; checks for special
  // case as described in JVM spec., p.243 & p.271.  The function
  // returns the (pc) offset of the idivl instruction - may be needed
  // for implicit exceptions.
  //
  // constraint : ra/rb =/= scratch
  //         normal case                          special case
  //
  // input : ra: dividend                         min_long
  //         rb: divisor                          -1
  //
  // result: either
  //         quotient  (= ra idiv rb)         min_long
  //         remainder (= ra irem rb)         0

  assert(ra != scratch && rb != scratch, "reg cannot be scratch");
  static const int64_t min_long = 0x80000000;
  Label normal_case, special_case;

  // check for special cases
  movw(scratch, min_long);
  cmpw(ra, scratch);
  br(Assembler::NE, normal_case);
  // check for -1 in rb
  cmnw(rb, 1);
  br(Assembler::NE, normal_case);
  if (! want_remainder)
    mov(result, ra);
  else
    mov(result, zr);
  b(special_case);

  // handle normal case
  bind(normal_case);
  int idivl_offset = offset();
  if (! want_remainder) {
    sdivw(result, ra, rb);
  } else {
    sdivw(scratch, ra, rb);
    msubw(result, scratch, rb, ra);
  }

  // normal and special case exit
  bind(special_case);

  return idivl_offset;
}

int MacroAssembler::corrected_idivq(Register result, Register ra, Register rb,
				    bool want_remainder, Register scratch)
{
  // Full implementation of Java idiv and irem; checks for special
  // case as described in JVM spec., p.243 & p.271.  The function
  // returns the (pc) offset of the idivq instruction - may be needed
  // for implicit exceptions.
  //
  // constraint : ra/rb =/= scratch
  //         normal case                          special case
  //
  // input : ra: dividend                         min_long
  //         rb: divisor                          -1
  //
  // result: either
  //         quotient  (= ra idiv rb)         min_long
  //         remainder (= ra irem rb)         0

  assert(ra != scratch && rb != scratch, "reg cannot be scratch");
  static const int64_t min_long = 0x8000000000000000ULL;
  Label normal_case, special_case, nonzero;

  // check for special cases
  mov(scratch, min_long);
  cmp(ra, scratch);
  br(Assembler::NE, normal_case);
  // check for -1 in rb
  cmn(rb, 1);
  br(Assembler::NE, normal_case);
  if (! want_remainder)
    mov(result, ra);
  else
    mov(result, zr);
  b(special_case);

  // handle normal case
  bind(normal_case);
  int idivq_offset = offset();
  if (! want_remainder) {
    sdiv(result, ra, rb);
  } else {
    sdiv(scratch, ra, rb);
    msub(result, scratch, rb, ra);
  }

  // normal and special case exit
  bind(special_case);

  return idivq_offset;
}

// MacroAssembler routines found actually to be needed

void MacroAssembler::push(Register src)
{
  str(src, Address(pre(esp, -1 * wordSize)));
}

void MacroAssembler::pop(Register dst)
{
  ldr(dst, Address(post(esp, 1 * wordSize)));
}

// Note: load_unsigned_short used to be called load_unsigned_word.
int MacroAssembler::load_unsigned_short(Register dst, Address src) {
  int off = offset();
  ldrh(dst, src);
  return off;
}

int MacroAssembler::load_unsigned_byte(Register dst, Address src) {
  int off = offset();
  ldrb(dst, src);
  return off;
}

int MacroAssembler::load_signed_short(Register dst, Address src) {
  int off = offset();
  ldrsh(dst, src);
  return off;
}

int MacroAssembler::load_signed_byte(Register dst, Address src) {
  int off = offset();
  ldrsb(dst, src);
  return off;
}

int MacroAssembler::load_signed_short32(Register dst, Address src) {
  int off = offset();
  ldrshw(dst, src);
  return off;
}

int MacroAssembler::load_signed_byte32(Register dst, Address src) {
  int off = offset();
  ldrsbw(dst, src);
  return off;
}

void MacroAssembler::load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2) {
  switch (size_in_bytes) {
  case  8:  ldr(dst, src); break;
  case  4:  ldrw(dst, src); break;
  case  2:  is_signed ? load_signed_short(dst, src) : load_unsigned_short(dst, src); break;
  case  1:  is_signed ? load_signed_byte( dst, src) : load_unsigned_byte( dst, src); break;
  default:  ShouldNotReachHere();
  }
}

void MacroAssembler::store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2) {
  switch (size_in_bytes) {
  case  8:  str(src, dst); break;
  case  4:  strw(src, dst); break;
  case  2:  strh(src, dst); break;
  case  1:  strb(src, dst); break;
  default:  ShouldNotReachHere();
  }
}

void MacroAssembler::decrementw(Register reg, int value)
{
  if (value < 0)  { incrementw(reg, -value);      return; }
  if (value == 0) {                               return; }
  if (value < (1 << 12)) { subw(reg, reg, value); return; }
  /* else */ {
    guarantee(reg != rscratch2, "invalid dst for register decrement");
    movw(rscratch2, (unsigned)value);
    subw(reg, reg, rscratch2);
  }
}

void MacroAssembler::decrement(Register reg, int value)
{
  if (value < 0)  { increment(reg, -value);      return; }
  if (value == 0) {                              return; }
  if (value < (1 << 12)) { sub(reg, reg, value); return; }
  /* else */ {
    assert(reg != rscratch2, "invalid dst for register decrement");
    mov(rscratch2, (unsigned long)value);
    sub(reg, reg, rscratch2);
  }
}

void MacroAssembler::decrementw(Address dst, int value)
{
  assert(!dst.uses(rscratch1), "invalid dst for address decrement");
  ldrw(rscratch1, dst);
  decrementw(rscratch1, value);
  strw(rscratch1, dst);
}

void MacroAssembler::decrement(Address dst, int value)
{
  assert(!dst.uses(rscratch1), "invalid address for decrement");
  ldr(rscratch1, dst);
  decrement(rscratch1, value);
  str(rscratch1, dst);
}

void MacroAssembler::incrementw(Register reg, int value)
{
  if (value < 0)  { decrementw(reg, -value);      return; }
  if (value == 0) {                               return; }
  if (value < (1 << 12)) { addw(reg, reg, value); return; }
  /* else */ {
    assert(reg != rscratch2, "invalid dst for register increment");
    movw(rscratch2, (unsigned)value);
    addw(reg, reg, rscratch2);
  }
}

void MacroAssembler::increment(Register reg, int value)
{
  if (value < 0)  { decrement(reg, -value);      return; }
  if (value == 0) {                              return; }
  if (value < (1 << 12)) { add(reg, reg, value); return; }
  /* else */ {
    assert(reg != rscratch2, "invalid dst for register increment");
    movw(rscratch2, (unsigned)value);
    add(reg, reg, rscratch2);
  }
}

void MacroAssembler::increment(Address dst, int value)
{
  assert(!dst.uses(rscratch1), "invalid dst for address increment");
  ldrw(rscratch1, dst);
  incrementw(rscratch1, value);
  strw(rscratch1, dst);
}

void MacroAssembler::incrementw(Address dst, int value)
{
  assert(!dst.uses(rscratch1), "invalid dst for address increment");
  ldr(rscratch1, dst);
  increment(rscratch1, value);
  str(rscratch1, dst);
}


void MacroAssembler::pusha() {
  push(0x7fffffff, sp);
}

void MacroAssembler::popa() {
  pop(0x7fffffff, sp);
}

// Push lots of registers in the bit set supplied.  Don't push sp.
// Return the number of registers pushed
int MacroAssembler::push(unsigned int bitset, Register stack) {
  // need to push all registers including original sp

  int words_pushed = 0;

  // Scan bitset to accumulate register pairs
  unsigned char regs[32];
  unsigned count = 0;
  for (int reg = 0; reg <= 30; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }
  regs[count++] = zr->encoding_nocheck();
  count &= ~1;  // Only push an even nuber of regs

  for (int i = count - 2; i >= 0; i-= 2) {
    stp(as_Register(regs[i]), as_Register(regs[i+1]),
	Address(pre(stack, -2 * wordSize)));
    words_pushed += 2;
  }

  return words_pushed;
}

int MacroAssembler::pop(unsigned int bitset, Register stack) {
  int words_pushed = 0;

  // Scan bitset to accumulate register pairs
  unsigned char regs[32];
  unsigned count = 0;
  for (int reg = 0; reg <= 30; reg++) {
    if (1 & bitset)
      regs[count++] = reg;
    bitset >>= 1;
  }
  regs[count++] = zr->encoding_nocheck();
  count &= ~1;

  for (unsigned i = 0; i < count; i+= 2) {
    ldp(as_Register(regs[i]), as_Register(regs[i+1]),
	Address(post(stack, 2 * wordSize)));
    words_pushed += 2;
  }

  return words_pushed;
}

#ifdef ASSERT 
void MacroAssembler::verify_heapbase(const char* msg) {
#if 0
  assert (UseCompressedOops || UseCompressedKlassPointers, "should be compressed");
  assert (Universe::heap() != NULL, "java heap should be initialized");
  if (CheckCompressedOops) {
    Label ok;
    push(1 << rscratch1->encoding(), sp); // cmpptr trashes rscratch1
    cmpptr(rheapbase, ExternalAddress((address)Universe::narrow_ptrs_base_addr()));
    br(Assembler::EQ, ok);
    stop(msg);
    bind(ok);
    pop(1 << rscratch1->encoding(), sp);
  }
#endif
}
#endif

void MacroAssembler::stop(const char* msg) {
  address ip = pc();
  pusha();
  mov(c_rarg0, (address)msg);
  mov(c_rarg1, (address)ip);
  mov(c_rarg2, sp);
  mov(c_rarg3, CAST_FROM_FN_PTR(address, MacroAssembler::debug64));
  // call(c_rarg3);
  blrt(c_rarg3, 3, 0, 1);
  hlt(0);
}

void MacroAssembler::enter()
{
  stp(rfp, lr, Address(pre(sp, -2 * wordSize)));
  mov(rfp, sp);
}

void MacroAssembler::leave()
{
  mov(sp, rfp);
  ldp(rfp, lr, Address(post(sp, 2 * wordSize)));
}

// If a constant does not fit in an immediate field, generate some
// number of MOV instructions and then perform the operation.
void MacroAssembler::wrap_add_sub_imm_insn(Register Rd, Register Rn, unsigned imm,
					   add_sub_imm_insn insn1,
					   add_sub_reg_insn insn2) {
  assert(Rd != zr, "Rd = zr and not setting flags?");
  if (operand_valid_for_add_sub_immediate((int)imm)) {
    (this->*insn1)(Rd, Rn, imm);
  } else {
    if (labs(imm) < (1 << 24)) {
       (this->*insn1)(Rd, Rn, imm & -(1 << 12));
       (this->*insn1)(Rd, Rd, imm & ((1 << 12)-1));
    } else {
       assert_different_registers(Rd, Rn);
       mov(Rd, (uint64_t)imm);
       (this->*insn2)(Rd, Rn, Rd, LSL, 0);
    }
  }
}

// Seperate vsn which sets the flags. Optimisations are more restricted
// because we must set the flags correctly.
void MacroAssembler::wrap_adds_subs_imm_insn(Register Rd, Register Rn, unsigned imm,
					   add_sub_imm_insn insn1,
					   add_sub_reg_insn insn2) {
  if (operand_valid_for_add_sub_immediate((int)imm)) {
    (this->*insn1)(Rd, Rn, imm);
  } else {
    assert_different_registers(Rd, Rn);
    assert(Rd != zr, "overflow in immediate operand");
    mov(Rd, (uint64_t)imm);
    (this->*insn2)(Rd, Rn, Rd, LSL, 0);
  }
}


void MacroAssembler::add(Register Rd, Register Rn, RegisterOrConstant increment) {
  if (increment.is_register()) {
    add(Rd, Rn, increment.as_register());
  } else {
    add(Rd, Rn, increment.as_constant());
  }
}

void MacroAssembler::addw(Register Rd, Register Rn, RegisterOrConstant increment) {
  if (increment.is_register()) {
    add(Rd, Rn, increment.as_register());
  } else {
    add(Rd, Rn, increment.as_constant());
  }
}

#ifdef ASSERT
static Register spill_registers[] = {
  rheapbase,
  rcpool,
  rmonitors,
  rlocals,
  rmethod
};

#define spill_msg(_reg) \
  "register " _reg " invalid after call"

static const char *spill_error_msgs[] = {
  spill_msg("rheapbase"),
  spill_msg("rcpool"),
  spill_msg("rmonitors"),
  spill_msg("rlocals"),
  spill_msg("rmethod")
};

#define SPILL_FRAME_COUNT (sizeof(spill_registers)/sizeof(spill_registers[0]))

#define SPILL_FRAME_BYTESIZE (SPILL_FRAME_COUNT * wordSize)

void MacroAssembler::spill(Register rscratcha, Register rscratchb)
{
#if 0
  Label bumped;
  // load and bump spill pointer
  ldr(rscratcha, Address(rthread, JavaThread::spill_stack_offset()));
  sub(rscratcha, rscratcha, SPILL_FRAME_BYTESIZE);
  // check for overflow
  ldr(rscratchb, Address(rthread, JavaThread::spill_stack_limit_offset()));
  cmp(rscratcha, rscratchb);
  br(Assembler::GE, bumped);
  stop("oops! ran out of register spill area");
  // spill registers
  bind(bumped);
  for (int i = 0; i < (int)SPILL_FRAME_COUNT; i++) {
    Register r = spill_registers[i];
    assert(r != rscratcha && r != rscratchb, "invalid scratch reg in spill");
    str(r, Address(rscratcha, (i * wordSize)));
  }
  // store new spill pointer
  str(rscratcha, (Address(rthread, JavaThread::spill_stack_offset())));
#endif
}

void MacroAssembler::spillcheck(Register rscratcha, Register rscratchb)
{
#if 0
  // load spill pointer
  ldr(rscratcha, (Address(rthread, JavaThread::spill_stack_offset())));
  // check registers
  for (int i = 0; i < (int)SPILL_FRAME_COUNT; i++) {
    Register r = spill_registers[i];
    assert(r != rscratcha && r != rscratchb, "invalid scratch reg in spillcheck");
    // native code is allowed to modify rcpool
    Label valid;
    ldr(rscratchb, Address(rscratcha, (i * wordSize)));
    cmp(r, rscratchb);
    br(Assembler::EQ, valid);
    stop(spill_error_msgs[i]);
    bind(valid);
  }
  // decrement and store new spill pointer
  add(rscratcha, rscratcha, SPILL_FRAME_BYTESIZE);
  str(rscratcha, Address(rthread, JavaThread::spill_stack_offset()));
#endif
}

#endif // ASSERT

void MacroAssembler::reinit_heapbase()
{
  if (UseCompressedOops) {
    lea(rscratch1, ExternalAddress((address)Universe::narrow_ptrs_base_addr()));
    ldr(rheapbase, Address(rscratch1));
  }
}

// this simulates the behaviour of the x86 cmpxchg instruction using a
// load linked/store conditional pair. we use the acquire/release
// versions of these instructions so that we flush pending writes as
// per Java semantics.

// n.b the x86 version assumes the old value to be compared against is
// in rax and updates rax with the value located in memory if the
// cmpxchg fails. we supply a register for the old value explicitly

// the aarch64 load linked/store conditional instructions do not
// accept an offset. so, unlike x86, we must provide a plain register
// to identify the memory word to be compared/exchanged rather than a
// register+offset Address.

void MacroAssembler::cmpxchgptr(Register oldv, Register newv, Register addr, Register tmp,
				Label &succeed, Label &fail) {
  // oldv holds comparison value
  // newv holds value to write in exchange
  // addr identifies memory word to compare against/update
  // tmp returns 0/1 for success/failure
  Label retry_load, nope;
  
  bind(retry_load);
  // flush and load exclusive from the memory location
  // and fail if it is not what we expect
  ldaxr(tmp, addr);
  cmp(tmp, oldv);
  br(Assembler::NE, nope);
  // if we store+flush with no intervening write tmp wil be zero
  stlxr(tmp, newv, addr);
  cbzw(tmp, succeed);
  // retry so we only ever return after a load fails to compare
  // ensures we don't return a stale value after a failed write.
  b(retry_load);
  // if the memory word differs we return it in oldv and signal a fail
  bind(nope);
  mov(oldv, tmp);
  b(fail);
}

void MacroAssembler::cmpxchgw(Register oldv, Register newv, Register addr, Register tmp,
				Label &succeed, Label &fail) {
  // oldv holds comparison value
  // newv holds value to write in exchange
  // addr identifies memory word to compare against/update
  // tmp returns 0/1 for success/failure
  Label retry_load, nope;
  
  bind(retry_load);
  // flush and load exclusive from the memory location
  // and fail if it is not what we expect
  ldaxrw(tmp, addr);
  cmp(tmp, oldv);
  br(Assembler::NE, nope);
  // if we store+flush with no intervening write tmp wil be zero
  stlxrw(tmp, newv, addr);
  cbzw(tmp, succeed);
  // retry so we only ever return after a load fails to compare
  // ensures we don't return a stale value after a failed write.
  b(retry_load);
  // if the memory word differs we return it in oldv and signal a fail
  bind(nope);
  mov(oldv, tmp);
  b(fail);
}

void MacroAssembler::incr_allocated_bytes(Register thread,
                                          Register var_size_in_bytes,
                                          int con_size_in_bytes,
                                          Register t1) {
  if (!thread->is_valid()) {
    thread = rthread;
  }
  assert(t1->is_valid(), "need temp reg");

  ldr(t1, Address(thread, in_bytes(JavaThread::allocated_bytes_offset())));
  if (var_size_in_bytes->is_valid()) {
    add(t1, t1, var_size_in_bytes);
  } else {
    add(t1, t1, con_size_in_bytes);
  }
  str(t1, Address(thread, in_bytes(JavaThread::allocated_bytes_offset())));
}

#ifndef PRODUCT
extern "C" void findpc(intptr_t x);
#endif

void MacroAssembler::debug64(char* msg, int64_t pc, int64_t regs[])
{
  // In order to get locks to work, we need to fake a in_VM state
  if (ShowMessageBoxOnError ) {
    JavaThread* thread = JavaThread::current();
    JavaThreadState saved_state = thread->thread_state();
    thread->set_thread_state(_thread_in_vm);
#ifndef PRODUCT
    if (CountBytecodes || TraceBytecodes || StopInterpreterAt) {
      ttyLocker ttyl;
      BytecodeCounter::print();
    }
#endif
    // To see where a verify_oop failed, get $ebx+40/X for this frame.
    // XXX correct this offset for amd64
    // This is the value of eip which points to where verify_oop will return.
    if (os::message_box(msg, "Execution stopped, print registers?")) {
      ttyLocker ttyl;
      tty->print_cr(" pc = 0x%016lx", pc);
#ifndef PRODUCT
      tty->cr();
      findpc(pc);
      tty->cr();
#endif
      tty->print_cr(" r0 = 0x%016lx", regs[0]);
      tty->print_cr(" r1 = 0x%016lx", regs[1]);
      tty->print_cr(" r2 = 0x%016lx", regs[2]);
      tty->print_cr(" r3 = 0x%016lx", regs[3]);
      tty->print_cr(" r4 = 0x%016lx", regs[4]);
      tty->print_cr(" r5 = 0x%016lx", regs[5]);
      tty->print_cr(" r6 = 0x%016lx", regs[6]);
      tty->print_cr(" r7 = 0x%016lx", regs[7]);
      tty->print_cr(" r8 = 0x%016lx", regs[8]);
      tty->print_cr(" r9 = 0x%016lx", regs[9]);
      tty->print_cr("r10 = 0x%016lx", regs[10]);
      tty->print_cr("r11 = 0x%016lx", regs[11]);
      tty->print_cr("r12 = 0x%016lx", regs[12]);
      tty->print_cr("r13 = 0x%016lx", regs[13]);
      tty->print_cr("r14 = 0x%016lx", regs[14]);
      tty->print_cr("r15 = 0x%016lx", regs[15]);
      tty->print_cr("r16 = 0x%016lx", regs[16]);
      tty->print_cr("r17 = 0x%016lx", regs[17]);
      tty->print_cr("r18 = 0x%016lx", regs[18]);
      tty->print_cr("r19 = 0x%016lx", regs[19]);
      tty->print_cr("r20 = 0x%016lx", regs[20]);
      tty->print_cr("r21 = 0x%016lx", regs[21]);
      tty->print_cr("r22 = 0x%016lx", regs[22]);
      tty->print_cr("r23 = 0x%016lx", regs[23]);
      tty->print_cr("r24 = 0x%016lx", regs[24]);
      tty->print_cr("r25 = 0x%016lx", regs[25]);
      tty->print_cr("r26 = 0x%016lx", regs[26]);
      tty->print_cr("r27 = 0x%016lx", regs[27]);
      tty->print_cr("r28 = 0x%016lx", regs[28]);
      tty->print_cr("r30 = 0x%016lx", regs[30]);
      tty->print_cr("r31 = 0x%016lx", regs[31]);
      BREAKPOINT;
    }
    ThreadStateTransition::transition(thread, _thread_in_vm, saved_state);
  } else {
    ttyLocker ttyl;
    ::tty->print_cr("=============== DEBUG MESSAGE: %s ================\n",
                    msg);
    assert(false, err_msg("DEBUG MESSAGE: %s", msg));
  }
}

#ifdef BUILTIN_SIM
// routine to generate an x86 prolog for a stub function which
// bootstraps into the generated ARM code which directly follows the
// stub
//
// the argument encodes the number of general and fp registers
// passed by the caller and the callng convention (currently just
// the number of general registers and assumes C argument passing)

extern "C" {
int aarch64_stub_prolog_size();
void aarch64_stub_prolog();
void setup_arm_sim(void *sp, int calltype);
}

void MacroAssembler::c_stub_prolog(int gp_arg_count, int fp_arg_count, int ret_type)
{
  int calltype = (((ret_type & 0x3) << 8) |
		  ((fp_arg_count & 0xf) << 4) |
		  (gp_arg_count & 0xf));

  // the addresses for the x86 to ARM entry code we need to use
  address start = pc();
  // printf("start = %lx\n", start);
  int byteCount =  aarch64_stub_prolog_size();
  // printf("byteCount = %x\n", byteCount);
  int instructionCount = (byteCount + 3)/ 4;
  // printf("instructionCount = %x\n", instructionCount);
  for (int i = 0; i < instructionCount; i++) {
    nop();
  }

  memcpy(start, (void*)aarch64_stub_prolog, byteCount);

  // write the address of the setup routine and the call format at the
  // end of into the copied code
  u_int64_t *patch_end = (u_int64_t *)(start + byteCount);
  patch_end[-2] = (u_int64_t)setup_arm_sim;
  patch_end[-1] = calltype;
}
#endif

void MacroAssembler::push_CPU_state() {
    push(0x3fffffff, sp);         // integer registers except lr & sp

    for (int i = 30; i >= 0; i -= 2)
      stpd(as_FloatRegister(i), as_FloatRegister(i+1),
	   Address(pre(sp, -2 * wordSize)));
}

void MacroAssembler::pop_CPU_state() {
  for (int i = 0; i < 32; i += 2)
    ldpd(as_FloatRegister(i), as_FloatRegister(i+1),
	 Address(post(sp, 2 * wordSize)));

  pop(0x3fffffff, sp);         // integer registers except lr & sp
}

SkipIfEqual::SkipIfEqual(
    MacroAssembler* masm, const bool* flag_addr, bool value) {
  _masm = masm;
  unsigned long offset;
  _masm->adrp(rscratch1, ExternalAddress((address)flag_addr), offset);
  _masm->ldrb(rscratch1, Address(rscratch1, offset));
  _masm->cbzw(rscratch1, _label);
}

SkipIfEqual::~SkipIfEqual() {
  _masm->bind(_label);
}

void MacroAssembler::cmpptr(Register src1, Address src2) {
  unsigned long offset;
  adrp(rscratch1, src2, offset);
  ldr(rscratch1, Address(rscratch1, offset));
  cmp(src1, rscratch1);
}

void MacroAssembler::store_check(Register obj) {
  // Does a store check for the oop in register obj. The content of
  // register obj is destroyed afterwards.
  store_check_part_1(obj);
  store_check_part_2(obj);
}

void MacroAssembler::store_check(Register obj, Address dst) {
  store_check(obj);
}


// split the store check operation so that other instructions can be scheduled inbetween
void MacroAssembler::store_check_part_1(Register obj) {
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
  lsr(obj, obj, CardTableModRefBS::card_shift);
}

void MacroAssembler::store_check_part_2(Register obj) {
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->kind() == BarrierSet::CardTableModRef, "Wrong barrier set kind");
  CardTableModRefBS* ct = (CardTableModRefBS*)bs;
  assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

  // The calculation for byte_map_base is as follows:
  // byte_map_base = _byte_map - (uintptr_t(low_bound) >> card_shift);
  // So this essentially converts an address to a displacement and
  // it will never need to be relocated.

  // FIXME: It's not likely that disp will fit into an offset so we
  // don't bother to check, but it could save an instruction.
  intptr_t disp = (intptr_t) ct->byte_map_base;
  mov(rscratch1, disp);
  strb(zr, Address(obj, rscratch1));
}

void MacroAssembler::load_klass(Register dst, Register src) {
  if (UseCompressedKlassPointers) {
    ldrw(dst, Address(src, oopDesc::klass_offset_in_bytes()));
    decode_klass_not_null(dst);
  } else {
    ldr(dst, Address(src, oopDesc::klass_offset_in_bytes()));
  }
}

void MacroAssembler::store_klass(Register dst, Register src) {
  if (UseCompressedKlassPointers) {
    encode_klass_not_null(src);
    strw(src, Address(dst, oopDesc::klass_offset_in_bytes()));
  } else {
    str(src, Address(dst, oopDesc::klass_offset_in_bytes()));
  }
}

void MacroAssembler::store_klass_gap(Register dst, Register src) {
  if (UseCompressedKlassPointers) {
    // Store to klass gap in destination
    str(src, Address(dst, oopDesc::klass_gap_offset_in_bytes()));
  }
}

// Algorithm must match oop.inline.hpp encode_heap_oop.
void MacroAssembler::encode_heap_oop(Register d, Register s) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop: heap base corrupted?");
#endif
  verify_oop(s, "broken oop in encode_heap_oop");
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      lsr(d, s, LogMinObjAlignmentInBytes);
    } else {
      mov(d, s);
    }
  } else {
    subs(d, s, rheapbase);
    csel(d, d, zr, Assembler::HS);
    lsr(d, d, LogMinObjAlignmentInBytes);

    /*  Old algorithm: is this any worse?
    Label nonnull;
    cbnz(r, nonnull);
    sub(r, r, rheapbase);
    bind(nonnull);
    lsr(r, r, LogMinObjAlignmentInBytes);
    */
  }
}

void MacroAssembler::encode_heap_oop_not_null(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop_not_null: heap base corrupted?");
  if (CheckCompressedOops) {
    Label ok;
    cbnz(r, ok);
    stop("null oop passed to encode_heap_oop_not_null");
    bind(ok);
  }
#endif
  verify_oop(r, "broken oop in encode_heap_oop_not_null");
  if (Universe::narrow_oop_base() != NULL) {
    sub(r, r, rheapbase);
  }
  if (Universe::narrow_oop_shift() != 0) {
    assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    lsr(r, r, LogMinObjAlignmentInBytes);
  }
}

void MacroAssembler::encode_heap_oop_not_null(Register dst, Register src) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_heap_oop_not_null2: heap base corrupted?");
  if (CheckCompressedOops) {
    Label ok;
    cbnz(src, ok);
    stop("null oop passed to encode_heap_oop_not_null2");
    bind(ok);
  }
#endif
  verify_oop(src, "broken oop in encode_heap_oop_not_null2");

  Register data = src;
  if (Universe::narrow_oop_base() != NULL) {
    sub(dst, src, rheapbase);
    data = dst;
  }
  if (Universe::narrow_oop_shift() != 0) {
    assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    lsr(dst, data, LogMinObjAlignmentInBytes);
    data = dst;
  }
  if (data == src)
    mov(dst, src);
}

void  MacroAssembler::decode_heap_oop(Register r) {
#ifdef ASSERT
  verify_heapbase("MacroAssembler::decode_heap_oop: heap base corrupted?");
#endif
  if (Universe::narrow_oop_base() == NULL) {
    if (Universe::narrow_oop_shift() != 0) {
      assert (LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
      lsl(r, r, LogMinObjAlignmentInBytes);
    }
  } else {
    Label done;
    cbz(r, done);
    add(r, rheapbase, r, Assembler::LSL, LogMinObjAlignmentInBytes);
    bind(done);
  }
  verify_oop(r, "broken oop in decode_heap_oop");
}

void  MacroAssembler::decode_heap_oop_not_null(Register r) {
  // Note: it will change flags
  assert (UseCompressedOops, "should only be used for compressed headers");
  assert (Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (Universe::narrow_oop_base() != NULL) {
      add(r, rheapbase, r, Assembler::LSL, LogMinObjAlignmentInBytes);
    } else {
      add(r, zr, r, Assembler::LSL, LogMinObjAlignmentInBytes);
    }
  } else {
    assert (Universe::narrow_oop_base() == NULL, "sanity");
  }
}

void  MacroAssembler::decode_heap_oop_not_null(Register dst, Register src) {
  // Note: it will change flags
  assert (UseCompressedOops, "should only be used for compressed headers");
  assert (Universe::heap() != NULL, "java heap should be initialized");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_oop_shift() != 0) {
    assert(LogMinObjAlignmentInBytes == Universe::narrow_oop_shift(), "decode alg wrong");
    if (Universe::narrow_oop_base() != NULL) {
      add(dst, rheapbase, src, Assembler::LSL, LogMinObjAlignmentInBytes);
    } else {
      add(dst, zr, src, Assembler::LSL, LogMinObjAlignmentInBytes);
    }
  } else {
    assert (Universe::narrow_oop_base() == NULL, "sanity");
    if (dst != src) {
      mov(dst, src);
    }
  }
}

void MacroAssembler::encode_klass_not_null(Register r) {
  assert(Metaspace::is_initialized(), "metaspace should be initialized");
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_klass_not_null: heap base corrupted?");
#endif
  if (Universe::narrow_klass_base() != NULL) {
    sub(r, r, rheapbase);
  }
  if (Universe::narrow_klass_shift() != 0) {
    assert (LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    lsr(r, r, LogKlassAlignmentInBytes);
  }
}

void MacroAssembler::encode_klass_not_null(Register dst, Register src) {
  assert(Metaspace::is_initialized(), "metaspace should be initialized");
#ifdef ASSERT
  verify_heapbase("MacroAssembler::encode_klass_not_null2: heap base corrupted?");
#endif
  if (dst != src) {
    mov(dst, src);
  }
  if (Universe::narrow_klass_base() != NULL) {
    sub(dst, dst, rheapbase);
  }
  if (Universe::narrow_klass_shift() != 0) {
    assert (LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    lsr(dst, dst, LogKlassAlignmentInBytes);
  }
}

void  MacroAssembler::decode_klass_not_null(Register r) {
  assert(Metaspace::is_initialized(), "metaspace should be initialized");
  // Note: it will change flags
  assert (UseCompressedKlassPointers, "should only be used for compressed headers");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_klass_shift() != 0) {
    assert(LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    if (Universe::narrow_klass_base() != NULL) {
      add(r, rheapbase, r, Assembler::LSL, LogKlassAlignmentInBytes);
    } else {
      add(r, zr, r, Assembler::LSL, LogKlassAlignmentInBytes);
    }
  } else {
    assert (Universe::narrow_klass_base() == NULL, "sanity");
  }
}

void  MacroAssembler::decode_klass_not_null(Register dst, Register src) {
  assert(Metaspace::is_initialized(), "metaspace should be initialized");
  // Note: it will change flags
  assert (UseCompressedKlassPointers, "should only be used for compressed headers");
  // Cannot assert, unverified entry point counts instructions (see .ad file)
  // vtableStubs also counts instructions in pd_code_size_limit.
  // Also do not verify_oop as this is called by verify_oop.
  if (Universe::narrow_klass_shift() != 0) {
    assert(LogKlassAlignmentInBytes == Universe::narrow_klass_shift(), "decode alg wrong");
    if (Universe::narrow_klass_base() != NULL) {
      add(dst, rheapbase, src, Assembler::LSL, LogKlassAlignmentInBytes);
    } else {
      add(dst, zr, src, Assembler::LSL, LogKlassAlignmentInBytes);
    }
  } else {
    assert (Universe::narrow_klass_base() == NULL, "sanity");
    if (dst != src) {
      mov(dst, src);
    }
  }
}

// TODO
//
// these next two methods load a narrow oop or klass constant into a
// register. they currently do the dumb thing of installing 64 bits of
// unencoded constant into the register and then encoding it.
// installing the encoded 32 bit constant directly requires updating
// the relocation code so it can recognize that this is a 32 bit load
// rather than a 64 bit load.

void  MacroAssembler::set_narrow_oop(Register dst, jobject obj) {
  assert (UseCompressedOops, "should only be used for compressed headers");
  assert (Universe::heap() != NULL, "java heap should be initialized");
  assert (oop_recorder() != NULL, "this assembler needs an OopRecorder");
  movoop(dst, obj);
  encode_heap_oop_not_null(dst);
}


void  MacroAssembler::set_narrow_klass(Register dst, Klass* k) {
  assert (UseCompressedKlassPointers, "should only be used for compressed headers");
  assert (oop_recorder() != NULL, "this assembler needs an OopRecorder");
  mov_metadata(dst, k);
  encode_klass_not_null(dst);
}

void MacroAssembler::load_heap_oop(Register dst, Address src)
{
  if (UseCompressedOops) {
    ldrw(dst, src);
    decode_heap_oop(dst);
  } else {
    ldr(dst, src);
  }  
}

void MacroAssembler::load_heap_oop_not_null(Register dst, Address src)
{
  if (UseCompressedOops) {
    ldrw(dst, src);
    decode_heap_oop_not_null(dst);
  } else {
    ldr(dst, src);
  }  
}

void MacroAssembler::store_heap_oop(Address dst, Register src) {
  if (UseCompressedOops) {
    assert(!dst.uses(src), "not enough registers");
    encode_heap_oop(src);
    strw(src, dst);
  } else
    str(src, dst);
}

// Used for storing NULLs.
void MacroAssembler::store_heap_oop_null(Address dst) {
  if (UseCompressedOops) {
    strw(zr, dst);
  } else
    str(zr, dst);
}

#if INCLUDE_ALL_GCS
void MacroAssembler::g1_write_barrier_pre(Register obj,
                                          Register pre_val,
                                          Register thread,
                                          Register tmp,
                                          bool tosca_live,
                                          bool expand_call) { Unimplemented(); }

void MacroAssembler::g1_write_barrier_post(Register store_addr,
                                           Register new_val,
                                           Register thread,
                                           Register tmp,
                                           Register tmp2) { Unimplemented(); }

#endif // INCLUDE_ALL_GCS

Address MacroAssembler::allocate_metadata_address(Metadata* obj) {
  assert(oop_recorder() != NULL, "this assembler needs a Recorder");
  int index = oop_recorder()->allocate_metadata_index(obj);
  RelocationHolder rspec = metadata_Relocation::spec(index);
  return Address((address)obj, rspec);
}

void MacroAssembler::movoop(Register dst, jobject obj) {
  int oop_index;
  if (obj == NULL) {
    oop_index = oop_recorder()->allocate_oop_index(obj);
  } else {
    oop_index = oop_recorder()->find_index(obj);
    assert(Universe::heap()->is_in_reserved(JNIHandles::resolve(obj)), "should be real oop");
  }
  RelocationHolder rspec = oop_Relocation::spec(oop_index);
  address const_ptr = long_constant((jlong)obj);
  if (! const_ptr) {
    mov(dst, Address((address)obj, rspec));
  } else {
    code()->consts()->relocate(const_ptr, rspec);
    ldr_constant(dst, const_ptr);
  }
}

void MacroAssembler::mov_metadata(Register dst, Metadata* obj) {
  int oop_index;
  if (obj == NULL) {
    oop_index = oop_recorder()->allocate_metadata_index(obj);
  } else {
    oop_index = oop_recorder()->find_index(obj);
  }
  RelocationHolder rspec = metadata_Relocation::spec(oop_index);
  address const_ptr = long_constant((jlong)obj);
  if (! const_ptr) {
    mov(dst, Address((address)obj, rspec));
  } else {
    code()->consts()->relocate(const_ptr, rspec);
    ldr_constant(dst, const_ptr);
  }
}

Address MacroAssembler::constant_oop_address(jobject obj) {
  assert(oop_recorder() != NULL, "this assembler needs an OopRecorder");
  assert(Universe::heap()->is_in_reserved(JNIHandles::resolve(obj)), "not an oop");
  int oop_index = oop_recorder()->find_index(obj);
  return Address((address)obj, oop_Relocation::spec(oop_index));
}

// Defines obj, preserves var_size_in_bytes, okay for t2 == var_size_in_bytes.
void MacroAssembler::tlab_allocate(Register obj,
                                   Register var_size_in_bytes,
                                   int con_size_in_bytes,
                                   Register t1,
                                   Register t2,
                                   Label& slow_case) {
  assert_different_registers(obj, t1, t2);
  assert_different_registers(obj, var_size_in_bytes, t1);
  Register end = t2;

  // verify_tlab();

  ldr(obj, Address(rthread, JavaThread::tlab_top_offset()));
  if (var_size_in_bytes == noreg) {
    lea(end, Address(obj, con_size_in_bytes));
  } else {
    lea(end, Address(obj, var_size_in_bytes));
  }
  ldr(rscratch1, Address(rthread, JavaThread::tlab_end_offset()));
  cmp(end, rscratch1);
  br(Assembler::HI, slow_case);

  // update the tlab top pointer
  str(end, Address(rthread, JavaThread::tlab_top_offset()));

  // recover var_size_in_bytes if necessary
  if (var_size_in_bytes == end) {
    sub(var_size_in_bytes, var_size_in_bytes, obj);
  }
  // verify_tlab();
}

// Preserves r19, and r3.
Register MacroAssembler::tlab_refill(Label& retry,
                                     Label& try_eden,
                                     Label& slow_case) {
  Register top = r0;
  Register t1  = r2;
  Register t2  = r4;
  assert_different_registers(top, rthread, t1, t2, /* preserve: */ r19, r3);
  Label do_refill, discard_tlab;

  if (CMSIncrementalMode || !Universe::heap()->supports_inline_contig_alloc()) {
    // No allocation in the shared eden.
    b(slow_case);
  }

  ldr(top, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
  ldr(t1,  Address(rthread, in_bytes(JavaThread::tlab_end_offset())));

  // calculate amount of free space
  sub(t1, t1, top);
  lsr(t1, t1, LogHeapWordSize);

  // Retain tlab and allocate object in shared space if
  // the amount free in the tlab is too large to discard.

  ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_refill_waste_limit_offset())));
  cmp(t1, rscratch1);
  br(Assembler::LE, discard_tlab);

  // Retain
  // ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_refill_waste_limit_offset())));
  mov(t2, (int32_t) ThreadLocalAllocBuffer::refill_waste_limit_increment());
  add(rscratch1, rscratch1, t2);
  str(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_refill_waste_limit_offset())));

  if (TLABStats) {
    // increment number of slow_allocations
    addmw(Address(rthread, in_bytes(JavaThread::tlab_slow_allocations_offset())),
	 1, rscratch1);
  }
  b(try_eden);

  bind(discard_tlab);
  if (TLABStats) {
    // increment number of refills
    addmw(Address(rthread, in_bytes(JavaThread::tlab_number_of_refills_offset())), 1,
	 rscratch1);
    // accumulate wastage -- t1 is amount free in tlab
    addmw(Address(rthread, in_bytes(JavaThread::tlab_fast_refill_waste_offset())), t1,
	 rscratch1);
  }

  // if tlab is currently allocated (top or end != null) then
  // fill [top, end + alignment_reserve) with array object
  cbz(top, do_refill);

  // set up the mark word
  mov(rscratch1, (intptr_t)markOopDesc::prototype()->copy_set_hash(0x2));
  str(rscratch1, Address(top, oopDesc::mark_offset_in_bytes()));
  // set the length to the remaining space
  sub(t1, t1, typeArrayOopDesc::header_size(T_INT));
  add(t1, t1, (int32_t)ThreadLocalAllocBuffer::alignment_reserve());
  lsl(t1, t1, log2_intptr(HeapWordSize/sizeof(jint)));
  strw(t1, Address(top, arrayOopDesc::length_offset_in_bytes()));
  // set klass to intArrayKlass
  {
    unsigned long offset;
    // dubious reloc why not an oop reloc?
    adrp(rscratch1, ExternalAddress((address)Universe::intArrayKlassObj_addr()),
	 offset);
    ldr(t1, Address(rscratch1, offset));
  }
  // store klass last.  concurrent gcs assumes klass length is valid if
  // klass field is not null.
  store_klass(top, t1);

  mov(t1, top);
  ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_start_offset())));
  sub(t1, t1, rscratch1);
  incr_allocated_bytes(rthread, t1, 0, rscratch1);

  // refill the tlab with an eden allocation
  bind(do_refill);
  ldr(t1, Address(rthread, in_bytes(JavaThread::tlab_size_offset())));
  lsl(t1, t1, LogHeapWordSize);
  // allocate new tlab, address returned in top
  eden_allocate(top, t1, 0, t2, slow_case);

  // Check that t1 was preserved in eden_allocate.
#ifdef ASSERT
  if (UseTLAB) {
    Label ok;
    Register tsize = r4;
    assert_different_registers(tsize, rthread, t1);
    str(tsize, Address(pre(sp, -16)));
    ldr(tsize, Address(rthread, in_bytes(JavaThread::tlab_size_offset())));
    lsl(tsize, tsize, LogHeapWordSize);
    cmp(t1, tsize);
    br(Assembler::EQ, ok);
    STOP("assert(t1 != tlab size)");
    should_not_reach_here();

    bind(ok);
    ldr(tsize, Address(post(sp, 16)));
  }
#endif
  str(top, Address(rthread, in_bytes(JavaThread::tlab_start_offset())));
  str(top, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
  add(top, top, t1);
  sub(top, top, (int32_t)ThreadLocalAllocBuffer::alignment_reserve_in_bytes());
  str(top, Address(rthread, in_bytes(JavaThread::tlab_end_offset())));
  verify_tlab();
  b(retry);

  return rthread; // for use by caller
}

// Defines obj, preserves var_size_in_bytes
void MacroAssembler::eden_allocate(Register obj,
                                   Register var_size_in_bytes,
                                   int con_size_in_bytes,
                                   Register t1,
                                   Label& slow_case) {
  assert_different_registers(obj, var_size_in_bytes, t1);
  if (CMSIncrementalMode || !Universe::heap()->supports_inline_contig_alloc()) {
    b(slow_case);
  } else {
    Register end = t1;
    Register heap_end = rscratch2;
    Label retry;
    bind(retry);
    {
      unsigned long offset;
      adrp(rscratch1, ExternalAddress((address) Universe::heap()->end_addr()), offset);
      ldr(heap_end, Address(rscratch1, offset));
    }

    ExternalAddress heap_top((address) Universe::heap()->top_addr());

    // Get the current top of the heap
    {
      unsigned long offset;
      adrp(rscratch1, heap_top, offset);
      lea(rscratch1, Address(rscratch1, offset));
      ldaxr(obj, rscratch1);
    }

    // Adjust it my the size of our new object
    if (var_size_in_bytes == noreg) {
      lea(end, Address(obj, con_size_in_bytes));
    } else {
      lea(end, Address(obj, var_size_in_bytes));
    }

    // if end < obj then we wrapped around high memory
    cmp(end, obj);
    br(Assembler::LO, slow_case);

    cmp(end, heap_end);
    br(Assembler::HI, slow_case);

    // If heap_top hasn't been changed by some other thread, update it.
    stlxr(rscratch1, end, rscratch1);
    cbnzw(rscratch1, retry);
  }
}

void MacroAssembler::verify_tlab() {
#ifdef ASSERT
  if (UseTLAB && VerifyOops) {
    Label next, ok;

    stp(rscratch2, rscratch1, Address(pre(sp, -16)));

    ldr(rscratch2, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
    ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_start_offset())));
    cmp(rscratch2, rscratch1);
    br(Assembler::HS, next);
    STOP("assert(top >= start)");
    should_not_reach_here();

    bind(next);
    ldr(rscratch2, Address(rthread, in_bytes(JavaThread::tlab_end_offset())));
    ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
    cmp(rscratch2, rscratch1);
    br(Assembler::HS, ok);
    STOP("assert(top <= end)");
    should_not_reach_here();

    bind(ok);
    ldp(rscratch2, rscratch1, Address(post(sp, 16)));
  }
#endif
}

// Writes to stack successive pages until offset reached to check for
// stack overflow + shadow pages.  This clobbers tmp.
void MacroAssembler::bang_stack_size(Register size, Register tmp) {
  assert_different_registers(tmp, size, rscratch1);
  mov(tmp, sp);
  // Bang stack for total size given plus shadow page size.
  // Bang one page at a time because large size can bang beyond yellow and
  // red zones.
  Label loop;
  mov(rscratch1, os::vm_page_size());
  bind(loop);
  lea(tmp, Address(tmp, -os::vm_page_size()));
  subsw(size, size, rscratch1);
  str(size, Address(tmp));
  br(Assembler::GT, loop);

  // Bang down shadow pages too.
  // The -1 because we already subtracted 1 page.
  for (int i = 0; i< StackShadowPages-1; i++) {
    // this could be any sized move but this is can be a debugging crumb
    // so the bigger the better.
    lea(tmp, Address(tmp, -os::vm_page_size()));
    str(size, Address(tmp));
  }
}


address MacroAssembler::read_polling_page(Register r, address page, relocInfo::relocType rtype) {
  unsigned long off;
  adrp(r, Address(page, rtype), off);
  InstructionMark im(this);
  code_section()->relocate(inst_mark(), rtype);
  ldrw(zr, Address(r, off));
  return inst_mark();
}

address MacroAssembler::read_polling_page(Register r, relocInfo::relocType rtype) {
  InstructionMark im(this);
  code_section()->relocate(inst_mark(), rtype);
  ldrw(zr, Address(r, 0));
  return inst_mark();
}

void MacroAssembler::adrp(Register reg1, const Address &dest, unsigned long &byte_offset) {
  if (labs(pc() - dest.target()) >= (1LL << 32)) {
    // Out of range.  This doesn't happen very often, but we have to
    // handle it
    mov(reg1, dest);
    byte_offset = 0;
  } else {
    InstructionMark im(this);
    code_section()->relocate(inst_mark(), dest.rspec());
    byte_offset = (uint64_t)dest.target() & 0xfff;
    _adrp(reg1, dest.target());
  }
}

void MacroAssembler::generate_flush_loop(flush_insn flush, Register start, Register lines) {
  Label again, exit;

  assert_different_registers(start, lines, rscratch1, rscratch2);

  cmp(lines, zr);
  br(Assembler::LE, exit);
  mov(rscratch1, start);
  mov(rscratch2, lines);
  bind(again);
  (this->*flush)(rscratch1);
  sub(rscratch2, rscratch2, 1);
  add(rscratch1, rscratch1, ICache::line_size);
  cbnz(rscratch2, again);
  bind(exit);
}
