/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates.
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



#ifndef PRODUCT
#define COMMENT(x)   do { __ block_comment(x); } while (0)
#else
#define COMMENT(x)
#endif

NEEDS_CLEANUP // remove this definitions ?
const Register IC_Klass    = rscratch2;   // where the IC klass is cached
const Register SYNC_header = r0;   // synchronization header
const Register SHIFT_count = r0;   // where count for shift operations must be

#define __ _masm->


static void select_different_registers(Register preserve,
                                       Register extra,
                                       Register &tmp1,
                                       Register &tmp2) {
  if (tmp1 == preserve) {
    assert_different_registers(tmp1, tmp2, extra);
    tmp1 = extra;
  } else if (tmp2 == preserve) {
    assert_different_registers(tmp1, tmp2, extra);
    tmp2 = extra;
  }
  assert_different_registers(preserve, tmp1, tmp2);
}



static void select_different_registers(Register preserve,
                                       Register extra,
                                       Register &tmp1,
                                       Register &tmp2,
                                       Register &tmp3) {
  if (tmp1 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp1 = extra;
  } else if (tmp2 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp2 = extra;
  } else if (tmp3 == preserve) {
    assert_different_registers(tmp1, tmp2, tmp3, extra);
    tmp3 = extra;
  }
  assert_different_registers(preserve, tmp1, tmp2, tmp3);
}


bool LIR_Assembler::is_small_constant(LIR_Opr opr) { Unimplemented(); return false; }


LIR_Opr LIR_Assembler::receiverOpr() {
  return FrameMap::receiver_opr;
}

LIR_Opr LIR_Assembler::osrBufferPointer() {
  return FrameMap::as_pointer_opr(receiverOpr()->as_register());
}

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

address LIR_Assembler::int_constant(jlong n) {
  address const_addr = __ long_constant(n);
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

static jlong as_long(LIR_Opr data) {
  jlong result;
  switch (data->type()) {
  case T_INT:
    result = (data->as_jint());
    break;
  case T_LONG:
    result = (data->as_jlong());
    break;
  default:
    ShouldNotReachHere();
  }
  return result;
}

static bool is_reg(LIR_Opr op) {
  return op->is_double_cpu() | op->is_single_cpu();
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
    switch(opr->type()) {
      case T_INT:
	return Address(base, index, Address::sxtw(addr->scale()));
      case T_LONG:
	return Address(base, index, Address::lsl(addr->scale()));
      default:
	ShouldNotReachHere();
      }
  } else  {
    intptr_t addr_offset = intptr_t(addr->disp());
    if (Address::offset_ok_for_immed(addr_offset, addr->scale()))
      return Address(base, addr_offset, Address::lsl(addr->scale()));
    else {
      address const_addr = int_constant(addr_offset);
      __ ldr_constant(tmp, const_addr);
      return Address(base, tmp, Address::lsl(addr->scale()));
    }
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


void LIR_Assembler::osr_entry() {
  offsets()->set_value(CodeOffsets::OSR_Entry, code_offset());
  BlockBegin* osr_entry = compilation()->hir()->osr_entry();
  ValueStack* entry_state = osr_entry->state();
  int number_of_locks = entry_state->locks_size();

  // we jump here if osr happens with the interpreter
  // state set up to continue at the beginning of the
  // loop that triggered osr - in particular, we have
  // the following registers setup:
  //
  // r2: osr buffer
  //

  // build frame
  ciMethod* m = compilation()->method();
  __ build_frame(initial_frame_size_in_bytes());

  // OSR buffer is
  //
  // locals[nlocals-1..0]
  // monitors[0..number_of_locks]
  //
  // locals is a direct copy of the interpreter frame so in the osr buffer
  // so first slot in the local array is the last local from the interpreter
  // and last slot is local[0] (receiver) from the interpreter
  //
  // Similarly with locks. The first lock slot in the osr buffer is the nth lock
  // from the interpreter frame, the nth lock slot in the osr buffer is 0th lock
  // in the interpreter frame (the method lock if a sync method)

  // Initialize monitors in the compiled activation.
  //   r2: pointer to osr buffer
  //
  // All other registers are dead at this point and the locals will be
  // copied into place by code emitted in the IR.

  Register OSR_buf = osrBufferPointer()->as_pointer_register();
  { assert(frame::interpreter_frame_monitor_size() == BasicObjectLock::size(), "adjust code below");
    int monitor_offset = BytesPerWord * method()->max_locals() +
      (2 * BytesPerWord) * (number_of_locks - 1);
    // SharedRuntime::OSR_migration_begin() packs BasicObjectLocks in
    // the OSR buffer using 2 word entries: first the lock and then
    // the oop.
    for (int i = 0; i < number_of_locks; i++) {
      int slot_offset = monitor_offset - ((i * 2) * BytesPerWord);
#ifdef ASSERT
      // verify the interpreter's monitor has a non-null object
      {
        Label L;
        __ ldr(rscratch1, Address(OSR_buf, slot_offset + 1*BytesPerWord));
        __ cbnz(rscratch1, L);
        __ stop("locked object is NULL");
        __ bind(L);
      }
#endif
      __ ldr(r19, Address(OSR_buf, slot_offset + 0));
      __ str(r19, frame_map()->address_for_monitor_lock(i));
      __ ldr(r19, Address(OSR_buf, slot_offset + 1*BytesPerWord));
      __ str(r19, frame_map()->address_for_monitor_object(i));
    }
  }
}


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
    address const_ptr = int_constant(jlong(o));
    __ code()->consts()->relocate(const_ptr, rspec);
    __ ldr_constant(reg, const_ptr);

    if (PrintRelocations && Verbose) {
	puts("jobject2reg:\n");
	printf("oop %p  at %p\n", o, const_ptr);
	fflush(stdout);
	das((uint64_t)__ pc(), -2);
    }
  }
}


void LIR_Assembler::jobject2reg_with_patching(Register reg, CodeEmitInfo *info) {
  // Allocate a new index in table to hold the object once it's been patched
  int oop_index = __ oop_recorder()->allocate_oop_index(NULL);
  PatchingStub* patch = new PatchingStub(_masm, PatchingStub::load_mirror_id, oop_index);

  RelocationHolder rspec = oop_Relocation::spec(oop_index);
  address const_ptr = int_constant(-1);
  __ code()->consts()->relocate(const_ptr, rspec);
  __ ldr_constant(reg, const_ptr);
  patching_epilog(patch, lir_patch_normal, reg, info);
}


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
  __ block_comment("remove_frame and dispatch to the unwind handler");
  __ remove_frame(initial_frame_size_in_bytes());
  __ b(RuntimeAddress(Runtime1::entry_for(Runtime1::unwind_exception_id)));

  // Emit the slow path assembly
  if (stub != NULL) {
    stub->emit_code(this);
  }

  return offset;
}


int LIR_Assembler::emit_deopt_handler() {
  // if the last instruction is a call (typically to do a throw which
  // is coming at the end after block reordering) the return address
  // must still point into the code area in order to avoid assertion
  // failures when searching for the corresponding bci => add a nop
  // (was bug 5/14/1999 - gri)
  __ nop();

  // generate code for exception handler
  address handler_base = __ start_a_stub(deopt_handler_size);
  if (handler_base == NULL) {
    // not enough space left for the handler
    bailout("deopt handler overflow");
    return -1;
  }

  int offset = code_offset();

  __ adr(lr, pc());
  __ b(RuntimeAddress(SharedRuntime::deopt_blob()->unpack()));
  guarantee(code_offset() - offset <= deopt_handler_size, "overflow");
  __ end_a_stub();

  return offset;
}


// This is the fast version of java.lang.String.compare; it has not
// OSR-entry and therefore, we generate a slow version for OSR's
void LIR_Assembler::emit_string_compare(LIR_Opr arg0, LIR_Opr arg1, LIR_Opr dst, CodeEmitInfo* info)  {
  __ mov(r2, (address)__FUNCTION__);
  __ call_Unimplemented();
}


void LIR_Assembler::add_debug_info_for_branch(address adr, CodeEmitInfo* info) {
  _masm->code_section()->relocate(adr, relocInfo::poll_type);
  int pc_offset = code_offset();
  flush_debug_info(pc_offset);
  info->record_debug_info(compilation()->debug_info_recorder(), pc_offset);
  if (info->exception_handlers() != NULL) {
    compilation()->add_exception_handlers_for_pco(pc_offset, info->exception_handlers());
  }
}

// Rather than take a segfault when the polling page is protected,
// explicitly check for a safepoint in progress and if there is one,
// fake a call to the handler as if a segfault had been caught.
void LIR_Assembler::poll_for_safepoint(relocInfo::relocType rtype, CodeEmitInfo* info) {
  __ mov(rscratch1, SafepointSynchronize::address_of_state());
  __ ldrb(rscratch1, Address(rscratch1));
  Label nope, poll;
  __ cbz(rscratch1, nope);
  __ block_comment("safepoint");
  __ enter();
  __ push(0x3, sp);                // r0 & r1
  __ push(0x3ffffffc, sp);         // integer registers except lr & sp & r0 & r1
  __ adr(r0, poll);
  __ str(r0, Address(rthread, JavaThread::saved_exception_pc_offset()));
  __ mov(rscratch1, CAST_FROM_FN_PTR(address, SharedRuntime::get_poll_stub));
  __ blrt(rscratch1, 1, 0, 1);
  __ pop(0x3ffffffc, sp);          // integer registers except lr & sp & r0 & r1
  __ mov(rscratch1, r0);
  __ pop(0x3, sp);                 // r0 & r1
  __ leave();
  __ br(rscratch1);
  address polling_page(os::get_polling_page() + (SafepointPollOffset % os::vm_page_size()));
  assert(os::is_poll_address(polling_page), "should be");
  unsigned long off;
  __ adrp(rscratch1, Address(polling_page, rtype), off);
  __ bind(poll);
  if (info)
    add_debug_info_for_branch(info);  // This isn't just debug info:
                                      // it's the oop map
  else
    __ code_section()->relocate(pc(), rtype);
  __ ldrw(zr, Address(rscratch1, off));
  __ bind(nope);
}

