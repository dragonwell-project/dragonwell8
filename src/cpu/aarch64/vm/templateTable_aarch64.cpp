/*
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "memory/universe.inline.hpp"
#include "oops/methodDataOop.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"

#ifndef CC_INTERP

#define __ _masm->

// Platform-dependent initialization

void TemplateTable::pd_initialize() {
  // No amd64 specific initialization
}

// Address computation: local variables

static inline Address iaddress(int n) { Unimplemented(); return (address)0; }

static inline Address laddress(int n) { Unimplemented(); return (address)0; }

static inline Address faddress(int n) { Unimplemented(); return (address)0; }

static inline Address daddress(int n) { Unimplemented(); return (address)0; }

static inline Address aaddress(int n) { Unimplemented(); return (address)0; }

static inline Address iaddress(Register r) { Unimplemented(); return (address)0; }

static inline Address laddress(Register r) { Unimplemented(); return (address)0; }

static inline Address faddress(Register r) { Unimplemented(); return (address)0; }

static inline Address daddress(Register r) { Unimplemented(); return (address)0; }

static inline Address aaddress(Register r) { Unimplemented(); return (address)0; }

static inline Address at_rsp() { Unimplemented(); return (address)0; }

// At top of Java expression stack which may be different than esp().  It
// isn't for category 1 objects.
static inline Address at_tos   () { Unimplemented(); return (address)0; }

static inline Address at_tos_p1() { Unimplemented(); return (address)0; }

static inline Address at_tos_p2() { Unimplemented(); return (address)0; }

static inline Address at_tos_p3() { Unimplemented(); return (address)0; }

// Condition conversion
static Assembler::Condition j_not(TemplateTable::Condition cc) { Unimplemented(); return Assembler::dummy; }


// Miscelaneous helper routines
// Store an oop (or NULL) at the address described by obj.
// If val == noreg this means store a NULL

static void do_oop_store(InterpreterMacroAssembler* _masm,
                         Address obj,
                         Register val,
                         BarrierSet::Name barrier,
                         bool precise) { Unimplemented(); }

Address TemplateTable::at_bcp(int offset) { Unimplemented(); return (address)0; }

void TemplateTable::patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                                   Register temp_reg, bool load_bc_into_bc_reg/*=true*/,
                                   int byte_no) { Unimplemented(); }


// Individual instructions

void TemplateTable::nop() {
  transition(vtos, vtos);
  // nothing to do
}

void TemplateTable::shouldnotreachhere() {
  transition(vtos, vtos);
  __ stop("shouldnotreachhere bytecode");
}

void TemplateTable::aconst_null() { Unimplemented(); }

void TemplateTable::iconst(int value) { Unimplemented(); }

void TemplateTable::lconst(int value) { Unimplemented(); }

void TemplateTable::fconst(int value) { Unimplemented(); }

void TemplateTable::dconst(int value) { Unimplemented(); }

void TemplateTable::bipush() { Unimplemented(); }

void TemplateTable::sipush() { Unimplemented(); }

void TemplateTable::ldc(bool wide) { Unimplemented(); }

// Fast path for caching oop constants.
// %%% We should use this to handle Class and String constants also.
// %%% It will simplify the ldc/primitive path considerably.
void TemplateTable::fast_aldc(bool wide) { Unimplemented(); }

void TemplateTable::ldc2_w() { Unimplemented(); }

void TemplateTable::locals_index(Register reg, int offset) { Unimplemented(); }

void TemplateTable::iload() { Unimplemented(); }

void TemplateTable::fast_iload2() { Unimplemented(); }

void TemplateTable::fast_iload() { Unimplemented(); }

void TemplateTable::lload() { Unimplemented(); }

void TemplateTable::fload() { Unimplemented(); }

void TemplateTable::dload() { Unimplemented(); }

void TemplateTable::aload() { Unimplemented(); }

void TemplateTable::locals_index_wide(Register reg) { Unimplemented(); }

void TemplateTable::wide_iload() { Unimplemented(); }

void TemplateTable::wide_lload() { Unimplemented(); }

void TemplateTable::wide_fload() { Unimplemented(); }

void TemplateTable::wide_dload() { Unimplemented(); }

void TemplateTable::wide_aload() { Unimplemented(); }

void TemplateTable::index_check(Register array, Register index) { Unimplemented(); }

void TemplateTable::iaload() { Unimplemented(); }

void TemplateTable::laload() { Unimplemented(); }

void TemplateTable::faload() { Unimplemented(); }

void TemplateTable::daload() { Unimplemented(); }

void TemplateTable::aaload() { Unimplemented(); }

void TemplateTable::baload() { Unimplemented(); }

void TemplateTable::caload() { Unimplemented(); }

// iload followed by caload frequent pair
void TemplateTable::fast_icaload() { Unimplemented(); }

void TemplateTable::saload() { Unimplemented(); }

void TemplateTable::iload(int n) { Unimplemented(); }

void TemplateTable::lload(int n) { Unimplemented(); }

void TemplateTable::fload(int n) { Unimplemented(); }

