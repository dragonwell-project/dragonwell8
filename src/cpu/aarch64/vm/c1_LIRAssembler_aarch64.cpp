/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
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
#include "c1/c1_Compilation.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_ValueStack.hpp"
#include "ci/ciArrayKlass.hpp"
#include "ci/ciInstance.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "memory/barrierSet.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "nativeInst_aarch64.hpp"
#include "oops/objArrayKlass.hpp"
#include "runtime/sharedRuntime.hpp"



NEEDS_CLEANUP // remove this definitions ?
const Register IC_Klass    = r0;   // where the IC klass is cached
const Register SYNC_header = r0;   // synchronization header
const Register SHIFT_count = r0;   // where count for shift operations must be

#define __ _masm->


static void select_different_registers(Register preserve,
                                       Register extra,
                                       Register &tmp1,
                                       Register &tmp2) { Unimplemented(); }



static void select_different_registers(Register preserve,
                                       Register extra,
                                       Register &tmp1,
                                       Register &tmp2,
                                       Register &tmp3) { Unimplemented(); }



bool LIR_Assembler::is_small_constant(LIR_Opr opr) { Unimplemented(); return false; }


LIR_Opr LIR_Assembler::receiverOpr() {
  return FrameMap::receiver_opr;
}

LIR_Opr LIR_Assembler::osrBufferPointer() { Unimplemented(); return 0; }

//--------------fpu register translations-----------------------


address LIR_Assembler::float_constant(float f) {
  address const_addr = __ float_constant(f);
  if (const_addr == NULL) {
    bailout("const section overflow");
    return __ code()->consts()->start();
  } else {
    return const_addr;
  }
}


address LIR_Assembler::double_constant(double d) {
  address const_addr = __ double_constant(d);
  if (const_addr == NULL) {
    bailout("const section overflow");
    return __ code()->consts()->start();
  } else {
    return const_addr;
  }
}

void LIR_Assembler::set_24bit_FPU() { Unimplemented(); }

void LIR_Assembler::reset_FPU() { Unimplemented(); }

void LIR_Assembler::fpop() { Unimplemented(); }

void LIR_Assembler::fxch(int i) { Unimplemented(); }

void LIR_Assembler::fld(int i) { Unimplemented(); }

void LIR_Assembler::ffree(int i) { Unimplemented(); }

void LIR_Assembler::breakpoint() { Unimplemented(); }

void LIR_Assembler::push(LIR_Opr opr) { Unimplemented(); }

void LIR_Assembler::pop(LIR_Opr opr) { Unimplemented(); }

bool LIR_Assembler::is_literal_address(LIR_Address* addr) { Unimplemented(); return false; }
//-------------------------------------------

static Register as_reg(LIR_Opr op) {
  return op->is_double_cpu() ? op->as_register_lo() : op->as_register();
}

Address LIR_Assembler::as_Address(LIR_Address* addr, Register tmp) {
  Register base = addr->base()->as_pointer_register();
  LIR_Opr opr = addr->index();
  if (opr->is_cpu_register()) {
    Register index;
    if (opr->is_single_cpu())
      index = opr->as_register();
    else
      index = opr->as_register_lo();
    assert(addr->disp() == 0, "must be");
    return Address(base, index, Address::sxtw(addr->scale()));
  } else  {
    intptr_t addr_offset = (intptr_t(addr->disp()) << addr->scale());
    assert(Address::offset_ok_for_immed(addr_offset, 0), "must be");
    return Address(base, addr_offset);
  }
}

Address LIR_Assembler::as_Address_hi(LIR_Address* addr) {
  ShouldNotReachHere();
}

Address LIR_Assembler::as_Address(LIR_Address* addr) {
  return as_Address(addr, rscratch1);
}

Address LIR_Assembler::as_Address_lo(LIR_Address* addr) {
  return as_Address(addr, rscratch1);  // Ouch
  // FIXME: This needs to be much more clever.  See x86.
}


void LIR_Assembler::osr_entry() { Unimplemented(); }


// inline cache check; done before the frame is built.
int LIR_Assembler::check_icache() {
  Register receiver = FrameMap::receiver_opr->as_register();
  Register ic_klass = IC_Klass;
  const int ic_cmp_size = 4 * 4;
  const bool do_post_padding = VerifyOops || UseCompressedKlassPointers;
  if (!do_post_padding) {
    // insert some nops so that the verified entry point is aligned on CodeEntryAlignment
    while ((__ offset() + ic_cmp_size) % CodeEntryAlignment != 0) {
      __ nop();
    }
  }
  int offset = __ offset();
  __ inline_cache_check(receiver, IC_Klass);
  assert(__ offset() % CodeEntryAlignment == 0 || do_post_padding, "alignment must be correct");
  if (do_post_padding) {
    // force alignment after the cache check.
    // It's been verified to be aligned if !VerifyOops
    __ align(CodeEntryAlignment);
  }
  return offset;
}


void LIR_Assembler::jobject2reg(jobject o, Register reg) {
  if (o == NULL) {
    __ mov(reg, zr);
  } else {
    int oop_index = __ oop_recorder()->find_index(o);
    assert(Universe::heap()->is_in_reserved(JNIHandles::resolve(o)), "should be real oop");
    RelocationHolder rspec = oop_Relocation::spec(oop_index);
    __ mov(reg, Address(NULL_WORD, rspec)); // Will be set when the nmethod is created
  }
}

void LIR_Assembler::jobject2reg_with_patching(Register reg, CodeEmitInfo* info) { Unimplemented(); }


// This specifies the rsp decrement needed to build the frame
int LIR_Assembler::initial_frame_size_in_bytes() {
  // if rounding, must let FrameMap know!

  // The frame_map records size in slots (32bit word)

  // subtract two words to account for return address and link
  return (frame_map()->framesize() - (2*VMRegImpl::slots_per_word))  * VMRegImpl::stack_slot_size;
}