void LIR_Assembler::return_op(LIR_Opr result) {
  assert(result->is_illegal() || !result->is_single_cpu() || result->as_register() == r0, "word returns are in r0,");
  // Pop the stack before the safepoint code
  __ remove_frame(initial_frame_size_in_bytes());
  if (UseCompilerSafepoints) {
    address polling_page(os::get_polling_page() + (SafepointPollOffset % os::vm_page_size()));
    __ read_polling_page(rscratch1, polling_page, relocInfo::poll_return_type);
  } else {
    poll_for_safepoint(relocInfo::poll_return_type);
  }
  __ ret(lr);
}

int LIR_Assembler::safepoint_poll(LIR_Opr tmp, CodeEmitInfo* info) {
  address polling_page(os::get_polling_page()
		       + (SafepointPollOffset % os::vm_page_size()));
  if (UseCompilerSafepoints) {
    guarantee(info != NULL, "Shouldn't be NULL");
    assert(os::is_poll_address(polling_page), "should be");
    unsigned long off;
    __ adrp(rscratch1, Address(polling_page, relocInfo::poll_type), off);
    add_debug_info_for_branch(info);  // This isn't just debug info:
                                      // it's the oop map
    __ ldrw(zr, Address(rscratch1, off));
  } else {
    poll_for_safepoint(relocInfo::poll_type, info);
  }

  return __ offset();
}


void LIR_Assembler::move_regs(Register from_reg, Register to_reg) {
  if (from_reg == r31_sp)
    from_reg = sp;
  if (to_reg == r31_sp)
    to_reg = sp;
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
      __ movw(dest->as_register(), c->as_jint());
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

void LIR_Assembler::const2stack(LIR_Opr src, LIR_Opr dest) {
  LIR_Const* c = src->as_constant_ptr();
  switch (c->type()) {
  case T_OBJECT:
    {
      if (! c->as_jobject())
	__ str(zr, frame_map()->address_for_slot(dest->single_stack_ix()));
      else {
	const2reg(src, FrameMap::rscratch1_opr, lir_patch_none, NULL);
	reg2stack(FrameMap::rscratch1_opr, dest, c->type(), false);
      }
    }
    break;
  case T_INT:
  case T_FLOAT:
    {
      Register reg = zr;
      if (c->as_jint_bits() == 0)
	__ strw(zr, frame_map()->address_for_slot(dest->single_stack_ix()));
      else {
	__ movw(rscratch1, c->as_jint_bits());
	__ strw(rscratch1, frame_map()->address_for_slot(dest->single_stack_ix()));
      }
    }
    break;
  case T_LONG:
  case T_DOUBLE:
    {
      Register reg = zr;
      if (c->as_jlong_bits() == 0)
	__ str(zr, frame_map()->address_for_slot(dest->double_stack_ix(),
						 lo_word_offset_in_bytes));
      else {
	__ mov(rscratch1, (intptr_t)c->as_jlong_bits());
	__ str(rscratch1, frame_map()->address_for_slot(dest->double_stack_ix(),
							lo_word_offset_in_bytes));
      }
    }
    break;
  default:
    ShouldNotReachHere();
  }
}

void LIR_Assembler::const2mem(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info, bool wide) {
  assert(src->is_constant(), "should not call otherwise");
  LIR_Const* c = src->as_constant_ptr();
  LIR_Address* to_addr = dest->as_address_ptr();

  void (Assembler::* insn)(Register Rt, const Address &adr);

  switch (type) {
  case T_ADDRESS:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::str;
    break;
  case T_LONG:
    assert(c->as_jlong() == 0, "should be");
    insn = &Assembler::str;
    break;
  case T_INT:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::strw;
    break;
  case T_OBJECT:
  case T_ARRAY:
    assert(c->as_jobject() == 0, "should be");
    if (UseCompressedOops && !wide) {
      insn = &Assembler::strw;
    } else {
      insn = &Assembler::str;
    }
    break;
  case T_CHAR:
  case T_SHORT:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::strh;
    break;
  case T_BOOLEAN:
  case T_BYTE:
    assert(c->as_jint() == 0, "should be");
    insn = &Assembler::strb;
    break;
  default:
    ShouldNotReachHere();
  }

  if (info) add_debug_info_for_null_check_here(info);
  (_masm->*insn)(zr, as_Address(to_addr, rscratch1));
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
    __ fmovd(dest->as_double_reg(), src->as_double_reg());

  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::reg2stack(LIR_Opr src, LIR_Opr dest, BasicType type, bool pop_fpu_stack) {
  if (src->is_single_cpu()) {
    if (type == T_ARRAY || type == T_OBJECT) {
      __ str(src->as_register(), frame_map()->address_for_slot(dest->single_stack_ix()));
      __ verify_oop(src->as_register());
    } else if (type == T_METADATA || type == T_DOUBLE) {
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

    if (UseCompressedOops && !wide) {
      __ encode_heap_oop(compressed_src, src->as_register());
    } else {
      compressed_src = src->as_register();
    }
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
      __ strd(src->as_double_reg(), as_Address(to_addr));
      break;
    }

    case T_ARRAY:   // fall through
    case T_OBJECT:  // fall through
      if (UseCompressedOops && !wide) {
        __ strw(compressed_src, as_Address(to_addr, rscratch2));
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
      __ strh(src->as_register(), as_Address(to_addr));
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
    } else if (type == T_METADATA || type == T_DOUBLE) {
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

void LIR_Assembler::stack2stack(LIR_Opr src, LIR_Opr dest, BasicType type) {
  LIR_Opr temp;
  if (type == T_LONG)
    temp = FrameMap::rscratch1_long_opr;
  else
    temp = FrameMap::rscratch1_opr;

  stack2reg(src, temp, src->type());
  reg2stack(temp, dest, dest->type(), false);
}


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
      __ ldrd(dest->as_double_reg(), as_Address(from_addr));
      break;
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
      // FIXME: OMG this is a horrible kludge.  Any offset from an
      // address that matches klass_offset_in_bytes() will be loaded
      // as a word, not a long.
      if (UseCompressedKlassPointers && addr->disp() == oopDesc::klass_offset_in_bytes()) {
	__ ldrw(dest->as_register(), as_Address(from_addr));
      } else {
	__ ldr(dest->as_register(), as_Address(from_addr));
      }
      break;
    case T_INT:
      __ ldrw(dest->as_register(), as_Address(from_addr));
      break;

    case T_LONG: {
      __ ldr(dest->as_register_lo(), as_Address_lo(from_addr));
      break;
    }

    case T_BYTE:
      __ ldrsb(dest->as_register(), as_Address(from_addr));
      break;
    case T_BOOLEAN: {
      __ ldrb(dest->as_register(), as_Address(from_addr));
      break;
    }

    case T_CHAR:
      __ ldrh(dest->as_register(), as_Address(from_addr));
      break;
    case T_SHORT:
      __ ldrsh(dest->as_register(), as_Address(from_addr));
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
    Assembler::Condition acond;
    if (op->code() == lir_cond_float_branch) {
      bool is_unordered = (op->ublock() == op->block());
      // Assembler::EQ does not permit unordered branches, so we add
      // another branch here.  Likewise, Assembler::NE does not permit
      // ordered branches.
      if (is_unordered && op->cond() == lir_cond_equal 
	  || !is_unordered && op->cond() == lir_cond_notEqual)
	__ br(Assembler::VS, *(op->ublock()->label()));
      switch(op->cond()) {
      case lir_cond_equal:        acond = Assembler::EQ; break;
      case lir_cond_notEqual:     acond = Assembler::NE; break;
      case lir_cond_less:         acond = (is_unordered ? Assembler::LT : Assembler::LO); break;
      case lir_cond_lessEqual:    acond = (is_unordered ? Assembler::LE : Assembler::LS); break;
      case lir_cond_greaterEqual: acond = (is_unordered ? Assembler::HS : Assembler::GE); break;
      case lir_cond_greater:      acond = (is_unordered ? Assembler::HI : Assembler::GT); break;
      default:                    ShouldNotReachHere();
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
    case Bytecodes::_l2f:
      {
	__ scvtfs(dest->as_float_reg(), src->as_register_lo());
	break;
      }
    case Bytecodes::_f2d:
      {
	__ fcvts(dest->as_double_reg(), src->as_float_reg());
	break;
      }
    case Bytecodes::_d2f:
      {
	__ fcvtd(dest->as_float_reg(), src->as_double_reg());
	break;
      }
    case Bytecodes::_i2c:
      {
	__ ubfx(dest->as_register(), src->as_register(), 0, 16);
	break;
      }
    case Bytecodes::_i2l:
      {
	__ sxtw(dest->as_register_lo(), src->as_register());
	break;
      }
    case Bytecodes::_i2s:
      {
	__ sxth(dest->as_register(), src->as_register());
	break;
      }
    case Bytecodes::_i2b:
      {
	__ sxtb(dest->as_register(), src->as_register());
	break;
      }
    case Bytecodes::_l2i:
      {
	_masm->block_comment("FIXME: This could be a no-op");
	__ uxtw(dest->as_register(), src->as_register_lo());
	break;
      }
    case Bytecodes::_d2l:
      {
	Register tmp = op->tmp1()->as_register();
	__ clear_fpsr();
	__ fcvtzd(dest->as_register_lo(), src->as_double_reg());
	__ get_fpsr(tmp);
	__ tst(tmp, 1); // FPSCR.IOC
	__ br(Assembler::NE, *(op->stub()->entry()));
	__ bind(*op->stub()->continuation());
	break;
      }
    case Bytecodes::_f2i:
      {
	Register tmp = op->tmp1()->as_register();
	__ clear_fpsr();
	__ fcvtzsw(dest->as_register(), src->as_float_reg());
	__ get_fpsr(tmp);
	__ tst(tmp, 1); // FPSCR.IOC
	__ br(Assembler::NE, *(op->stub()->entry()));
	__ bind(*op->stub()->continuation());
	break;
      }
    case Bytecodes::_f2l:
      {
	Register tmp = op->tmp1()->as_register();
	__ clear_fpsr();
	__ fcvtzs(dest->as_register_lo(), src->as_float_reg());
	__ get_fpsr(tmp);
	__ tst(tmp, 1); // FPSCR.IOC
	__ br(Assembler::NE, *(op->stub()->entry()));
	__ bind(*op->stub()->continuation());
	break;
      }
    case Bytecodes::_d2i:
      {
	Register tmp = op->tmp1()->as_register();
	__ clear_fpsr();
	__ fcvtzdw(dest->as_register(), src->as_double_reg());
	__ get_fpsr(tmp);
	__ tst(tmp, 1); // FPSCR.IOC
	__ br(Assembler::NE, *(op->stub()->entry()));
	__ bind(*op->stub()->continuation());
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
                                        Register recv, Label* update_done) {
  for (uint i = 0; i < ReceiverTypeData::row_limit(); i++) {
    Label next_test;
    // See if the receiver is receiver[n].
    __ ldr(rscratch1, Address(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i))));
    __ cmp(recv, rscratch1);
    __ br(Assembler::NE, next_test);
    Address data_addr(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i)));
    __ addptr(data_addr, DataLayout::counter_increment);
    __ b(*update_done);
    __ bind(next_test);
  }

  // Didn't find receiver; find next empty slot and fill it in
  for (uint i = 0; i < ReceiverTypeData::row_limit(); i++) {
    Label next_test;
    Address recv_addr(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_offset(i)));
    __ ldr(rscratch1, recv_addr);
    __ cbnz(rscratch1, next_test);
    __ str(recv, recv_addr);
    __ mov(rscratch1, DataLayout::counter_increment);
    __ str(rscratch1, Address(mdo, md->byte_offset_of_slot(data, ReceiverTypeData::receiver_count_offset(i))));
    __ b(*update_done);
    __ bind(next_test);
  }
}

void LIR_Assembler::emit_typecheck_helper(LIR_OpTypeCheck *op, Label* success, Label* failure, Label* obj_is_null) {
  // we always need a stub for the failure case.
  CodeStub* stub = op->stub();
  Register obj = op->object()->as_register();
  Register k_RInfo = op->tmp1()->as_register();
  Register klass_RInfo = op->tmp2()->as_register();
  Register dst = op->result_opr()->as_register();
  ciKlass* k = op->klass();
  Register Rtmp1 = noreg;

  // check if it needs to be profiled
  ciMethodData* md;
  ciProfileData* data;

  if (op->should_profile()) {
    ciMethod* method = op->profiled_method();
    assert(method != NULL, "Should have method");
    int bci = op->profiled_bci();
    md = method->method_data_or_null();
    assert(md != NULL, "Sanity");
    data = md->bci_to_data(bci);
    assert(data != NULL,                "need data for type check");
    assert(data->is_ReceiverTypeData(), "need ReceiverTypeData for type check");
  }
  Label profile_cast_success, profile_cast_failure;
  Label *success_target = op->should_profile() ? &profile_cast_success : success;
  Label *failure_target = op->should_profile() ? &profile_cast_failure : failure;

  if (obj == k_RInfo) {
    k_RInfo = dst;
  } else if (obj == klass_RInfo) {
    klass_RInfo = dst;
  }
  if (k->is_loaded() && !UseCompressedKlassPointers) {
    select_different_registers(obj, dst, k_RInfo, klass_RInfo);
  } else {
    Rtmp1 = op->tmp3()->as_register();
    select_different_registers(obj, dst, k_RInfo, klass_RInfo, Rtmp1);
  }

  assert_different_registers(obj, k_RInfo, klass_RInfo);
  if (!k->is_loaded()) {
    klass2reg_with_patching(k_RInfo, op->info_for_patch());
  } else {
#ifdef _LP64
    __ mov_metadata(k_RInfo, k->constant_encoding());
#endif // _LP64
  }
  assert(obj != k_RInfo, "must be different");

    if (op->should_profile()) {
      Label not_null;
      __ cbnz(obj, not_null);
      // Object is null; update MDO and exit
      Register mdo  = klass_RInfo;
      __ mov_metadata(mdo, md->constant_encoding());
      Address data_addr(mdo, md->byte_offset_of_slot(data, DataLayout::header_offset()));
      int header_bits = DataLayout::flag_mask_to_header_mask(BitData::null_seen_byte_constant());
      __ ldrw(rscratch1, data_addr);
      __ orrw(rscratch1, rscratch1, header_bits);
      __ strw(rscratch1, data_addr);
      __ b(*obj_is_null);
      __ bind(not_null);
    } else {
      __ cbz(obj, *obj_is_null);
    }

  __ verify_oop(obj);

  if (op->fast_check()) {
    // get object class
    // not a safepoint as obj null check happens earlier
    __ load_klass(rscratch1, obj);
    __ cmp( rscratch1, k_RInfo);

    __ br(Assembler::NE, *failure_target);
    // successful cast, fall through to profile or jump
  } else {
    // get object class
    // not a safepoint as obj null check happens earlier
    __ load_klass(klass_RInfo, obj);
    if (k->is_loaded()) {
      // See if we get an immediate positive hit
      __ ldr(rscratch1, Address(klass_RInfo, long(k->super_check_offset())));
      __ cmp(k_RInfo, rscratch1);
      if ((juint)in_bytes(Klass::secondary_super_cache_offset()) != k->super_check_offset()) {
        __ br(Assembler::NE, *failure_target);
        // successful cast, fall through to profile or jump
      } else {
        // See if we get an immediate positive hit
        __ br(Assembler::EQ, *success_target);
        // check for self
        __ cmp(klass_RInfo, k_RInfo);
        __ br(Assembler::EQ, *success_target);

	__ stp(klass_RInfo, k_RInfo, Address(__ pre(sp, -2 * wordSize)));
        __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::slow_subtype_check_id)));
	__ ldr(klass_RInfo, Address(__ post(sp, 2 * wordSize)));
        // result is a boolean
	__ cbzw(klass_RInfo, *failure_target);
        // successful cast, fall through to profile or jump
      }
    } else {
      // perform the fast part of the checking logic
      __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, success_target, failure_target, NULL);
      // call out-of-line instance of __ check_klass_subtype_slow_path(...):
      __ stp(klass_RInfo, k_RInfo, Address(__ pre(sp, -2 * wordSize)));
      __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::slow_subtype_check_id)));
      __ ldp(k_RInfo, klass_RInfo, Address(__ post(sp, 2 * wordSize)));
      // result is a boolean
      __ cbz(k_RInfo, *failure_target);
      // successful cast, fall through to profile or jump
    }
  }
  if (op->should_profile()) {
    Register mdo  = klass_RInfo, recv = k_RInfo;
    __ bind(profile_cast_success);
    __ mov_metadata(mdo, md->constant_encoding());
    __ load_klass(recv, obj);
    Label update_done;
    type_profile_helper(mdo, md, data, recv, success);
    __ b(*success);

    __ bind(profile_cast_failure);
    __ mov_metadata(mdo, md->constant_encoding());
    Address counter_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
    __ ldr(rscratch1, counter_addr);
    __ sub(rscratch1, rscratch1, DataLayout::counter_increment);
    __ str(rscratch1, counter_addr);
    __ b(*failure);
  }
  __ b(*success);
}


