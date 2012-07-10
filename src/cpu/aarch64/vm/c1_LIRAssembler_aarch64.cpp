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


LIR_Opr LIR_Assembler::receiverOpr() { Unimplemented(); return 0; }

LIR_Opr LIR_Assembler::osrBufferPointer() { Unimplemented(); return 0; }

//--------------fpu register translations-----------------------


address LIR_Assembler::float_constant(float f) { Unimplemented(); return 0; }


address LIR_Assembler::double_constant(double d) { Unimplemented(); return 0; }


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

Address LIR_Assembler::as_Address(LIR_Address* addr) { Unimplemented(); return (address)0; }

Address LIR_Assembler::as_Address(LIR_Address* addr, Register tmp) { Unimplemented(); return (address)0; }


Address LIR_Assembler::as_Address_hi(LIR_Address* addr) { Unimplemented(); return (address)0; }


Address LIR_Assembler::as_Address_lo(LIR_Address* addr) { Unimplemented(); return (address)0; }


void LIR_Assembler::osr_entry() { Unimplemented(); }


// inline cache check; done before the frame is built.
int LIR_Assembler::check_icache() { Unimplemented(); return 0; }


void LIR_Assembler::jobject2reg_with_patching(Register reg, CodeEmitInfo* info) { Unimplemented(); }


// This specifies the rsp decrement needed to build the frame
int LIR_Assembler::initial_frame_size_in_bytes() { Unimplemented(); return 0; }


int LIR_Assembler::emit_exception_handler() { Unimplemented(); return 0; }


// Emit the code to remove the frame from the stack in the exception
// unwind path.
int LIR_Assembler::emit_unwind_handler() { Unimplemented(); return 0; }


int LIR_Assembler::emit_deopt_handler() { Unimplemented(); return 0; }


// This is the fast version of java.lang.String.compare; it has not
// OSR-entry and therefore, we generate a slow version for OSR's
void LIR_Assembler::emit_string_compare(LIR_Opr arg0, LIR_Opr arg1, LIR_Opr dst, CodeEmitInfo* info) { Unimplemented(); }



void LIR_Assembler::return_op(LIR_Opr result) { Unimplemented(); }


int LIR_Assembler::safepoint_poll(LIR_Opr tmp, CodeEmitInfo* info) { Unimplemented(); return 0; }


void LIR_Assembler::move_regs(Register from_reg, Register to_reg) { Unimplemented(); }

void LIR_Assembler::swap_reg(Register a, Register b) { Unimplemented(); }


void LIR_Assembler::const2reg(LIR_Opr src, LIR_Opr dest, LIR_PatchCode patch_code, CodeEmitInfo* info) { Unimplemented(); }

void LIR_Assembler::const2stack(LIR_Opr src, LIR_Opr dest) { Unimplemented(); }

void LIR_Assembler::const2mem(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info, bool wide) { Unimplemented(); }


void LIR_Assembler::reg2reg(LIR_Opr src, LIR_Opr dest) { Unimplemented(); }

void LIR_Assembler::reg2stack(LIR_Opr src, LIR_Opr dest, BasicType type, bool pop_fpu_stack) { Unimplemented(); }


void LIR_Assembler::reg2mem(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code, CodeEmitInfo* info, bool pop_fpu_stack, bool wide, bool /* unaligned */) { Unimplemented(); }


void LIR_Assembler::stack2reg(LIR_Opr src, LIR_Opr dest, BasicType type) { Unimplemented(); }


void LIR_Assembler::stack2stack(LIR_Opr src, LIR_Opr dest, BasicType type) { Unimplemented(); }


void LIR_Assembler::mem2reg(LIR_Opr src, LIR_Opr dest, BasicType type, LIR_PatchCode patch_code, CodeEmitInfo* info, bool wide, bool /* unaligned */) { Unimplemented(); }


void LIR_Assembler::prefetchr(LIR_Opr src) { Unimplemented(); }


void LIR_Assembler::prefetchw(LIR_Opr src) { Unimplemented(); }


NEEDS_CLEANUP; // This could be static?
Address::ScaleFactor LIR_Assembler::array_element_size(BasicType type) const { Unimplemented(); return Address::times_4; }


void LIR_Assembler::emit_op3(LIR_Op3* op) { Unimplemented(); }

void LIR_Assembler::emit_opBranch(LIR_OpBranch* op) { Unimplemented(); }

void LIR_Assembler::emit_opConvert(LIR_OpConvert* op) { Unimplemented(); }