int LIR_Assembler::emit_exception_handler() {
  // if the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri)
  __ nop();

  // generate code for exception handler
  address handler_base = __ start_a_stub(exception_handler_size);
  if (handler_base == NULL) {
    // not enough space left for the handler
    bailout("exception handler overflow");
    return -1;
  }

  int offset = code_offset();

  // the exception oop and pc are in rax, and rdx
  // no other registers need to be preserved, so invalidate them
  __ invalidate_registers(false, true, true, false, true, true);

  // check that there is really an exception
  __ verify_not_null_oop(r0);

  // search an exception handler (r0: exception oop, r3: throwing pc)
  __ bl(RuntimeAddress(Runtime1::entry_for(Runtime1::handle_exception_from_callee_id)));
  __ should_not_reach_here();
  guarantee(code_offset() - offset <= exception_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}


// Emit the code to remove the frame from the stack in the exception
// unwind path.
int LIR_Assembler::emit_unwind_handler() {
#ifndef PRODUCT
  if (CommentedAssembly) {
    _masm->block_comment("Unwind handler");
  }
#endif

  int offset = code_offset();

  // Fetch the exception from TLS and clear out exception related thread state
  __ ldr(r0, Address(rthread, JavaThread::exception_oop_offset()));
  __ str(zr, Address(rthread, JavaThread::exception_oop_offset()));
  __ str(zr, Address(rthread, JavaThread::exception_pc_offset()));

  __ bind(_unwind_handler_entry);
  __ verify_not_null_oop(r0);
  if (method()->is_synchronized() || compilation()->env()->dtrace_method_probes()) {
    __ mov(r19, r0);  // Preserve the exception
  }

  // Preform needed unlocking
  MonitorExitStub* stub = NULL;
  if (method()->is_synchronized()) {
    monitor_address(0, FrameMap::r0_opr);
    stub = new MonitorExitStub(FrameMap::r0_opr, true, 0);
    __ unlock_object(r5, r4, r0, *stub->entry());
    __ bind(*stub->continuation());
  }

  if (compilation()->env()->dtrace_method_probes()) {
    __ call_Unimplemented();
#if 0
    __ movptr(Address(rsp, 0), rax);
    __ mov_metadata(Address(rsp, sizeof(void*)), method()->constant_encoding());
    __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit)));
#endif
  }

  if (method()->is_synchronized() || compilation()->env()->dtrace_method_probes()) {
    __ mov(r0, r19);  // Restore the exception
  }

  // remove the activation and dispatch to the unwind handler
  __ remove_frame(initial_frame_size_in_bytes());
  __ b(RuntimeAddress(Runtime1::entry_for(Runtime1::unwind_exception_id)));

  // Emit the slow path assembly
  if (stub != NULL) {
    stub->emit_code(this);
  }

  return offset;
}


int LIR_Assembler::emit_deopt_handler() { __ call_Unimplemented(); return 0; }


// This is the fast version of java.lang.String.compare; it has not
// OSR-entry and therefore, we generate a slow version for OSR's
void LIR_Assembler::emit_string_compare(LIR_Opr arg0, LIR_Opr arg1, LIR_Opr dst, CodeEmitInfo* info) { __ call_Unimplemented(); }



void LIR_Assembler::add_debug_info_for_branch(address adr, CodeEmitInfo* info) {
  _masm->code_section()->relocate(adr, relocInfo::poll_type);
  int pc_offset = code_offset();
  flush_debug_info(pc_offset);
  info->record_debug_info(compilation()->debug_info_recorder(), pc_offset);
  if (info->exception_handlers() != NULL) {
    compilation()->add_exception_handlers_for_pco(pc_offset, info->exception_handlers());
  }
}


void LIR_Assembler::return_op(LIR_Opr result) {
  assert(result->is_illegal() || !result->is_single_cpu() || result->as_register() == r0, "word returns are in r0,");
  // Pop the stack before the safepoint code
  __ remove_frame(initial_frame_size_in_bytes());

  bool result_is_oop = result->is_valid() ? result->is_oop() : false;

  // Note: we do not need to round double result; float result has the right precision
  // the poll sets the condition code, but no data registers
  address polling_page(os::get_polling_page() + (SafepointPollOffset % os::vm_page_size()));
  __ read_polling_page(rscratch1, polling_page, relocInfo::poll_return_type);

  __ ret(lr);
}


int LIR_Assembler::safepoint_poll(LIR_Opr tmp, CodeEmitInfo* info) {
  address polling_page(os::get_polling_page()
		       + (SafepointPollOffset % os::vm_page_size() + __ offset()));
  guarantee(info != NULL, "Shouldn't be NULL");
  address insn = __ read_polling_page(rscratch1, polling_page, relocInfo::poll_type);
  add_debug_info_for_branch(insn, info);

  return __ offset();
}


void LIR_Assembler::move_regs(Register from_reg, Register to_reg) {
  __ mov(to_reg, from_reg);
}

void LIR_Assembler::swap_reg(Register a, Register b) { Unimplemented(); }