void LIR_Assembler::emit_opTypeCheck(LIR_OpTypeCheck* op) {
  LIR_Code code = op->code();
  if (code == lir_store_check) {
    Register value = op->object()->as_register();
    Register array = op->array()->as_register();
    Register k_RInfo = op->tmp1()->as_register();
    Register klass_RInfo = op->tmp2()->as_register();
    Register Rtmp1 = op->tmp3()->as_register();

    CodeStub* stub = op->stub();

    // check if it needs to be profiled
    ciMethodData* md;
    ciProfileData* data;

    if (op->should_profile()) {
      ciMethod* method = op->profiled_method();
      assert(method != NULL, "Should have method");
      int bci = op->profiled_bci();
      md = method->method_data_or_null();
      assert(md != NULL, "Sanity");
      data = md->bci_to_data(bci);
      assert(data != NULL,                "need data for type check");
      assert(data->is_ReceiverTypeData(), "need ReceiverTypeData for type check");
    }
    Label profile_cast_success, profile_cast_failure, done;
    Label *success_target = op->should_profile() ? &profile_cast_success : &done;
    Label *failure_target = op->should_profile() ? &profile_cast_failure : stub->entry();

    if (op->should_profile()) {
      Label not_null;
      __ cbnz(value, not_null);
      // Object is null; update MDO and exit
      Register mdo  = klass_RInfo;
      __ mov_metadata(mdo, md->constant_encoding());
      Address data_addr
	= __ form_address(rscratch2, mdo,
			  md->byte_offset_of_slot(data, DataLayout::header_offset()),
			  LogBytesPerInt);
      int header_bits = DataLayout::flag_mask_to_header_mask(BitData::null_seen_byte_constant());
      __ ldrw(rscratch1, data_addr);
      __ orrw(rscratch1, rscratch1, header_bits);
      __ strw(rscratch1, data_addr);
      __ b(done);
      __ bind(not_null);
    } else {
      __ cbz(value, done);
    }

    add_debug_info_for_null_check_here(op->info_for_exception());
    __ load_klass(k_RInfo, array);
    __ load_klass(klass_RInfo, value);

    // get instance klass (it's already uncompressed)
    __ ldr(k_RInfo, Address(k_RInfo, ObjArrayKlass::element_klass_offset()));
    // perform the fast part of the checking logic
    __ check_klass_subtype_fast_path(klass_RInfo, k_RInfo, Rtmp1, success_target, failure_target, NULL);
    // call out-of-line instance of __ check_klass_subtype_slow_path(...):
    __ stp(klass_RInfo, k_RInfo, Address(__ pre(sp, -2 * wordSize)));
    __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::slow_subtype_check_id)));
    __ ldp(k_RInfo, klass_RInfo, Address(__ post(sp, 2 * wordSize)));
    // result is a boolean
    __ cbzw(k_RInfo, *failure_target);
    // fall through to the success case

    if (op->should_profile()) {
      Register mdo  = klass_RInfo, recv = k_RInfo;
      __ bind(profile_cast_success);
      __ mov_metadata(mdo, md->constant_encoding());
      __ load_klass(recv, value);
      Label update_done;
      type_profile_helper(mdo, md, data, recv, &done);
      __ b(done);

      __ bind(profile_cast_failure);
      __ mov_metadata(mdo, md->constant_encoding());
      Address counter_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
      __ ldr(rscratch1, counter_addr);
      __ sub(rscratch1, rscratch1, DataLayout::counter_increment);
      __ str(rscratch1, counter_addr);
      __ b(*stub->entry());
    }

    __ bind(done);
  } else if (code == lir_checkcast) {
    Register obj = op->object()->as_register();
    Register dst = op->result_opr()->as_register();
    Label success;
    emit_typecheck_helper(op, &success, op->stub()->entry(), &success);
    __ bind(success);
    if (dst != obj) {
      __ mov(dst, obj);
    }
  } else if (code == lir_instanceof) {
    Register obj = op->object()->as_register();
    Register dst = op->result_opr()->as_register();
    Label success, failure, done;
    emit_typecheck_helper(op, &success, &failure, &failure);
    __ bind(failure);
    __ mov(dst, zr);
    __ b(done);
    __ bind(success);
    __ mov(dst, 1);
    __ bind(done);
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::casw(Register addr, Register newval, Register cmpval) {
  Label nope;
  // flush and load exclusive from the memory location
  // and fail if it is not what we expect
  __ ldaxrw(rscratch1, addr);
  __ cmpw(rscratch1, cmpval);
  __ cset(rscratch1, Assembler::NE);
  __ br(Assembler::NE, nope);
  // if we store+flush with no intervening write rscratch1 wil be zero
  __ stlxrw(rscratch1, newval, addr);
  __ bind(nope);
}

void LIR_Assembler::casl(Register addr, Register newval, Register cmpval) {
  Label nope;
  // flush and load exclusive from the memory location
  // and fail if it is not what we expect
  __ ldaxr(rscratch1, addr);
  __ cmp(rscratch1, cmpval);
  __ cset(rscratch1, Assembler::NE);
  __ br(Assembler::NE, nope);
  // if we store+flush with no intervening write rscratch1 wil be zero
  __ stlxr(rscratch1, newval, addr);
  __ bind(nope);
}


void LIR_Assembler::emit_compare_and_swap(LIR_OpCompareAndSwap* op) {
  assert(VM_Version::supports_cx8(), "wrong machine");
  Register addr = as_reg(op->addr());
  Register newval = as_reg(op->new_value());
  Register cmpval = as_reg(op->cmp_value());
  Label succeed, fail, around;

  if (op->code() == lir_cas_obj) {
    if (UseCompressedOops) {
      __ encode_heap_oop(cmpval);
      __ mov(rscratch2, newval);
      newval = rscratch2;
      __ encode_heap_oop(newval);
      casw(addr, newval, cmpval);
    } else {
      casl(addr, newval, cmpval);
    }
  } else if (op->code() == lir_cas_int) {
    casw(addr, newval, cmpval);
  } else {
    casl(addr, newval, cmpval);
  }
}


void LIR_Assembler::cmove(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Opr result, BasicType type) {

  Assembler::Condition acond, ncond;
  switch (condition) {
  case lir_cond_equal:        acond = Assembler::EQ; ncond = Assembler::NE; break;
  case lir_cond_notEqual:     acond = Assembler::NE; ncond = Assembler::EQ; break;
  case lir_cond_less:         acond = Assembler::LT; ncond = Assembler::GE; break;
  case lir_cond_lessEqual:    acond = Assembler::LE; ncond = Assembler::GT; break;
  case lir_cond_greaterEqual: acond = Assembler::GE; ncond = Assembler::LT; break;
  case lir_cond_greater:      acond = Assembler::GT; ncond = Assembler::LE; break;
  case lir_cond_belowEqual:   Unimplemented(); break;
  case lir_cond_aboveEqual:   Unimplemented(); break;
  default:                    ShouldNotReachHere();
  }

  assert(result->is_single_cpu() || result->is_double_cpu(),
	 "expect single register for result");
  if (opr1->is_constant() && opr2->is_constant()
      && opr1->type() == T_INT && opr2->type() == T_INT) {
    jint val1 = opr1->as_jint();
    jint val2 = opr2->as_jint();
    if (val1 == 0 && val2 == 1) {
      __ cset(result->as_register(), ncond);
      return;
    } else if (val1 == 1 && val2 == 0) {
      __ cset(result->as_register(), acond);
      return;
    }
  }

  if (opr1->is_constant() && opr2->is_constant()
      && opr1->type() == T_LONG && opr2->type() == T_LONG) {
    jlong val1 = opr1->as_jlong();
    jlong val2 = opr2->as_jlong();
    if (val1 == 0 && val2 == 1) {
      __ cset(result->as_register_lo(), ncond);
      return;
    } else if (val1 == 1 && val2 == 0) {
      __ cset(result->as_register_lo(), acond);
      return;
    }
  }

  if (opr1->is_stack()) {
    stack2reg(opr1, FrameMap::rscratch1_opr, result->type());
    opr1 = FrameMap::rscratch1_opr;
  } else if (opr1->is_constant()) {
    LIR_Opr tmp
      = opr1->type() == T_LONG ? FrameMap::rscratch1_long_opr : FrameMap::rscratch1_opr;
    const2reg(opr1, tmp, lir_patch_none, NULL);
    opr1 = tmp;
  }

  if (opr2->is_stack()) {
    stack2reg(opr2, FrameMap::rscratch2_opr, result->type());
    opr2 = FrameMap::rscratch2_opr;
  } else if (opr2->is_constant()) {
    LIR_Opr tmp
      = opr2->type() == T_LONG ? FrameMap::rscratch2_long_opr : FrameMap::rscratch2_opr;
    const2reg(opr2, tmp, lir_patch_none, NULL);
    opr2 = tmp;
  }

  if (result->type() == T_LONG)
    __ csel(result->as_register_lo(), opr1->as_register_lo(), opr2->as_register_lo(), acond);
  else
    __ csel(result->as_register(), opr1->as_register(), opr2->as_register(), acond);
}

void LIR_Assembler::arith_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest, CodeEmitInfo* info, bool pop_fpu_stack) {
  assert(info == NULL, "should never be used, idiv/irem and ldiv/lrem not handled by this method");

  if (left->is_single_cpu()) {
    Register lreg = left->as_register();
    Register dreg = as_reg(dest);

    if (right->is_single_cpu()) {
      // cpu register - cpu register

      assert(left->type() == T_INT && right->type() == T_INT && dest->type() == T_INT,
	     "should be");
      Register rreg = right->as_register();
      switch (code) {
      case lir_add: __ addw (dest->as_register(), lreg, rreg); break;
      case lir_sub: __ subw (dest->as_register(), lreg, rreg); break;
      case lir_mul: __ mulw (dest->as_register(), lreg, rreg); break;
      default:      ShouldNotReachHere();
      }

    } else if (right->is_double_cpu()) {
      Register rreg = right->as_register_lo();
      // single_cpu + double_cpu: can happen with obj+long
      assert(code == lir_add || code == lir_sub, "mismatched arithmetic op");
      switch (code) {
      case lir_add: __ add(dreg, lreg, rreg); break;
      case lir_sub: __ sub(dreg, lreg, rreg); break;
      default: ShouldNotReachHere();
      }
    } else if (right->is_constant()) {
      // cpu register - constant
      jlong c;

      // FIXME.  This is fugly: we really need to factor all this logic.
      switch(right->type()) {
      case T_LONG:
	c = right->as_constant_ptr()->as_jlong();
	break;
      case T_INT:
      case T_ADDRESS:
	c = right->as_constant_ptr()->as_jint();
	break;
      default:
	ShouldNotReachHere();
	break;
      }

      assert(code == lir_add || code == lir_sub, "mismatched arithmetic op");
      if (c == 0 && dreg == lreg) {
	COMMENT("effective nop elided");
	return;
      }
      switch(left->type()) {
      case T_INT:
	switch (code) {
        case lir_add: __ addw(dreg, lreg, c); break;
        case lir_sub: __ subw(dreg, lreg, c); break;
        default: ShouldNotReachHere();
	}
	break;
      case T_OBJECT:
      case T_ADDRESS:
	switch (code) {
        case lir_add: __ add(dreg, lreg, c); break;
        case lir_sub: __ sub(dreg, lreg, c); break;
        default: ShouldNotReachHere();
	}
	break;
	ShouldNotReachHere();
      }
    } else {
      ShouldNotReachHere();
    }

  } else if (left->is_double_cpu()) {
    Register lreg_lo = left->as_register_lo();

    if (right->is_double_cpu()) {
      // cpu register - cpu register
      Register rreg_lo = right->as_register_lo();
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
      Register dreg = as_reg(dest);
      assert(code == lir_add || code == lir_sub, "mismatched arithmetic op");
      if (c == 0 && dreg == lreg_lo) {
	COMMENT("effective nop elided");
	return;
      }
      switch (code) {
        case lir_add: __ add(dreg, lreg_lo, c); break;
        case lir_sub: __ sub(dreg, lreg_lo, c); break;
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


void LIR_Assembler::intrinsic_op(LIR_Code code, LIR_Opr value, LIR_Opr unused, LIR_Opr dest, LIR_Op* op) {
  switch(code) {
  case lir_abs : __ fabsd(dest->as_double_reg(), value->as_double_reg()); break;
  case lir_sqrt: __ fsqrtd(dest->as_double_reg(), value->as_double_reg()); break;
  default      : ShouldNotReachHere();
  }
}

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



void LIR_Assembler::arithmetic_idiv(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr temp, LIR_Opr result, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::comp_op(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Op2* op) {
  if (opr1->is_constant() && opr2->is_single_cpu()) {
    // tableswitch
    Register reg = as_reg(opr2);
    struct tableswitch &table = switches[opr1->as_constant_ptr()->as_jint()];
    __ tableswitch(reg, table._first_key, table._last_key, table._branches, table._after);
  } else if (opr1->is_single_cpu() || opr1->is_double_cpu()) {
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
      __ cmp(reg1, reg2);
      return;
    }

    if (opr2->is_constant()) {
      jlong imm;
      switch(opr2->type()) {
      case T_LONG:
	imm = opr2->as_constant_ptr()->as_jlong();
	break;
      case T_INT:
      case T_ADDRESS:
	imm = opr2->as_constant_ptr()->as_jint();
	break;
      case T_OBJECT:
      case T_ARRAY:
	imm = jlong(opr2->as_constant_ptr()->as_jobject());
	break;
      default:
	ShouldNotReachHere();
	break;
      }

      if (Assembler::operand_valid_for_add_sub_immediate(imm)) {
	if (type2aelembytes(opr1->type()) <= 4)
	  __ cmpw(reg1, imm);
	else
	  __ cmp(reg1, imm);
	return;
      } else {
	__ mov(rscratch1, imm);
	if (type2aelembytes(opr1->type()) <= 4)
	  __ cmpw(reg1, rscratch1);
	else
	  __ cmp(reg1, rscratch1);
	return;
      }
    } else
      ShouldNotReachHere();
  } else if (opr1->is_single_fpu()) {
    FloatRegister reg1 = opr1->as_float_reg();
    assert(opr2->is_single_fpu(), "expect single float register");
    FloatRegister reg2 = opr2->as_float_reg();
    __ fcmps(reg1, reg2);
  } else if (opr1->is_double_fpu()) {
    FloatRegister reg1 = opr1->as_double_reg();
    assert(opr2->is_double_fpu(), "expect double float register");
    FloatRegister reg2 = opr2->as_double_reg();
    __ fcmpd(reg1, reg2);
  } else {
    ShouldNotReachHere();
  }
}

void LIR_Assembler::comp_fl2i(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst, LIR_Op2* op){
  if (code == lir_cmp_fd2i || code == lir_ucmp_fd2i) {
    bool is_unordered_less = (code == lir_ucmp_fd2i);
    if (left->is_single_fpu()) {
      __ float_cmp(true, is_unordered_less ? -1 : 1, left->as_float_reg(), right->as_float_reg(), dst->as_register());
    } else if (left->is_double_fpu()) {
      __ float_cmp(false, is_unordered_less ? -1 : 1, left->as_double_reg(), right->as_double_reg(), dst->as_register());
    } else {
      ShouldNotReachHere();
    }
  } else if (code == lir_cmp_l2i) {
    Label done;
    __ cmp(left->as_register_lo(), right->as_register_lo());
    __ mov(dst->as_register(), (u_int64_t)-1L);
    __ br(Assembler::LT, done);
    __ csinc(dst->as_register(), zr, zr, Assembler::EQ);
    __ bind(done);
  } else {
    ShouldNotReachHere();
  }
}


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


void LIR_Assembler::throw_op(LIR_Opr exceptionPC, LIR_Opr exceptionOop, CodeEmitInfo* info) {
  assert(exceptionOop->as_register() == r0, "must match");
  assert(exceptionPC->as_register() == r3, "must match");

  // exception object is not added to oop map by LinearScan
  // (LinearScan assumes that no oops are in fixed registers)
  info->add_register_oop(exceptionOop);
  Runtime1::StubID unwind_id;

  // get current pc information
  // pc is only needed if the method has an exception handler, the unwind code does not need it.
  int pc_for_athrow_offset = __ offset();
  InternalAddress pc_for_athrow(__ pc());
  __ adr(exceptionPC->as_register(), pc_for_athrow);
  add_call_info(pc_for_athrow_offset, info); // for exception handler

  __ verify_not_null_oop(r0);
  // search an exception handler (rax: exception oop, rdx: throwing pc)
  if (compilation()->has_fpu_code()) {
    unwind_id = Runtime1::handle_exception_id;
  } else {
    unwind_id = Runtime1::handle_exception_nofpu_id;
  }
  __ bl(RuntimeAddress(Runtime1::entry_for(unwind_id)));

  // FIXME: enough room for two byte trap   ????
  __ nop();
}


void LIR_Assembler::unwind_op(LIR_Opr exceptionOop) {
  assert(exceptionOop->as_register() == r0, "must match");

  __ b(_unwind_handler_entry);
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, LIR_Opr count, LIR_Opr dest, LIR_Opr tmp) {
  Register lreg = left->is_single_cpu() ? left->as_register() : left->as_register_lo();
  Register dreg = dest->is_single_cpu() ? dest->as_register() : dest->as_register_lo();

  switch (left->type()) {
    case T_INT: {
      switch (code) {
      case lir_shl:  __ lslvw (dreg, lreg, count->as_register()); break;
      case lir_shr:  __ asrvw (dreg, lreg, count->as_register()); break;
      case lir_ushr: __ lsrvw (dreg, lreg, count->as_register()); break;
      default:
	ShouldNotReachHere();
	break;
      }
      break;
    case T_LONG:
    case T_ADDRESS:
    case T_OBJECT:
      switch (code) {
      case lir_shl:  __ lslv (dreg, lreg, count->as_register()); break;
      case lir_shr:  __ asrv (dreg, lreg, count->as_register()); break;
      case lir_ushr: __ lsrv (dreg, lreg, count->as_register()); break;
      default:
	ShouldNotReachHere();
	break;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, jint count, LIR_Opr dest) {
  Register dreg = dest->is_single_cpu() ? dest->as_register() : dest->as_register_lo();
  Register lreg = left->is_single_cpu() ? left->as_register() : left->as_register_lo();

  switch (left->type()) {
    case T_INT: {
      switch (code) {
      case lir_shl:  __ lslw (dreg, lreg, count); break;
      case lir_shr:  __ asrw (dreg, lreg, count); break;
      case lir_ushr: __ lsrw (dreg, lreg, count); break;
      default:
	ShouldNotReachHere();
	break;
      }
      break;
    case T_LONG:
    case T_ADDRESS:
    case T_OBJECT:
      switch (code) {
      case lir_shl:  __ lsl (dreg, lreg, count); break;
      case lir_shr:  __ asr (dreg, lreg, count); break;
      case lir_ushr: __ lsr (dreg, lreg, count); break;
      default:
	ShouldNotReachHere();
	break;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }
}


void LIR_Assembler::store_parameter(Register r, int offset_from_rsp_in_words) {
  assert(offset_from_rsp_in_words >= 0, "invalid offset from rsp");
  int offset_from_rsp_in_bytes = offset_from_rsp_in_words * BytesPerWord;
  assert(offset_from_rsp_in_bytes < frame_map()->reserved_argument_area_size(), "invalid offset");
  __ str (r, Address(sp, offset_from_rsp_in_bytes));
}


void LIR_Assembler::store_parameter(jint c,     int offset_from_rsp_in_words) {
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
void LIR_Assembler::emit_arraycopy(LIR_OpArrayCopy* op) {
  ciArrayKlass* default_type = op->expected_type();
  Register src = op->src()->as_register();
  Register dst = op->dst()->as_register();
  Register src_pos = op->src_pos()->as_register();
  Register dst_pos = op->dst_pos()->as_register();
  Register length  = op->length()->as_register();
  Register tmp = op->tmp()->as_register();

  CodeStub* stub = op->stub();
  int flags = op->flags();
  BasicType basic_type = default_type != NULL ? default_type->element_type()->basic_type() : T_ILLEGAL;
  if (basic_type == T_ARRAY) basic_type = T_OBJECT;

  // if we don't know anything, just go through the generic arraycopy
  if (default_type == NULL // || basic_type == T_OBJECT
      ) {
    Label done;
    assert(src == r1 && src_pos == r2, "mismatch in calling convention");

    // Save the arguments in case the generic arraycopy fails and we
    // have to fall back to the JNI stub
    __ stp(dst,     dst_pos, Address(sp, 0*BytesPerWord));
    __ stp(length,  src_pos, Address(sp, 2*BytesPerWord));
    __ str(src,              Address(sp, 4*BytesPerWord));

    address C_entry = CAST_FROM_FN_PTR(address, Runtime1::arraycopy);
    address copyfunc_addr = StubRoutines::generic_arraycopy();

    // The arguments are in java calling convention so we shift them
    // to C convention
    assert_different_registers(c_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4);
    __ mov(c_rarg0, j_rarg0);
    assert_different_registers(c_rarg1, j_rarg2, j_rarg3, j_rarg4);
    __ mov(c_rarg1, j_rarg1);
    assert_different_registers(c_rarg2, j_rarg3, j_rarg4);
    __ mov(c_rarg2, j_rarg2);
    assert_different_registers(c_rarg3, j_rarg4);
    __ mov(c_rarg3, j_rarg3);
    __ mov(c_rarg4, j_rarg4);
    if (copyfunc_addr == NULL) { // Use C version if stub was not generated
      __ mov(rscratch1, RuntimeAddress(C_entry));
      __ blrt(rscratch1, 5, 0, 1);
    } else {
#ifndef PRODUCT
      if (PrintC1Statistics) {
        __ incrementw(ExternalAddress((address)&Runtime1::_generic_arraycopystub_cnt));
      }
#endif
      __ bl(RuntimeAddress(copyfunc_addr));
    }

    __ cbz(r0, *stub->continuation());

    // Reload values from the stack so they are where the stub
    // expects them.
    __ ldp(dst,     dst_pos, Address(sp, 0*BytesPerWord));
    __ ldp(length,  src_pos, Address(sp, 2*BytesPerWord));
    __ ldr(src,              Address(sp, 4*BytesPerWord));

    if (copyfunc_addr != NULL) {
      __ subw(rscratch1, length, r0); // Number of oops actually copied
      __ addw(src_pos, src_pos, rscratch1);
      __ addw(dst_pos, dst_pos, rscratch1);
      __ mov(length, r0); // Number of oops left to copy
    }
    __ b(*stub->entry());

    __ bind(*stub->continuation());
    return;
  }

  assert(default_type != NULL && default_type->is_array_klass() && default_type->is_loaded(), "must be true at this point");

  int elem_size = type2aelembytes(basic_type);
  int shift_amount;
  int scale = exact_log2(elem_size);

  Address src_length_addr = Address(src, arrayOopDesc::length_offset_in_bytes());
  Address dst_length_addr = Address(dst, arrayOopDesc::length_offset_in_bytes());
  Address src_klass_addr = Address(src, oopDesc::klass_offset_in_bytes());
  Address dst_klass_addr = Address(dst, oopDesc::klass_offset_in_bytes());

  // length and pos's are all sign extended at this point on 64bit

  // test for NULL
  if (flags & LIR_OpArrayCopy::src_null_check) {
    __ cbz(src, *stub->entry());
  }
  if (flags & LIR_OpArrayCopy::dst_null_check) {
    __ cbz(dst, *stub->entry());
  }

  // check if negative
  if (flags & LIR_OpArrayCopy::src_pos_positive_check) {
    __ cmpw(src_pos, 0);
    __ br(Assembler::LT, *stub->entry());
  }
  if (flags & LIR_OpArrayCopy::dst_pos_positive_check) {
    __ cmpw(dst_pos, 0);
    __ br(Assembler::LT, *stub->entry());
  }

  if (flags & LIR_OpArrayCopy::src_range_check) {
    __ lea(tmp, Address(src_pos, length));
    __ ldrw(rscratch1, src_length_addr);
    __ cmpw(tmp, rscratch1);
    __ br(Assembler::HI, *stub->entry());
  }
  if (flags & LIR_OpArrayCopy::dst_range_check) {
    __ lea(tmp, Address(dst_pos, length));
    __ ldrw(rscratch1, dst_length_addr);
    __ cmpw(tmp, rscratch1);
    __ br(Assembler::HI, *stub->entry());
  }

  if (flags & LIR_OpArrayCopy::length_positive_check) {
    __ cmpw(length, 0);
    __ br(Assembler::LT, *stub->entry());
  }

  // FIXME: The logic in LIRGenerator::arraycopy_helper clears
  // length_positive_check if the source of our length operand is an
  // arraylength.  However, that arraylength might be zero, and the
  // stub that we're about to call contains an assertion that count !=
  // 0 .  So we make this check purely in order not to trigger an
  // assertion failure.
  __ cbz(length, *stub->continuation());

  if (flags & LIR_OpArrayCopy::type_check) {
    // We don't know the array types are compatible
    if (basic_type != T_OBJECT) {
      // Simple test for basic type arrays
      if (UseCompressedKlassPointers) {
        __ ldrw(tmp, src_klass_addr);
        __ ldrw(rscratch1, dst_klass_addr);
        __ cmpw(tmp, rscratch1);
      } else {
        __ ldr(tmp, src_klass_addr);
        __ ldr(rscratch1, dst_klass_addr);
        __ cmp(tmp, rscratch1);
      }
      __ br(Assembler::NE, *stub->entry());
    } else {
      // For object arrays, if src is a sub class of dst then we can
      // safely do the copy.
      Label cont, slow;

#define PUSH(r1, r2)					\
      stp(r1, r2, __ pre(sp, -2 * wordSize));

#define POP(r1, r2)					\
      ldp(r1, r2, __ post(sp, 2 * wordSize));

      __ PUSH(src, dst);

      __ load_klass(src, src);
      __ load_klass(dst, dst);

      __ check_klass_subtype_fast_path(src, dst, tmp, &cont, &slow, NULL);

      __ PUSH(src, dst);
      __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::slow_subtype_check_id)));
      __ POP(src, dst);

      __ cbnz(src, cont);

      __ bind(slow);
      __ POP(src, dst);

      address copyfunc_addr = StubRoutines::checkcast_arraycopy();
      if (copyfunc_addr != NULL) { // use stub if available
        // src is not a sub class of dst so we have to do a
        // per-element check.

        int mask = LIR_OpArrayCopy::src_objarray|LIR_OpArrayCopy::dst_objarray;
        if ((flags & mask) != mask) {
          // Check that at least both of them object arrays.
          assert(flags & mask, "one of the two should be known to be an object array");

          if (!(flags & LIR_OpArrayCopy::src_objarray)) {
            __ load_klass(tmp, src);
          } else if (!(flags & LIR_OpArrayCopy::dst_objarray)) {
            __ load_klass(tmp, dst);
          }
          int lh_offset = in_bytes(Klass::layout_helper_offset());
          Address klass_lh_addr(tmp, lh_offset);
          jint objArray_lh = Klass::array_layout_helper(T_OBJECT);
	  __ ldrw(rscratch1, klass_lh_addr);
	  __ mov(rscratch2, objArray_lh);
          __ eorw(rscratch1, rscratch1, rscratch2);
          __ cbnzw(rscratch1, *stub->entry());
        }

       // Spill because stubs can use any register they like and it's
       // easier to restore just those that we care about.
	__ stp(dst,     dst_pos, Address(sp, 0*BytesPerWord));
	__ stp(length,  src_pos, Address(sp, 2*BytesPerWord));
	__ str(src,              Address(sp, 4*BytesPerWord));

        __ uxtw(length, length); // FIXME ??  higher 32bits must be null

        __ lea(c_rarg0, Address(src, src_pos, Address::uxtw(scale)));
	__ add(c_rarg0, c_rarg0, arrayOopDesc::base_offset_in_bytes(basic_type));
        assert_different_registers(c_rarg0, dst, dst_pos, length);
        __ lea(c_rarg1, Address(dst, dst_pos, Address::uxtw(scale)));
	__ add(c_rarg1, c_rarg1, arrayOopDesc::base_offset_in_bytes(basic_type));
        assert_different_registers(c_rarg1, dst, length);
        __ mov(c_rarg2, length);
        assert_different_registers(c_rarg2, dst);

        __ load_klass(c_rarg4, dst);
        __ ldr(c_rarg4, Address(c_rarg4, ObjArrayKlass::element_klass_offset()));
        __ ldrw(c_rarg3, Address(c_rarg4, Klass::super_check_offset_offset()));
        __ call(RuntimeAddress(copyfunc_addr));

#ifndef PRODUCT
        if (PrintC1Statistics) {
          Label failed;
          __ cbnz(r0, failed);
          __ incrementw(ExternalAddress((address)&Runtime1::_arraycopy_checkcast_cnt));
          __ bind(failed);
        }
#endif

        __ cbz(r0, *stub->continuation());

#ifndef PRODUCT
        if (PrintC1Statistics) {
          __ incrementw(ExternalAddress((address)&Runtime1::_arraycopy_checkcast_attempt_cnt));
        }
#endif
	assert_different_registers(dst, dst_pos, length, src_pos, src, r0, rscratch1);

        // Restore previously spilled arguments
	__ ldp(dst,     dst_pos, Address(sp, 0*BytesPerWord));
	__ ldp(length,  src_pos, Address(sp, 2*BytesPerWord));
	__ ldr(src,              Address(sp, 4*BytesPerWord));

	__ subw(rscratch1, length, r0); // Number of oops actually copied
	__ addw(src_pos, src_pos, rscratch1);
	__ addw(dst_pos, dst_pos, rscratch1);
	__ mov(length, r0); // Number of oops left to copy
      }

      __ b(*stub->entry());

      __ bind(cont);
      __ POP(src, dst);
    }
  }

#ifdef ASSERT
  if (basic_type != T_OBJECT || !(flags & LIR_OpArrayCopy::type_check)) {
    // Sanity check the known type with the incoming class.  For the
    // primitive case the types must match exactly with src.klass and
    // dst.klass each exactly matching the default type.  For the
    // object array case, if no type check is needed then either the
    // dst type is exactly the expected type and the src type is a
    // subtype which we can't check or src is the same array as dst
    // but not necessarily exactly of type default_type.
    Label known_ok, halt;
    __ mov_metadata(tmp, default_type->constant_encoding());
#ifdef _LP64
    if (UseCompressedKlassPointers) {
      __ encode_klass_not_null(tmp);
    }
#endif

    if (basic_type != T_OBJECT) {

      if (UseCompressedKlassPointers) {
	__ ldrw(rscratch1, dst_klass_addr);
	__ cmpw(tmp, rscratch1);
      } else {
	__ ldr(rscratch1, dst_klass_addr);
	__ cmp(tmp, rscratch1);
      }
      __ br(Assembler::NE, halt);
      if (UseCompressedKlassPointers) {
	__ ldrw(rscratch1, src_klass_addr);
	__ cmpw(tmp, rscratch1);
      } else {
	__ ldr(rscratch1, src_klass_addr);
	__ cmp(tmp, rscratch1);
      }
      __ br(Assembler::EQ, known_ok);
    } else {
      if (UseCompressedKlassPointers) {
	__ ldrw(rscratch1, dst_klass_addr);
	__ cmpw(tmp, rscratch1);
      } else {
	__ ldr(rscratch1, dst_klass_addr);
	__ cmp(tmp, rscratch1);
      }
      __ br(Assembler::EQ, known_ok);
      __ cmp(src, dst);
      __ br(Assembler::EQ, known_ok);
    }
    __ bind(halt);
    __ stop("incorrect type information in arraycopy");
    __ bind(known_ok);
  }
#endif

#ifndef PRODUCT
  if (PrintC1Statistics) {
    __ incrementw(ExternalAddress(Runtime1::arraycopy_count_address(basic_type)));
  }
#endif

  __ lea(c_rarg0, Address(src, src_pos, Address::uxtw(scale)));
  __ add(c_rarg0, c_rarg0, arrayOopDesc::base_offset_in_bytes(basic_type));
  assert_different_registers(c_rarg0, dst, dst_pos, length);
  __ lea(c_rarg1, Address(dst, dst_pos, Address::uxtw(scale)));
  __ add(c_rarg1, c_rarg1, arrayOopDesc::base_offset_in_bytes(basic_type));
  assert_different_registers(c_rarg1, dst, length);
  __ mov(c_rarg2, length);
  assert_different_registers(c_rarg2, dst);

  bool disjoint = (flags & LIR_OpArrayCopy::overlapping) == 0;
  bool aligned = (flags & LIR_OpArrayCopy::unaligned) == 0;
  const char *name;
  address entry = StubRoutines::select_arraycopy_function(basic_type, aligned, disjoint, name, false);

 CodeBlob *cb = CodeCache::find_blob(entry);
 if (cb) {
   __ bl(RuntimeAddress(entry));
 } else {
   __ call_VM_leaf(entry, 3);
 }

  __ bind(*stub->continuation());
}




void LIR_Assembler::emit_lock(LIR_OpLock* op) {
  Register obj = op->obj_opr()->as_register();  // may not be an oop
  Register hdr = op->hdr_opr()->as_register();
  Register lock = op->lock_opr()->as_register();
  if (!UseFastLocking) {
    __ b(*op->stub()->entry());
  } else if (op->code() == lir_lock) {
    Register scratch = noreg;
    if (UseBiasedLocking) {
      scratch = op->scratch_opr()->as_register();
    }
    assert(BasicLock::displaced_header_offset_in_bytes() == 0, "lock_reg must point to the displaced header");
    // add debug info for NullPointerException only if one is possible
    int null_check_offset = __ lock_object(hdr, obj, lock, scratch, *op->stub()->entry());
    if (op->info() != NULL) {
      add_debug_info_for_null_check(null_check_offset, op->info());
    }
    // done
  } else if (op->code() == lir_unlock) {
    assert(BasicLock::displaced_header_offset_in_bytes() == 0, "lock_reg must point to the displaced header");
    __ unlock_object(hdr, obj, lock, *op->stub()->entry());
  } else {
    Unimplemented();
  }
  __ bind(*op->stub()->continuation());
}


void LIR_Assembler::emit_profile_call(LIR_OpProfileCall* op) {
  ciMethod* method = op->profiled_method();
  int bci          = op->profiled_bci();
  ciMethod* callee = op->profiled_callee();

  // Update counter for all call types
  ciMethodData* md = method->method_data_or_null();
  assert(md != NULL, "Sanity");
  ciProfileData* data = md->bci_to_data(bci);
  assert(data->is_CounterData(), "need CounterData for calls");
  assert(op->mdo()->is_single_cpu(),  "mdo must be allocated");
  Register mdo  = op->mdo()->as_register();
  __ mov_metadata(mdo, md->constant_encoding());
  Address counter_addr(mdo, md->byte_offset_of_slot(data, CounterData::count_offset()));
  Bytecodes::Code bc = method->java_code_at_bci(bci);
  const bool callee_is_static = callee->is_loaded() && callee->is_static();
  // Perform additional virtual call profiling for invokevirtual and
  // invokeinterface bytecodes
  if ((bc == Bytecodes::_invokevirtual || bc == Bytecodes::_invokeinterface) &&
      !callee_is_static &&  // required for optimized MH invokes
      C1ProfileVirtualCalls) {
    assert(op->recv()->is_single_cpu(), "recv must be allocated");
    Register recv = op->recv()->as_register();
    assert_different_registers(mdo, recv);
    assert(data->is_VirtualCallData(), "need VirtualCallData for virtual calls");
    ciKlass* known_klass = op->known_holder();
    if (C1OptimizeVirtualCallProfiling && known_klass != NULL) {
      // We know the type that will be seen at this call site; we can
      // statically update the MethodData* rather than needing to do
      // dynamic tests on the receiver type

      // NOTE: we should probably put a lock around this search to
      // avoid collisions by concurrent compilations
      ciVirtualCallData* vc_data = (ciVirtualCallData*) data;
      uint i;
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (known_klass->equals(receiver)) {
          Address data_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)));
          __ addptr(data_addr, DataLayout::counter_increment);
          return;
        }
      }

      // Receiver type not found in profile data; select an empty slot

      // Note that this is less efficient than it should be because it
      // always does a write to the receiver part of the
      // VirtualCallData rather than just the first time
      for (i = 0; i < VirtualCallData::row_limit(); i++) {
        ciKlass* receiver = vc_data->receiver(i);
        if (receiver == NULL) {
          Address recv_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_offset(i)));
	  __ mov_metadata(rscratch1, known_klass->constant_encoding());
          __ str(rscratch1, recv_addr);
          Address data_addr(mdo, md->byte_offset_of_slot(data, VirtualCallData::receiver_count_offset(i)));
	  __ addptr(counter_addr, DataLayout::counter_increment);
          return;
        }
      }
    } else {
      __ load_klass(recv, recv);
      Label update_done;
      type_profile_helper(mdo, md, data, recv, &update_done);
      // Receiver did not match any saved receiver and there is no empty row for it.
      // Increment total counter to indicate polymorphic case.
      __ addptr(counter_addr, DataLayout::counter_increment);

      __ bind(update_done);
    }
  } else {
    // Static call
    __ addptr(counter_addr, DataLayout::counter_increment);
  }
}


void LIR_Assembler::emit_delay(LIR_OpDelay*) {
  Unimplemented();
}


void LIR_Assembler::monitor_address(int monitor_no, LIR_Opr dst) {
  __ lea(dst->as_register(), frame_map()->address_for_monitor_lock(monitor_no));
}


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


void LIR_Assembler::leal(LIR_Opr addr, LIR_Opr dest) {
  __ lea(dest->as_register_lo(), as_Address(addr->as_address_ptr()));
}


void LIR_Assembler::rt_call(LIR_Opr result, address dest, const LIR_OprList* args, LIR_Opr tmp, CodeEmitInfo* info) {
  assert(!tmp->is_valid(), "don't need temporary");

  CodeBlob *cb = CodeCache::find_blob(dest);
  if (cb) {
    __ bl(RuntimeAddress(dest));
  } else {
    __ mov(rscratch1, RuntimeAddress(dest));
    int len = args->length();
    int type = 0;
    if (! result->is_illegal()) {
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
    __ blrt(rscratch1, num_gpargs, num_fpargs, type);
  }

  if (info != NULL) {
    add_call_info_here(info);
  }
}

void LIR_Assembler::volatile_move_op(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info) {
  if (dest->is_address()) {
      LIR_Address* to_addr = dest->as_address_ptr();
      Register compressed_src = noreg;
      if (is_reg(src)) {
	  compressed_src = as_reg(src);
	  if (type == T_ARRAY || type == T_OBJECT) {
	    __ verify_oop(src->as_register());
	    if (UseCompressedOops) {
	      compressed_src = rscratch2;
	      __ mov(compressed_src, src->as_register());
	      __ encode_heap_oop(compressed_src);
	    }
	  }
      } else if (src->is_single_fpu()) {
	__ fmovs(rscratch2, src->as_float_reg());
	src = FrameMap::rscratch2_opr,	type = T_INT;
      } else if (src->is_double_fpu()) {
	__ fmovd(rscratch2, src->as_double_reg());
	src = FrameMap::rscratch2_long_opr, type = T_LONG;
      }

      if (dest->is_double_cpu())
	__ lea(rscratch1, as_Address(to_addr));
      else
	__ lea(rscratch1, as_Address_lo(to_addr));

      int null_check_here = code_offset();
      switch (type) {
      case T_ARRAY:   // fall through
      case T_OBJECT:  // fall through
	if (UseCompressedOops) {
	  __ stlrw(compressed_src, rscratch1);
	} else {
	  __ stlr(compressed_src, rscratch1);
	}
	break;
      case T_METADATA:
	// We get here to store a method pointer to the stack to pass to
	// a dtrace runtime call. This can't work on 64 bit with
	// compressed klass ptrs: T_METADATA can be a compressed klass
	// ptr or a 64 bit method pointer.
	LP64_ONLY(ShouldNotReachHere());
	__ stlr(src->as_register(), rscratch1);
	break;
      case T_ADDRESS:
	__ stlr(src->as_register(), rscratch1);
	break;
      case T_INT:
	__ stlrw(src->as_register(), rscratch1);
	break;

      case T_LONG: {
	__ stlr(src->as_register_lo(), rscratch1);
	break;
      }

      case T_BYTE:    // fall through
      case T_BOOLEAN: {
	__ stlrb(src->as_register(), rscratch1);
	break;
      }

      case T_CHAR:    // fall through
      case T_SHORT:
	__ stlrh(src->as_register(), rscratch1);
	break;

      default:
	ShouldNotReachHere();
      }
      if (info != NULL) {
	add_debug_info_for_null_check(null_check_here, info);
      }
  } else if (src->is_address()) {
    LIR_Address* from_addr = src->as_address_ptr();

    if (src->is_double_cpu())
      __ lea(rscratch1, as_Address(from_addr));
    else
      __ lea(rscratch1, as_Address_lo(from_addr));

    int null_check_here = code_offset();
    switch (type) {
    case T_ARRAY:   // fall through
    case T_OBJECT:  // fall through
      if (UseCompressedOops) {
	__ ldarw(dest->as_register(), rscratch1);
      } else {
	__ ldar(dest->as_register(), rscratch1);
      }
      break;
    case T_ADDRESS:
      __ ldar(dest->as_register(), rscratch1);
      break;
    case T_INT:
      __ ldarw(dest->as_register(), rscratch1);
      break;
    case T_LONG: {
      __ ldar(dest->as_register_lo(), rscratch1);
      break;
    }

    case T_BYTE:    // fall through
    case T_BOOLEAN: {
      __ ldarb(dest->as_register(), rscratch1);
      break;
    }

    case T_CHAR:    // fall through
    case T_SHORT:
      __ ldarh(dest->as_register(), rscratch1);
      break;

    case T_FLOAT:
      __ ldarw(rscratch2, rscratch1);
      __ fmovs(dest->as_float_reg(), rscratch2);
      break;

    case T_DOUBLE:
      __ ldar(rscratch2, rscratch1);
      __ fmovd(dest->as_double_reg(), rscratch2);
      break;

    default:
      ShouldNotReachHere();
    }
    if (info != NULL) {
      add_debug_info_for_null_check(null_check_here, info);
    }

    if (type == T_ARRAY || type == T_OBJECT) {
      if (UseCompressedOops) {
	__ decode_heap_oop(dest->as_register());
      }
      __ verify_oop(dest->as_register());
    } else if (type == T_ADDRESS && from_addr->disp() == oopDesc::klass_offset_in_bytes()) {
      if (UseCompressedKlassPointers) {
	__ decode_klass_not_null(dest->as_register());
      }
    }
  } else
    ShouldNotReachHere();
}

#ifdef ASSERT
// emit run-time assertion
void LIR_Assembler::emit_assert(LIR_OpAssert* op) {
  assert(op->code() == lir_assert, "must be");

  if (op->in_opr1()->is_valid()) {
    assert(op->in_opr2()->is_valid(), "both operands must be valid");
    comp_op(op->condition(), op->in_opr1(), op->in_opr2(), op);
  } else {
    assert(op->in_opr2()->is_illegal(), "both operands must be illegal");
    assert(op->condition() == lir_cond_always, "no other conditions allowed");
  }

  Label ok;
  if (op->condition() != lir_cond_always) {
    Assembler::Condition acond = Assembler::AL;
    switch (op->condition()) {
      case lir_cond_equal:        acond = Assembler::EQ;  break;
      case lir_cond_notEqual:     acond = Assembler::NE;  break;
      case lir_cond_less:         acond = Assembler::LT;  break;
      case lir_cond_lessEqual:    acond = Assembler::LE;  break;
      case lir_cond_greaterEqual: acond = Assembler::GE;  break;
      case lir_cond_greater:      acond = Assembler::GT;  break;
      case lir_cond_belowEqual:   acond = Assembler::LS;  break;
      case lir_cond_aboveEqual:   acond = Assembler::HS;  break;
      default:                    ShouldNotReachHere();
    }
    __ br(acond, ok);
  }
  if (op->halt()) {
    const char* str = __ code_string(op->msg());
    __ stop(str);
  } else {
    breakpoint();
  }
  __ bind(ok);
}
#endif

#ifndef PRODUCT
#define COMMENT(x)   do { __ block_comment(x); } while (0)
#else
#define COMMENT(x)
#endif

void LIR_Assembler::membar() {
  COMMENT("membar");
  __ membar(MacroAssembler::AnyAny);
}

void LIR_Assembler::membar_acquire() {
  __ block_comment("membar_acquire");
}

void LIR_Assembler::membar_release() {
  __ block_comment("membar_release");
}

void LIR_Assembler::membar_loadload() { Unimplemented(); }

void LIR_Assembler::membar_storestore() {
  COMMENT("membar_storestore");
  __ membar(MacroAssembler::StoreStore);
}

void LIR_Assembler::membar_loadstore() { Unimplemented(); }

void LIR_Assembler::membar_storeload() { Unimplemented(); }

void LIR_Assembler::get_thread(LIR_Opr result_reg) { Unimplemented(); }


void LIR_Assembler::peephole(LIR_List *lir) {
  if (tableswitch_count >= max_tableswitches)
    return;

  /*
    This finite-state automaton recognizes sequences of compare-and-
    branch instructions.  We will turn them into a tableswitch.  You
    could argue that C1 really shouldn't be doing this sort of
    optimization, but without it the code is really horrible.
  */

  enum { start_s, cmp1_s, beq_s, cmp_s } state;
  int first_key, last_key = -2147483648;
  int next_key = 0;
  int start_insn = -1;
  int last_insn = -1;
  Register reg = noreg;
  LIR_Opr reg_opr;
  state = start_s;

  LIR_OpList* inst = lir->instructions_list();
  for (int i = 0; i < inst->length(); i++) {
    LIR_Op* op = inst->at(i);
    switch (state) {
    case start_s:
      first_key = -1;
      start_insn = i;
      switch (op->code()) {
      case lir_cmp:
	LIR_Opr opr1 = op->as_Op2()->in_opr1();
	LIR_Opr opr2 = op->as_Op2()->in_opr2();
	if (opr1->is_cpu_register() && opr1->is_single_cpu()
	    && opr2->is_constant()
	    && opr2->type() == T_INT) {
	  reg_opr = opr1;
	  reg = opr1->as_register();
	  first_key = opr2->as_constant_ptr()->as_jint();
	  next_key = first_key + 1;
	  state = cmp_s;
	  goto next_state;
	}
	break;
      }
      break;
    case cmp_s:
      switch (op->code()) {
      case lir_branch:
	if (op->as_OpBranch()->cond() == lir_cond_equal) {
	  state = beq_s;
	  last_insn = i;
	  goto next_state;
	}
      }
      state = start_s;
      break;
    case beq_s:
      switch (op->code()) {
      case lir_cmp: {
	LIR_Opr opr1 = op->as_Op2()->in_opr1();
	LIR_Opr opr2 = op->as_Op2()->in_opr2();
	if (opr1->is_cpu_register() && opr1->is_single_cpu()
	    && opr1->as_register() == reg
	    && opr2->is_constant()
	    && opr2->type() == T_INT
	    && opr2->as_constant_ptr()->as_jint() == next_key) {
	  last_key = next_key;
	  next_key++;
	  state = cmp_s;
	  goto next_state;
	}
      }
      }
      last_key = next_key;
      state = start_s;
      break;
    default:
      assert(false, "impossible state");
    }
    if (state == start_s) {
      if (first_key < last_key - 5L && reg != noreg) {
	{
	  // printf("found run register %d starting at insn %d low value %d high value %d\n",
	  //        reg->encoding(),
	  //        start_insn, first_key, last_key);
	  //   for (int i = 0; i < inst->length(); i++) {
	  //     inst->at(i)->print();
	  //     tty->print("\n");
	  //   }
	  //   tty->print("\n");
	}

	struct tableswitch *sw = &switches[tableswitch_count];
	sw->_insn_index = start_insn, sw->_first_key = first_key,
	  sw->_last_key = last_key, sw->_reg = reg;
	inst->insert_before(last_insn + 1, new LIR_OpLabel(&sw->_after));
	{
	  // Insert the new table of branches
	  int offset = last_insn;
	  for (int n = first_key; n < last_key; n++) {
	    inst->insert_before
	      (last_insn + 1,
	       new LIR_OpBranch(lir_cond_always, T_ILLEGAL,
				inst->at(offset)->as_OpBranch()->label()));
	    offset -= 2, i++;
	  }
	}
	// Delete all the old compare-and-branch instructions
	for (int n = first_key; n < last_key; n++) {
	  inst->remove_at(start_insn);
	  inst->remove_at(start_insn);
	}
	// Insert the tableswitch instruction
	inst->insert_before(start_insn,
			    new LIR_Op2(lir_cmp, lir_cond_always,
					LIR_OprFact::intConst(tableswitch_count),
					reg_opr));
	inst->insert_before(start_insn + 1, new LIR_OpLabel(&sw->_branches));
	tableswitch_count++;
      }
      reg = noreg;
      last_key = -2147483648;
    }
  next_state:
    ;
  }
}

void LIR_Assembler::atomic_op(LIR_Code code, LIR_Opr src, LIR_Opr data, LIR_Opr dest, LIR_Opr tmp_op) {
  Address addr = as_Address(src->as_address_ptr(), noreg);
  BasicType type = src->type();
  bool is_oop = type == T_OBJECT || type == T_ARRAY;

  void (MacroAssembler::* lda)(Register Rd, Register Ra);
  void (MacroAssembler::* add)(Register Rd, Register Rn, RegisterOrConstant increment);
  void (MacroAssembler::* stl)(Register Rs, Register Rt, Register Rn);

  switch(type) {
  case T_INT:
    lda = &MacroAssembler::ldaxrw;
    add = &MacroAssembler::addw;
    stl = &MacroAssembler::stlxrw;
    break;
  case T_LONG:
    lda = &MacroAssembler::ldaxr;
    add = &MacroAssembler::add;
    stl = &MacroAssembler::stlxr;
    break;
  case T_OBJECT:
  case T_ARRAY:
    if (UseCompressedOops) {
      lda = &MacroAssembler::ldaxrw;
      add = &MacroAssembler::addw;
      stl = &MacroAssembler::stlxrw;
    } else {
      lda = &MacroAssembler::ldaxr;
      add = &MacroAssembler::add;
      stl = &MacroAssembler::stlxr;
    }
    break;
  default:
    ShouldNotReachHere();
  }

  switch (code) {
  case lir_xadd:
    {
      RegisterOrConstant inc;
      Register tmp = as_reg(tmp_op);
      Register dst = as_reg(dest);
      if (data->is_constant()) {
	inc = RegisterOrConstant(as_long(data));
	assert_different_registers(dst, addr.base(), tmp,
				   rscratch1, rscratch2);
      } else {
	inc = RegisterOrConstant(as_reg(data));
	assert_different_registers(inc.as_register(), dst, addr.base(), tmp,
				   rscratch1, rscratch2);
      }
      Label again;
      __ lea(tmp, addr);
      __ bind(again);
      (_masm->*lda)(dst, tmp);
      (_masm->*add)(rscratch1, dst, inc);
      (_masm->*stl)(rscratch2, rscratch1, tmp);
      __ cbnzw(rscratch2, again);
      break;
    }
  case lir_xchg:
    {
      Register tmp = tmp_op->as_register();
      Register obj = as_reg(data);
      Register dst = as_reg(dest);
      if (is_oop && UseCompressedOops) {
	__ encode_heap_oop(obj);
      }
      assert_different_registers(obj, addr.base(), tmp, rscratch2, dst);
      Label again;
      __ lea(tmp, addr);
      __ bind(again);
      (_masm->*lda)(dst, tmp);
      (_masm->*stl)(rscratch2, obj, tmp);
      __ cbnzw(rscratch2, again);
      if (is_oop && UseCompressedOops) {
	__ decode_heap_oop(dst);
      }
    }
    break;
  default:
    ShouldNotReachHere();
  }
  asm("nop");
}

#undef __