void TemplateTable::dload(int n) { Unimplemented(); }

void TemplateTable::aload(int n) { Unimplemented(); }

void TemplateTable::aload_0() { Unimplemented(); }

void TemplateTable::istore() { Unimplemented(); }

void TemplateTable::lstore() { Unimplemented(); }

void TemplateTable::fstore() { Unimplemented(); }

void TemplateTable::dstore() { Unimplemented(); }

void TemplateTable::astore() { Unimplemented(); }

void TemplateTable::wide_istore() { Unimplemented(); }

void TemplateTable::wide_lstore() { Unimplemented(); }

void TemplateTable::wide_fstore() { Unimplemented(); }

void TemplateTable::wide_dstore() { Unimplemented(); }

void TemplateTable::wide_astore() { Unimplemented(); }

void TemplateTable::iastore() { Unimplemented(); }

void TemplateTable::lastore() { Unimplemented(); }

void TemplateTable::fastore() { Unimplemented(); }

void TemplateTable::dastore() { Unimplemented(); }

void TemplateTable::aastore() { Unimplemented(); }

void TemplateTable::bastore() { Unimplemented(); }

void TemplateTable::castore() { Unimplemented(); }

void TemplateTable::sastore() { Unimplemented(); }

void TemplateTable::istore(int n) { Unimplemented(); }

void TemplateTable::lstore(int n) { Unimplemented(); }

void TemplateTable::fstore(int n) { Unimplemented(); }

void TemplateTable::dstore(int n) { Unimplemented(); }

void TemplateTable::astore(int n) { Unimplemented(); }

void TemplateTable::pop() { Unimplemented(); }

void TemplateTable::pop2() { Unimplemented(); }

void TemplateTable::dup() { Unimplemented(); }

void TemplateTable::dup_x1() { Unimplemented(); }

void TemplateTable::dup_x2() { Unimplemented(); }

void TemplateTable::dup2() { Unimplemented(); }

void TemplateTable::dup2_x1() { Unimplemented(); }

void TemplateTable::dup2_x2() { Unimplemented(); }

void TemplateTable::swap() { Unimplemented(); }

void TemplateTable::iop2(Operation op) { Unimplemented(); }

void TemplateTable::lop2(Operation op) { Unimplemented(); }

void TemplateTable::idiv() { Unimplemented(); }

void TemplateTable::irem() { Unimplemented(); }

void TemplateTable::lmul() { Unimplemented(); }

void TemplateTable::ldiv() { Unimplemented(); }

void TemplateTable::lrem() { Unimplemented(); }

void TemplateTable::lshl() { Unimplemented(); }

void TemplateTable::lshr() { Unimplemented(); }

void TemplateTable::lushr() { Unimplemented(); }

void TemplateTable::fop2(Operation op) { Unimplemented(); }

void TemplateTable::dop2(Operation op) { Unimplemented(); }

void TemplateTable::ineg() { Unimplemented(); }

void TemplateTable::lneg() { Unimplemented(); }

// Note: 'double' and 'long long' have 32-bits alignment on x86.
static jlong* double_quadword(jlong *adr, jlong lo, jlong hi) { Unimplemented(); }

void TemplateTable::fneg() { Unimplemented(); }

void TemplateTable::dneg() { Unimplemented(); }

void TemplateTable::iinc() { Unimplemented(); }

void TemplateTable::wide_iinc() { Unimplemented(); }

void TemplateTable::convert() { Unimplemented(); }

void TemplateTable::lcmp() { Unimplemented(); }

void TemplateTable::float_cmp(bool is_float, int unordered_result) { Unimplemented(); }

void TemplateTable::branch(bool is_jsr, bool is_wide) { Unimplemented(); }


void TemplateTable::if_0cmp(Condition cc) { Unimplemented(); }

void TemplateTable::if_icmp(Condition cc) { Unimplemented(); }

void TemplateTable::if_nullcmp(Condition cc) { Unimplemented(); }

void TemplateTable::if_acmp(Condition cc) { Unimplemented(); }

void TemplateTable::ret() { Unimplemented(); }

void TemplateTable::wide_ret() { Unimplemented(); }

void TemplateTable::tableswitch() { Unimplemented(); }

void TemplateTable::lookupswitch() { Unimplemented(); }

void TemplateTable::fast_linearswitch() { Unimplemented(); }

void TemplateTable::fast_binaryswitch() { Unimplemented(); }


void TemplateTable::_return(TosState state) { Unimplemented(); }