void LIR_Assembler::const2reg(LIR_Opr src, LIR_Opr dest, LIR_PatchCode patch_code, CodeEmitInfo* info) {
  assert(src->is_constant(), "should not call otherwise");
  assert(dest->is_register(), "should not call otherwise");
  LIR_Const* c = src->as_constant_ptr();

  switch (c->type()) {
    case T_INT: {
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ mov(dest->as_register(), c->as_jint());
      break;
    }

    case T_ADDRESS: {
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ mov(dest->as_register(), c->as_jint());
      break;
    }

    case T_LONG: {
      assert(patch_code == lir_patch_none, "no patching handled here");
      __ mov(dest->as_register_lo(), (intptr_t)c->as_jlong());
      break;
    }

    case T_OBJECT: {
        if (patch_code == lir_patch_none) {
          jobject2reg(c->as_jobject(), dest->as_register());
        } else {
          jobject2reg_with_patching(dest->as_register(), info);
        }
      break;
    }

    case T_METADATA: {
      if (patch_code != lir_patch_none) {
        klass2reg_with_patching(dest->as_register(), info);
      } else {
        __ mov_metadata(dest->as_register(), c->as_metadata());
      }
      break;
    }

    case T_FLOAT: {
      if (__ operand_valid_for_float_immediate(c->as_jfloat())) {
	__ fmovs(dest->as_float_reg(), (c->as_jfloat()));
      } else {
	__ adr(rscratch1, InternalAddress(float_constant(c->as_jfloat())));
	__ ldrs(dest->as_float_reg(), Address(rscratch1));
      }
      break;
    }

    case T_DOUBLE: {
      if (__ operand_valid_for_float_immediate(c->as_jdouble())) {
	__ fmovd(dest->as_double_reg(), (c->as_jdouble()));
      } else {
	__ adr(rscratch1, InternalAddress(double_constant(c->as_jdouble())));
	__ ldrd(dest->as_double_reg(), Address(rscratch1));
      }
      break;
    }

    default:
      ShouldNotReachHere();
  }
}

void LIR_Assembler::const2stack(LIR_Opr src, LIR_Opr dest) { Unimplemented(); }

void LIR_Assembler::const2mem(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info, bool wide) {
  assert(src->is_constant(), "should not call otherwise");
  LIR_Const* c = src->as_constant_ptr();
  LIR_Address* to_addr = dest->as_address_ptr();

  switch (c->type()) {
  case T_ADDRESS:
    {
    assert(c->as_jint() == 0, "should be");
    __ str(zr, as_Address(to_addr, rscratch1));
    break;
  }
  case T_OBJECT:
    {
    assert(c->as_jobject() == 0, "should be");
    __ str(zr, as_Address(to_addr, rscratch1));
    break;
  }
  default:
    ShouldNotReachHere();
  }
}