void LIR_Assembler::emit_alloc_obj(LIR_OpAllocObj* op) { Unimplemented(); }

void LIR_Assembler::emit_alloc_array(LIR_OpAllocArray* op) { Unimplemented(); }

void LIR_Assembler::type_profile_helper(Register mdo,
                                        ciMethodData *md, ciProfileData *data,
                                        Register recv, Label* update_done) { Unimplemented(); }

void LIR_Assembler::emit_typecheck_helper(LIR_OpTypeCheck *op, Label* success, Label* failure, Label* obj_is_null) { Unimplemented(); }


void LIR_Assembler::emit_opTypeCheck(LIR_OpTypeCheck* op) { Unimplemented(); }


void LIR_Assembler::emit_compare_and_swap(LIR_OpCompareAndSwap* op) { Unimplemented(); }

void LIR_Assembler::cmove(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Opr result, BasicType type) { Unimplemented(); }


void LIR_Assembler::arith_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dest, CodeEmitInfo* info, bool pop_fpu_stack) { Unimplemented(); }

void LIR_Assembler::arith_fpu_implementation(LIR_Code code, int left_index, int right_index, int dest_index, bool pop_fpu_stack) { Unimplemented(); }


void LIR_Assembler::intrinsic_op(LIR_Code code, LIR_Opr value, LIR_Opr unused, LIR_Opr dest, LIR_Op* op) { Unimplemented(); }

void LIR_Assembler::logic_op(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst) { Unimplemented(); }


// we assume that rax, and rdx can be overwritten
void LIR_Assembler::arithmetic_idiv(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr temp, LIR_Opr result, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::comp_op(LIR_Condition condition, LIR_Opr opr1, LIR_Opr opr2, LIR_Op2* op) { Unimplemented(); }

void LIR_Assembler::comp_fl2i(LIR_Code code, LIR_Opr left, LIR_Opr right, LIR_Opr dst, LIR_Op2* op) { Unimplemented(); }


void LIR_Assembler::align_call(LIR_Code code) { Unimplemented(); }


void LIR_Assembler::call(LIR_OpJavaCall* op, relocInfo::relocType rtype) { Unimplemented(); }



void LIR_Assembler::ic_call(LIR_OpJavaCall* op) { Unimplemented(); }


/* Currently, vtable-dispatch is only enabled for sparc platforms */
void LIR_Assembler::vtable_call(LIR_OpJavaCall* op) {
  ShouldNotReachHere();
}


void LIR_Assembler::emit_static_call_stub() { Unimplemented(); }


void LIR_Assembler::throw_op(LIR_Opr exceptionPC, LIR_Opr exceptionOop, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::unwind_op(LIR_Opr exceptionOop) { Unimplemented(); }


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, LIR_Opr count, LIR_Opr dest, LIR_Opr tmp) { Unimplemented(); }


void LIR_Assembler::shift_op(LIR_Code code, LIR_Opr left, jint count, LIR_Opr dest) { Unimplemented(); }


void LIR_Assembler::store_parameter(Register r, int offset_from_rsp_in_words) { Unimplemented(); }


void LIR_Assembler::store_parameter(jint c,     int offset_from_rsp_in_words) { Unimplemented(); }


void LIR_Assembler::store_parameter(jobject o,  int offset_from_rsp_in_words) { Unimplemented(); }


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


void LIR_Assembler::align_backward_branch_target() { Unimplemented(); }


void LIR_Assembler::negate(LIR_Opr left, LIR_Opr dest) { Unimplemented(); }


void LIR_Assembler::leal(LIR_Opr addr, LIR_Opr dest) { Unimplemented(); }


void LIR_Assembler::rt_call(LIR_Opr result, address dest, const LIR_OprList* args, LIR_Opr tmp, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::volatile_move_op(LIR_Opr src, LIR_Opr dest, BasicType type, CodeEmitInfo* info) { Unimplemented(); }


void LIR_Assembler::membar() { Unimplemented(); }

void LIR_Assembler::membar_acquire() { Unimplemented(); }

void LIR_Assembler::membar_release() { Unimplemented(); }

void LIR_Assembler::membar_loadload() { Unimplemented(); }

void LIR_Assembler::membar_storestore() { Unimplemented(); }

void LIR_Assembler::membar_loadstore() { Unimplemented(); }

void LIR_Assembler::membar_storeload() { Unimplemented(); }

void LIR_Assembler::get_thread(LIR_Opr result_reg) { Unimplemented(); }


void LIR_Assembler::peephole(LIR_List*) { Unimplemented(); }


#undef __