// ----------------------------------------------------------------------------
// Volatile variables demand their effects be made known to all CPU's
// in order.  Store buffers on most chips allow reads & writes to
// reorder; the JMM's ReadAfterWrite.java test fails in -Xint mode
// without some kind of memory barrier (i.e., it's not sufficient that
// the interpreter does not reorder volatile references, the hardware
// also must not reorder them).
//
// According to the new Java Memory Model (JMM):
// (1) All volatiles are serialized wrt to each other.  ALSO reads &
//     writes act as aquire & release, so:
// (2) A read cannot let unrelated NON-volatile memory refs that
//     happen after the read float up to before the read.  It's OK for
//     non-volatile memory refs that happen before the volatile read to
//     float down below it.
// (3) Similar a volatile write cannot let unrelated NON-volatile
//     memory refs that happen BEFORE the write float down to after the
//     write.  It's OK for non-volatile memory refs that happen after the
//     volatile write to float up before it.
//
// We only put in barriers around volatile refs (they are expensive),
// not _between_ memory refs (that would require us to track the
// flavor of the previous memory refs).  Requirements (2) and (3)
// require some barriers before volatile stores and after volatile
// loads.  These nearly cover requirement (1) but miss the
// volatile-store-volatile-load case.  This final case is placed after
// volatile-stores although it could just as well go before
// volatile-loads.

// void TemplateTable::volatile_barrier(Assembler::Membar_mask_bits
//                                      order_constraint) { Unimplemented(); }

void TemplateTable::resolve_cache_and_index(int byte_no,
                                            Register result,
                                            Register Rcache,
                                            Register index,
                                            size_t index_size) { Unimplemented(); }

// The Rcache and index registers must be set before call
void TemplateTable::load_field_cp_cache_entry(Register obj,
                                              Register cache,
                                              Register index,
                                              Register off,
                                              Register flags,
                                              bool is_static = false) { Unimplemented(); }

void TemplateTable::load_invoke_cp_cache_entry(int byte_no,
                                               Register method,
                                               Register itable_index,
                                               Register flags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal, /*unused*/
                                               bool is_invokedynamic) { Unimplemented(); }


// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void TemplateTable::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos) { Unimplemented(); }

void TemplateTable::pop_and_check_object(Register r) { Unimplemented(); }

void TemplateTable::getfield_or_static(int byte_no, bool is_static) { Unimplemented(); }


void TemplateTable::getfield(int byte_no) { Unimplemented(); }

void TemplateTable::getstatic(int byte_no) { Unimplemented(); }

// The registers cache and index expected to be set before call.
// The function may destroy various registers, just not the cache and index registers.
void TemplateTable::jvmti_post_field_mod(Register cache, Register index, bool is_static) { Unimplemented(); }

void TemplateTable::putfield_or_static(int byte_no, bool is_static) { Unimplemented(); }

void TemplateTable::putfield(int byte_no) { Unimplemented(); }

void TemplateTable::putstatic(int byte_no) { Unimplemented(); }

void TemplateTable::jvmti_post_fast_field_mod() { Unimplemented(); }

void TemplateTable::fast_storefield(TosState state) { Unimplemented(); }


void TemplateTable::fast_accessfield(TosState state) { Unimplemented(); }

void TemplateTable::fast_xaccess(TosState state) { Unimplemented(); }



//-----------------------------------------------------------------------------
// Calls

void TemplateTable::count_calls(Register method, Register temp) { Unimplemented(); }

void TemplateTable::prepare_invoke(Register method, Register index, int byte_no) { Unimplemented(); }


void TemplateTable::invokevirtual_helper(Register index,
                                         Register recv,
                                         Register flags) { Unimplemented(); }


void TemplateTable::invokevirtual(int byte_no) { Unimplemented(); }


void TemplateTable::invokespecial(int byte_no) { Unimplemented(); }


void TemplateTable::invokestatic(int byte_no) { Unimplemented(); }

void TemplateTable::fast_invokevfinal(int byte_no) { Unimplemented(); }

void TemplateTable::invokeinterface(int byte_no) { Unimplemented(); }

void TemplateTable::invokedynamic(int byte_no) { Unimplemented(); }


//-----------------------------------------------------------------------------
// Allocation

void TemplateTable::_new() { Unimplemented(); }

void TemplateTable::newarray() { Unimplemented(); }

void TemplateTable::anewarray() { Unimplemented(); }

void TemplateTable::arraylength() { Unimplemented(); }

void TemplateTable::checkcast() { Unimplemented(); }

void TemplateTable::instanceof() { Unimplemented(); }

//-----------------------------------------------------------------------------
// Breakpoints
void TemplateTable::_breakpoint() { Unimplemented(); }

//-----------------------------------------------------------------------------
// Exceptions

void TemplateTable::athrow() { Unimplemented(); }

//-----------------------------------------------------------------------------
// Synchronization
//
// Note: monitorenter & exit are symmetric routines; which is reflected
//       in the assembly code structure as well
//
// Stack layout:
//
// [expressions  ] <--- rsp               = expression stack top
// ..
// [expressions  ]
// [monitor entry] <--- monitor block top = expression stack bot
// ..
// [monitor entry]
// [frame data   ] <--- monitor block bot
// ...
// [saved rbp    ] <--- rbp
void TemplateTable::monitorenter() { Unimplemented(); }


void TemplateTable::monitorexit() { Unimplemented(); }


// Wide instructions
void TemplateTable::wide() { Unimplemented(); }


// Multi arrays
void TemplateTable::multianewarray() { Unimplemented(); }
#endif // !CC_INTERP