void LIR_Assembler::reg2reg(LIR_Opr src, LIR_Opr dest) {
  assert(src->is_register(), "should not call otherwise");
  assert(dest->is_register(), "should not call otherwise");

  // move between cpu-registers
  if (dest->is_single_cpu()) {
    if (src->type() == T_LONG) {
      // Can do LONG -> OBJECT
      move_regs(src->as_register_lo(), dest->as_register());
      return;
    }
    assert(src->is_single_cpu(), "must match");
    if (src->type() == T_OBJECT) {
      __ verify_oop(src->as_register());
    }
    move_regs(src->as_register(), dest->as_register());

  } else if (dest->is_double_cpu()) {
    if (src->type() == T_OBJECT || src->type() == T_ARRAY) {
      // Surprising to me but we can see move of a long to t_object
      __ verify_oop(src->as_register());
      move_regs(src->as_register(), dest->as_register_lo());
      return;
    }
    assert(src->is_double_cpu(), "must match");
    Register f_lo = src->as_register_lo();
    Register f_hi = src->as_register_hi();
    Register t_lo = dest->as_register_lo();
    Register t_hi = dest->as_register_hi();
    assert(f_hi == f_lo, "must be same");
    assert(t_hi == t_lo, "must be same");
    move_regs(f_lo, t_lo);

  } else if (dest->is_single_fpu()) {
    __ fmovs(dest->as_float_reg(), src->as_float_reg());

  } else if (dest->is_double_fpu()) {
    __ fmovd(src->as_double_reg(), src->as_double_reg());

  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::reg2stack(LIR_Opr src, LIR_Opr dest, BasicType type, bool pop_fpu_stack) {
    if (src->is_single_cpu()) {
    if (type == T_ARRAY || type == T_OBJECT) {
      __ str(src->as_register(), frame_map()->address_for_slot(dest->single_stack_ix()));
      __ verify_oop(src->as_register());
    } else if (type == T_METADATA) {
      __ str(src->as_register(), frame_map()->address_for_slot(dest->single_stack_ix()));
    } else {
      __ strw(src->as_register(), frame_map()->address_for_slot(dest->single_stack_ix()));
    }

  } else if (src->is_double_cpu()) {
    Address dest_addr_LO = frame_map()->address_for_slot(dest->double_stack_ix(), lo_word_offset_in_bytes);
    __ str(src->as_register_lo(), dest_addr_LO);

  } else if (src->is_single_fpu()) {
    Address dest_addr = frame_map()->address_for_slot(dest->single_stack_ix());
    __ strs(src->as_float_reg(), dest_addr);

  } else if (src->is_double_fpu()) {
    Address dest_addr = frame_map()->address_for_slot(dest->double_stack_ix());
    __ strd(src->as_double_reg(), dest_addr);

  } else {
    ShouldNotReachHere();
  }

}


void LIR_Assembler::reg2mem(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code, CodeEmitInfo* info, bool pop_fpu_stack, bool wide, bool /* unaligned */) {
  LIR_Address* to_addr = dest->as_address_ptr();
  PatchingStub* patch = NULL;
  Register compressed_src = rscratch1;

  if (type == T_ARRAY || type == T_OBJECT) {
    __ verify_oop(src->as_register());
#ifdef _LP64
    if (UseCompressedOops && !wide) {
      __ mov(compressed_src, src->as_register());
      __ encode_heap_oop(compressed_src);
    }
#endif
  }

  if (patch_code != lir_patch_none) {
    patch = new PatchingStub(_masm, PatchingStub::access_field_id);
  }

  int null_check_here = code_offset();
  switch (type) {
    case T_FLOAT: {
      __ strs(src->as_float_reg(), as_Address(to_addr));
      break;
    }

    case T_DOUBLE: {
      __ strd(src->as_float_reg(), as_Address(to_addr));
    }

    case T_ARRAY:   // fall through
    case T_OBJECT:  // fall through
      if (UseCompressedOops && !wide) {
        __ strw(compressed_src, as_Address(to_addr));
      } else {
         __ str(compressed_src, as_Address(to_addr));
      }
      break;
    case T_METADATA:
      // We get here to store a method pointer to the stack to pass to
      // a dtrace runtime call. This can't work on 64 bit with
      // compressed klass ptrs: T_METADATA can be a compressed klass
      // ptr or a 64 bit method pointer.
      LP64_ONLY(ShouldNotReachHere());
      __ str(src->as_register(), as_Address(to_addr));
      break;
    case T_ADDRESS:
      __ str(src->as_register(), as_Address(to_addr));
      break;
    case T_INT:
      __ strw(src->as_register(), as_Address(to_addr));
      break;

    case T_LONG: {
      __ str(src->as_register_lo(), as_Address_lo(to_addr));
      break;
    }

    case T_BYTE:    // fall through
    case T_BOOLEAN: {
      __ strb(src->as_register(), as_Address(to_addr));
      break;
    }

    case T_CHAR:    // fall through
    case T_SHORT:
      __ strw(src->as_register(), as_Address(to_addr));
      break;

    default:
      ShouldNotReachHere();
  }
  if (info != NULL) {
    add_debug_info_for_null_check(null_check_here, info);
  }

  if (patch_code != lir_patch_none) {
    patching_epilog(patch, patch_code, to_addr->base()->as_register(), info);
  }
}


void LIR_Assembler::stack2reg(LIR_Opr src, LIR_Opr dest, BasicType type) {
  assert(src->is_stack(), "should not call otherwise");
  assert(dest->is_register(), "should not call otherwise");

  if (dest->is_single_cpu()) {
    if (type == T_ARRAY || type == T_OBJECT) {
      __ ldr(dest->as_register(), frame_map()->address_for_slot(src->single_stack_ix()));
      __ verify_oop(dest->as_register());
    } else if (type == T_METADATA) {
      __ ldr(dest->as_register(), frame_map()->address_for_slot(src->single_stack_ix()));
    } else {
      __ ldrw(dest->as_register(), frame_map()->address_for_slot(src->single_stack_ix()));
    }

  } else if (dest->is_double_cpu()) {
    Address src_addr_LO = frame_map()->address_for_slot(src->double_stack_ix(), lo_word_offset_in_bytes);
    __ ldr(dest->as_register_lo(), src_addr_LO);

  } else if (dest->is_single_fpu()) {
    Address src_addr = frame_map()->address_for_slot(src->single_stack_ix());
    __ ldrs(dest->as_float_reg(), src_addr);

  } else if (dest->is_double_fpu()) {
    Address src_addr = frame_map()->address_for_slot(src->double_stack_ix());
    __ ldrd(dest->as_double_reg(), src_addr);

  } else {
    ShouldNotReachHere();
  }
}


void LIR_Assembler::klass2reg_with_patching(Register reg, CodeEmitInfo* info) {
  Metadata* o = NULL;
  PatchingStub* patch = new PatchingStub(_masm, PatchingStub::load_klass_id);
  __ mov_metadata(reg, o);
  patching_epilog(patch, lir_patch_normal, reg, info);
}

void LIR_Assembler::stack2stack(LIR_Opr src, LIR_Opr dest, BasicType type) { Unimplemented(); }


void LIR_Assembler::mem2reg(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code, CodeEmitInfo* info, bool wide, bool /* unaligned */) {
  LIR_Address* addr = src->as_address_ptr();

  LIR_Address* from_addr = src->as_address_ptr();
  PatchingStub* patch = NULL;

  if (patch_code != lir_patch_none) {
    patch = new PatchingStub(_masm, PatchingStub::access_field_id);
  }

  if (info != NULL) {
    add_debug_info_for_null_check_here(info);
  }
  int null_check_here = code_offset();
  switch (type) {
    case T_FLOAT: {
      __ ldrs(dest->as_float_reg(), as_Address(from_addr));
      break;
    }

    case T_DOUBLE: {
      __ ldrd(dest->as_float_reg(), as_Address(from_addr));
    }

    case T_ARRAY:   // fall through
    case T_OBJECT:  // fall through
      if (UseCompressedOops && !wide) {
        __ ldrw(dest->as_register(), as_Address(from_addr));
      } else {
         __ ldr(dest->as_register(), as_Address(from_addr));
      }
      break;
    case T_METADATA:
      // We get here to store a method pointer to the stack to pass to
      // a dtrace runtime call. This can't work on 64 bit with
      // compressed klass ptrs: T_METADATA can be a compressed klass
      // ptr or a 64 bit method pointer.
      LP64_ONLY(ShouldNotReachHere());
      __ ldr(dest->as_register(), as_Address(from_addr));
      break;
    case T_ADDRESS:
      __ ldr(dest->as_register(), as_Address(from_addr));
      break;
    case T_INT:
      __ ldrw(dest->as_register(), as_Address(from_addr));
      break;

    case T_LONG: {
      __ ldr(dest->as_register_lo(), as_Address_lo(from_addr));
      break;
    }

    case T_BYTE:    // fall through
    case T_BOOLEAN: {
      __ ldrb(dest->as_register(), as_Address(from_addr));
      break;
    }

    case T_CHAR:    // fall through
    case T_SHORT:
      __ ldrw(dest->as_register(), as_Address(from_addr));
      break;

    default:
      ShouldNotReachHere();
  }

  if (patch != NULL) {
    patching_epilog(patch, patch_code, addr->base()->as_register(), info);
  }

  if (type == T_ARRAY || type == T_OBJECT) {
#ifdef _LP64
    if (UseCompressedOops && !wide) {
      __ decode_heap_oop(dest->as_register());
    }
#endif
    __ verify_oop(dest->as_register());
  } else if (type == T_ADDRESS && addr->disp() == oopDesc::klass_offset_in_bytes()) {
#ifdef _LP64
    if (UseCompressedKlassPointers) {
      __ decode_klass_not_null(dest->as_register());
    }
#endif
  }
}


void LIR_Assembler::prefetchr(LIR_Opr src) { Unimplemented(); }


void LIR_Assembler::prefetchw(LIR_Opr src) { Unimplemented(); }


int LIR_Assembler::array_element_size(BasicType type) const {
  int elem_size = type2aelembytes(type);
  return exact_log2(elem_size);
}

void LIR_Assembler::emit_op3(LIR_Op3* op) {
  Register Rdividend = op->in_opr1()->as_register();
  Register Rdivisor  = op->in_opr2()->as_register();
  Register Rscratch  = op->in_opr3()->as_register();
  Register Rresult   = op->result_opr()->as_register();
  int divisor = -1;

  /*
  TODO: For some reason, using the Rscratch that gets passed in is
  not possible because the register allocator does not see the tmp reg
  as used, and assignes it the same register as Rdividend. We use rscratch1
   instead.

  assert(Rdividend != Rscratch, "");
  assert(Rdivisor  != Rscratch, "");
  */

  if (Rdivisor == noreg && is_power_of_2(divisor)) {
    // convert division by a power of two into some shifts and logical operations
  }

  if (op->code() == lir_irem) {
    __ corrected_idivl(Rresult, Rdividend, Rdivisor, true, rscratch1);
   } else if (op->code() == lir_idiv) {
    __ corrected_idivl(Rresult, Rdividend, Rdivisor, false, rscratch1);
  } else
    ShouldNotReachHere();
}

void LIR_Assembler::emit_opBranch(LIR_OpBranch* op) {
#ifdef ASSERT
  assert(op->block() == NULL || op->block()->label() == op->label(), "wrong label");
  if (op->block() != NULL)  _branch_target_blocks.append(op->block());
  if (op->ublock() != NULL) _branch_target_blocks.append(op->ublock());
#endif

  if (op->cond() == lir_cond_always) {
    if (op->info() != NULL) add_debug_info_for_branch(op->info());
    __ b(*(op->label()));
  } else {
    Assembler::Condition acond = Assembler::EQ;
    if (op->code() == lir_cond_float_branch) {
      switch(op->cond()) {
        case lir_cond_equal:        acond = Assembler::EQ; break;
        case lir_cond_notEqual:     acond = Assembler::NE; break;
        case lir_cond_less:         acond = Assembler::LO; break;
        case lir_cond_lessEqual:    acond = Assembler::LS; break;
        case lir_cond_greaterEqual: acond = Assembler::HS; break;
        case lir_cond_greater:      acond = Assembler::HI; break;
        default:                         ShouldNotReachHere();
      }
    } else {
      switch (op->cond()) {
        case lir_cond_equal:        acond = Assembler::EQ; break;
        case lir_cond_notEqual:     acond = Assembler::NE; break;
        case lir_cond_less:         acond = Assembler::LT; break;
        case lir_cond_lessEqual:    acond = Assembler::LE; break;
        case lir_cond_greaterEqual: acond = Assembler::GE; break;
        case lir_cond_greater:      acond = Assembler::GT; break;
        case lir_cond_belowEqual:   acond = Assembler::LS; break;
        case lir_cond_aboveEqual:   acond = Assembler::HS; break;
        default:                         ShouldNotReachHere();
      }
    }
    __ br(acond,*(op->label()));
  }
}



void LIR_Assembler::emit_opConvert(LIR_OpConvert* op) {
  LIR_Opr src  = op->in_opr();
  LIR_Opr dest = op->result_opr();

  switch (op->bytecode()) {
    case Bytecodes::_d2i:
      {
	Register tmp = op->tmp1()->as_register();
	Label L_Okay;
	__ clear_fpsr();
	__ fcvtzdw(dest->as_register(), src->as_double_reg());
	__ get_fpsr(tmp);
	__ cbnzw(tmp, *(op->stub()->entry()));
	__ bind(*op->stub()->continuation());
	break;
      }
    case Bytecodes::_i2f:
      {
	__ scvtfws(dest->as_float_reg(), src->as_register());
	break;
      }
    case Bytecodes::_i2d:
      {
	__ scvtfwd(dest->as_double_reg(), src->as_register());
	break;
      }
    case Bytecodes::_l2d:
      {
	__ scvtfd(dest->as_double_reg(), src->as_register_lo());
	break;
      }
    case Bytecodes::_f2d:
      {
	__ fcvts(dest->as_double_reg(), src->as_float_reg());
	break;
      }
    case Bytecodes::_i2l:
      {
	__ sxtw(dest->as_register_lo(), src->as_register());
	break;
      }

    case Bytecodes::_l2i:
      {
	_masm->block_comment("FIXME: This could be a no-op");
	__ uxtw(dest->as_register(), src->as_register_lo());
	break;
      }

    default: ShouldNotReachHere();
  }
}

void LIR_Assembler::emit_alloc_obj(LIR_OpAllocObj* op) {
  if (op->init_check()) {
    __ ldrb(rscratch1, Address(op->klass()->as_register(),
			       InstanceKlass::init_state_offset()));
    __ cmpw(rscratch1, InstanceKlass::fully_initialized);
    add_debug_info_for_null_check_here(op->stub()->info());
    __ br(Assembler::NE, *op->stub()->entry());
  }
  __ allocate_object(op->obj()->as_register(),
                     op->tmp1()->as_register(),
                     op->tmp2()->as_register(),
                     op->header_size(),
                     op->object_size(),
                     op->klass()->as_register(),
                     *op->stub()->entry());
  __ bind(*op->stub()->continuation());
}

void LIR_Assembler::emit_alloc_array(LIR_OpAllocArray* op) {
  Register len =  op->len()->as_register();
  __ uxtw(len, len);

  if (UseSlowPath ||
      (!UseFastNewObjectArray && (op->type() == T_OBJECT || op->type() == T_ARRAY)) ||
      (!UseFastNewTypeArray   && (op->type() != T_OBJECT && op->type() != T_ARRAY))) {
    __ b(*op->stub()->entry());
  } else {
    Register tmp1 = op->tmp1()->as_register();
    Register tmp2 = op->tmp2()->as_register();
    Register tmp3 = op->tmp3()->as_register();
    if (len == tmp1) {
      tmp1 = tmp3;
    } else if (len == tmp2) {
      tmp2 = tmp3;
    } else if (len == tmp3) {
      // everything is ok
    } else {
      __ mov(tmp3, len);
    }
    __ allocate_array(op->obj()->as_register(),
                      len,
                      tmp1,
                      tmp2,
                      arrayOopDesc::header_size(op->type()),
                      array_element_size(op->type()),
                      op->klass()->as_register(),
                      *op->stub()->entry());
  }
  __ bind(*op->stub()->continuation());
}

void LIR_Assembler::type_profile_helper(Register mdo,
                                        ciMethodData *md, ciProfileData *data,
                                        Register recv, Label* update_done) { Unimplemented(); }

void LIR_Assembler::emit_typecheck_helper(LIR_OpTypeCheck *op, Label* success, Label* failure, Label* obj_is_null) { Unimplemented(); }


void LIR_Assembler::emit_opTypeCheck(LIR_OpTypeCheck* op) { Unimplemented(); }


void LIR_Assembler::emit_compare_and_swap(LIR_OpCompareAndSwap* op) { Unimplemented(); }

void LIR_Assembler::cmove(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Opr result, BasicType type) { Unimplemented(); }

void LIR_Assembler::arith_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest, CodeEmitInfo* info, bool pop_fpu_stack) {
  assert(info == NULL, "should never be used, idiv/irem and ldiv/lrem not handled by this method");

  if (left->is_single_cpu()) {
    Register lreg = left->as_register();
    Register dreg = as_reg(dest);

    if (right->is_single_cpu()) {
      // cpu register - cpu register
      Register rreg = right->as_register();
      switch (code) {
      case lir_add: __ addw (dest->as_register(), lreg, rreg); break;
      case lir_sub: __ subw (dest->as_register(), lreg, rreg); break;
      case lir_mul: __ mul (dest->as_register(), lreg, rreg); break;
      default:      ShouldNotReachHere();
      }

    } else if (right->is_constant()) {
      // cpu register - constant
      jint c = right->as_constant_ptr()->as_jint();
      switch (code) {
        case lir_add: __ add(dreg, lreg, c); break;
        case lir_sub: __ sub(dreg, lreg, c); break;
        default: ShouldNotReachHere();
      }

    } else {
      ShouldNotReachHere();
    }

  } else if (left->is_double_cpu()) {
    Register lreg_lo = left->as_register_lo();
    Register lreg_hi = left->as_register_hi();

    if (right->is_double_cpu()) {
      // cpu register - cpu register
      Register rreg_lo = right->as_register_lo();
      Register rreg_hi = right->as_register_hi();
      assert_different_registers(lreg_lo, rreg_lo);
      switch (code) {
      case lir_add: __ add (dest->as_register_lo(), lreg_lo, rreg_lo); break;
      case lir_sub: __ sub (dest->as_register_lo(), lreg_lo, rreg_lo); break;
      case lir_mul: __ mul (dest->as_register_lo(), lreg_lo, rreg_lo); break;
      case lir_div: __ corrected_idivq(dest->as_register_lo(), lreg_lo, rreg_lo, false, rscratch1); break;
      case lir_rem: __ corrected_idivq(dest->as_register_lo(), lreg_lo, rreg_lo, true, rscratch1); break;
      default:
	ShouldNotReachHere();
      }

    } else if (right->is_constant()) {
      jlong c = right->as_constant_ptr()->as_jlong_bits();
      switch (code) {
        case lir_add: __ add(dest->as_register_lo(), lreg_lo, c); break;
        case lir_sub: __ sub(dest->as_register_lo(), lreg_lo, c); break;
        default:
          ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }
  } else if (left->is_single_fpu()) {
    assert(right->is_single_fpu(), "right hand side of float arithmetics needs to be float register");
    switch (code) {
    case lir_add: __ fadds (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
    case lir_sub: __ fsubs (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
    case lir_mul: __ fmuls (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
    case lir_div: __ fdivs (dest->as_float_reg(), left->as_float_reg(), right->as_float_reg()); break;
    default:
      ShouldNotReachHere();
    }
  } else if (left->is_double_fpu()) {
    if (right->is_double_fpu()) {
      // cpu register - cpu register
      switch (code) {
      case lir_add: __ faddd (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
      case lir_sub: __ fsubd (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
      case lir_mul: __ fmuld (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
      case lir_div: __ fdivd (dest->as_double_reg(), left->as_double_reg(), right->as_double_reg()); break;
      default:
	ShouldNotReachHere();
      }
    } else {
      if (right->is_constant()) {
	ShouldNotReachHere();
      }
      ShouldNotReachHere();
    }
  } else if (left->is_single_stack() || left->is_address()) {
    assert(left == dest, "left and dest must be equal");
    ShouldNotReachHere();
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::arith_fpu_implementation(LIR_Code code, int left_index, int right_index, int dest_index, bool pop_fpu_stack) { Unimplemented(); }


void LIR_Assembler::intrinsic_op(LIR_Code code, LIR_Opr value, LIR_Opr unused, LIR_Opr dest, LIR_Op* op) { Unimplemented(); }

void LIR_Assembler::logic_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst) {
  
  assert(left->is_single_cpu() || left->is_double_cpu(), "expect single or double register");
  if (left->is_single_cpu()) {
    assert (right->is_single_cpu() || right->is_constant(), "single register or constant expected");
    if (right->is_constant()
	&& Assembler::operand_valid_for_logical_immediate(true, right->as_jint())) {

      switch (code) {
      case lir_logic_and: __ andw (dst->as_register(), left->as_register(), right->as_jint()); break;
      case lir_logic_or:  __ orrw (dst->as_register(), left->as_register(), right->as_jint()); break;
      case lir_logic_xor: __ eorw (dst->as_register(), left->as_register(), right->as_jint()); break;
      default: ShouldNotReachHere(); break;
      }
    } else {
      switch (code) {
      case lir_logic_and: __ andw (dst->as_register(), left->as_register(), right->as_register()); break;
      case lir_logic_or:  __ orrw (dst->as_register(), left->as_register(), right->as_register()); break;
      case lir_logic_xor: __ eorw (dst->as_register(), left->as_register(), right->as_register()); break;
      default: ShouldNotReachHere(); break;
      }
    }
  } else {
    assert (right->is_double_cpu() || right->is_constant(), "single register or constant expected");
    if (right->is_double_cpu()) {
      switch (code) {
      case lir_logic_and: __ andr(dst->as_register_lo(), left->as_register_lo(), right->as_register_lo()); break;
      case lir_logic_or:  __ orr (dst->as_register_lo(), left->as_register_lo(), right->as_register_lo()); break;
      case lir_logic_xor: __ eor (dst->as_register_lo(), left->as_register_lo(), right->as_register_lo()); break;
      default:
	ShouldNotReachHere();
	break;
      }
    } else {
      switch (code) {
      case lir_logic_and: __ andr(dst->as_register_lo(), left->as_register_lo(), right->as_jlong()); break;
      case lir_logic_or:  __ orr (dst->as_register_lo(), left->as_register_lo(), right->as_jlong()); break;
      case lir_logic_xor: __ eor (dst->as_register_lo(), left->as_register_lo(), right->as_jlong()); break;
      default:
	ShouldNotReachHere();
	break;
      }
    }
  }
}


// we assume that rax, and rdx can be overwritten
void LIR_Assembler::arithmetic_idiv(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr temp, LIR_Opr result, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::comp_op(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Op2* op) {
  if (opr1->is_single_cpu() || opr1->is_double_cpu()) {
    Register reg1 = as_reg(opr1);
    if (opr2->is_single_cpu()) {
      // cpu register - cpu register
      Register reg2 = opr2->as_register();
      if (opr1->type() == T_OBJECT || opr1->type() == T_ARRAY) {
        __ cmp(reg1, reg2);
      } else {
        assert(opr2->type() != T_OBJECT && opr2->type() != T_ARRAY, "cmp int, oop?");
        __ cmpw(reg1, reg2);
      }
      return;
    }
    if (opr2->is_double_cpu()) {
      // cpu register - cpu register
      Register reg2 = opr2->as_register_lo();
      __ cmpw(reg1, reg2);
      return;
    }

    if (opr2->is_constant()) {
      jlong imm =
	(opr2->type() == T_LONG)
	? opr2->as_constant_ptr()->as_jlong()
	: opr2->as_constant_ptr()->as_jint();
      if (Assembler::operand_valid_for_add_sub_immediate(imm)) {
	if (opr1->is_single_cpu())
	  __ cmpw(reg1, imm);
	else
	  __ cmp(reg1, imm);
	return;
      }
    }
  }
  ShouldNotReachHere();
}

void LIR_Assembler::comp_fl2i(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst, LIR_Op2* op) { Unimplemented(); }


void LIR_Assembler::align_call(LIR_Code code) {  }


void LIR_Assembler::call(LIR_OpJavaCall* op, relocInfo::relocType rtype) {
  __ bl(Address(op->addr(), rtype));
  add_call_info(code_offset(), op->info());
}


void LIR_Assembler::ic_call(LIR_OpJavaCall* op) {
  __ ic_call(op->addr());
  add_call_info(code_offset(), op->info());
}


/* Currently, vtable-dispatch is only enabled for sparc platforms */
void LIR_Assembler::vtable_call(LIR_OpJavaCall* op) {
  ShouldNotReachHere();
}


void LIR_Assembler::emit_static_call_stub() {
  address call_pc = __ pc();
  address stub = __ start_a_stub(call_stub_size);
  if (stub == NULL) {
    bailout("static call stub overflow");
    return;
  }

  int start = __ offset();

  __ relocate(static_stub_Relocation::spec(call_pc));
  __ mov_metadata(rmethod, (Metadata*)NULL);
  __ b(__ pc());

  assert(__ offset() - start <= call_stub_size, "stub too big");
  __ end_a_stub();
}


void LIR_Assembler::throw_op(LIR_Opr exceptionPC, LIR_Opr exceptionOop, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::unwind_op(LIR_Opr exceptionOop) {
  assert(exceptionOop->as_register() == r0, "must match");

  __ b(_unwind_handler_entry);
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, LIR_Opr count, LIR_Opr dest, LIR_Opr tmp) {
  if (left->is_single_cpu()) {
    switch (code) {
    case lir_shl:  __ lslvw (dest->as_register(), left->as_register(), count->as_register()); break;
    case lir_shr:  __ asrvw (dest->as_register(), left->as_register(), count->as_register()); break;
    case lir_ushr: __ lsrvw (dest->as_register(), left->as_register(), count->as_register()); break;
    default:
      ShouldNotReachHere();
      break;
    }
  } else {
    assert (left->is_double_cpu(), "single or double cpu register expected");
    switch (code) {
    case lir_shl:  __ lslv (dest->as_register_lo(), left->as_register_lo(), count->as_register()); break;
    case lir_shr:  __ asrv (dest->as_register_lo(), left->as_register_lo(), count->as_register()); break;
    case lir_ushr: __ lsrv (dest->as_register_lo(), left->as_register_lo(), count->as_register()); break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, jint count, LIR_Opr dest) {

  if (left->is_single_cpu()) {
    switch (code) {
    case lir_shl:  __ lslw (dest->as_register(), left->as_register(), count); break;
    case lir_shr:  __ asrw (dest->as_register(), left->as_register(), count); break;
    case lir_ushr: __ lsrw (dest->as_register(), left->as_register(), count); break;
    default:
      ShouldNotReachHere();
      break;
    }
  } else {
    switch (code) {
    case lir_shl:  __ lsl (dest->as_register_lo(), left->as_register_lo(), count); break;
    case lir_shr:  __ asr (dest->as_register_lo(), left->as_register_lo(), count); break;
    case lir_ushr: __ lsr (dest->as_register_lo(), left->as_register_lo(), count); break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}


void LIR_Assembler::store_parameter(Register r, int offset_from_rsp_in_words) {
  ShouldNotReachHere();
  assert(offset_from_rsp_in_words >= 0, "invalid offset from rsp");
  int offset_from_rsp_in_bytes = offset_from_rsp_in_words * BytesPerWord;
  assert(offset_from_rsp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ str (r, Address(sp, offset_from_rsp_in_bytes));
}


void LIR_Assembler::store_parameter(jint c,     int offset_from_rsp_in_words) {
  ShouldNotReachHere();
  assert(offset_from_rsp_in_words >= 0, "invalid offset from rsp");
  int offset_from_rsp_in_bytes = offset_from_rsp_in_words * BytesPerWord;
  assert(offset_from_rsp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ mov (rscratch1, c);
  __ str (rscratch1, Address(sp, offset_from_rsp_in_bytes));
}


void LIR_Assembler::store_parameter(jobject o,  int offset_from_rsp_in_words) {
  ShouldNotReachHere();
  assert(offset_from_rsp_in_words >= 0, "invalid offset from rsp");
  int offset_from_rsp_in_bytes = offset_from_rsp_in_words * BytesPerWord;
  assert(offset_from_rsp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ lea(rscratch1, __ constant_oop_address(o));
  __ str(rscratch1, Address(sp, offset_from_rsp_in_bytes));
}


// This code replaces a call to arraycopy; no exception may
// be thrown in this code, they must be thrown in the System.arraycopy
// activation frame; we could save some checks if this would not be the case
void LIR_Assembler::emit_arraycopy(LIR_OpArrayCopy* op) { Unimplemented(); }


void LIR_Assembler::emit_lock(LIR_OpLock* op) { Unimplemented(); }


void LIR_Assembler::emit_profile_call(LIR_OpProfileCall* op) { Unimplemented(); }

void LIR_Assembler::emit_delay(LIR_OpDelay*) {
  Unimplemented();
}


void LIR_Assembler::monitor_address(int monitor_no, LIR_Opr dst) { Unimplemented(); }


void LIR_Assembler::align_backward_branch_target() {
}


void LIR_Assembler::negate(LIR_Opr left, LIR_Opr dest) {
  if (left->is_single_cpu()) {
    assert(dest->is_single_cpu(), "expect single result reg");
    __ negw(dest->as_register(), left->as_register());
  } else if (left->is_double_cpu()) {
    assert(dest->is_double_cpu(), "expect double result reg");
    __ neg(dest->as_register_lo(), left->as_register_lo());
  } else if (left->is_single_fpu()) {
    assert(dest->is_single_fpu(), "expect single float result reg");
    __ fnegs(dest->as_float_reg(), left->as_float_reg());
  } else {
    assert(left->is_double_fpu(), "expect double float operand reg");
    assert(dest->is_double_fpu(), "expect double float result reg");
    __ fnegd(dest->as_double_reg(), left->as_double_reg());
  }
}


void LIR_Assembler::leal(LIR_Opr addr, LIR_Opr dest) { Unimplemented(); }


void LIR_Assembler::rt_call(LIR_Opr result, address dest, const LIR_OprList* args, LIR_Opr tmp, CodeEmitInfo* info) {
  assert(!tmp->is_valid(), "don't need temporary");
  __ mov(rscratch1, RuntimeAddress(dest));
  int len = args->length();
  int type;
  switch (result->type()) {
  case T_VOID:
    type = 0;
    break;
  case T_INT:
  case T_LONG:
  case T_OBJECT:
    type = 1;
    break;
  case T_FLOAT:
    type = 2;
    break;
  case T_DOUBLE:
    type = 3;
    break;
  default:
    ShouldNotReachHere();
    break;
  }
  int num_gpargs = 0;
  int num_fpargs = 0;
  for (int i = 0; i < args->length(); i++) {
    LIR_Opr arg = args->at(i);
    if (arg->type() == T_FLOAT || arg->type() == T_DOUBLE) {
      num_fpargs++;
    } else {
      num_gpargs++;
    }
  }
  __ brx86(rscratch1, num_gpargs, num_fpargs, type);
  if (info != NULL) {
    add_call_info_here(info);
  }
}

void LIR_Assembler::volatile_move_op(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::membar() { Unimplemented(); }

void LIR_Assembler::membar_acquire() { Unimplemented(); }

void LIR_Assembler::membar_release() { Unimplemented(); }

void LIR_Assembler::membar_loadload() { Unimplemented(); }

void LIR_Assembler::membar_storestore() { Unimplemented(); }

void LIR_Assembler::membar_loadstore() { Unimplemented(); }

void LIR_Assembler::membar_storeload() { Unimplemented(); }

void LIR_Assembler::get_thread(LIR_Opr result_reg) { Unimplemented(); }


void LIR_Assembler::peephole(LIR_List*) {
}

void LIR_Assembler::atomic_op(LIR_Code code, LIR_Opr src, LIR_Opr data, LIR_Opr dest, LIR_Opr tmp) { Unimplemented(); }

#undef __
