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

static inline Address iaddress(Register r) {
  return Address(rlocals, r, Address::lsl(3));
}

static inline Address lAddress(int n)
{
  Unimplemented();
  return (Address)0;
}

static inline Address fAddress(int n)
{
  Unimplemented();
  return (Address)0;
}

static inline Address dAddress(int n)
{
  Unimplemented();
  return (Address)0;
}

static inline Address aAddress(int n)
{
  Unimplemented();
  return (Address)0;
}

static inline Address iAddress(Register r)
{
  Unimplemented();
  return (Address)0;
}

static inline Address lAddress(Register r)
{
  Unimplemented();
  return (Address)0;
}

static inline Address fAddress(Register r)
{
  Unimplemented();
  return (Address)0;
}

static inline Address dAddress(Register r)
{
  Unimplemented();
  return (Address)0;
}

static inline Address aAddress(Register r)
{
  Unimplemented();
  return (Address)0;
}

static inline Address at_rsp()
{
  Unimplemented();
  return (Address)0;
}

// At top of Java expression stack which may be different than esp().  It
// isn't for category 1 objects.
static inline Address at_tos   ()
{
  Unimplemented();
  return (Address)0;
}

static inline Address at_tos_p1()
{
  Unimplemented();
  return (Address)0;
}

static inline Address at_tos_p2()
{
  Unimplemented();
  return (Address)0;
}

static inline Address at_tos_p3()
{
  Unimplemented();
  return (Address)0;
}

// Condition conversion
static Assembler::Condition j_not(TemplateTable::Condition cc) {
  return Assembler::Condition(cc ^ 1);
}


// Miscelaneous helper routines
// Store an oop (or NULL) at the Address described by obj.
// If val == noreg this means store a NULL

static void do_oop_store(InterpreterMacroAssembler* _masm,
                         Address obj,
                         Register val,
                         BarrierSet::Name barrier,
                         bool precise)
{
  __ call_Unimplemented();
}

Address TemplateTable::at_bcp(int offset) {
  assert(_desc->uses_bcp(), "inconsistent uses_bcp information");
  return Address(rbcp, offset);
}

void TemplateTable::patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                                   Register temp_reg, bool load_bc_into_bc_reg/*=true*/,
                                   int byte_no)
{
  __ call_Unimplemented();
}


// Individual instructions

void TemplateTable::nop() {
  transition(vtos, vtos);
  // nothing to do
}

void TemplateTable::shouldnotreachhere() {
  transition(vtos, vtos);
  __ stop("shouldnotreachhere bytecode");
}

void TemplateTable::aconst_null()
{
  transition(vtos, atos);
  __ mov(r0, 0);
}

void TemplateTable::iconst(int value)
{
  transition(vtos, itos);
  __ mov(r0, value);
}

void TemplateTable::lconst(int value)
{
  __ mov(r0, value);
}

void TemplateTable::fconst(int value)
{
  transition(vtos, ftos);
  switch (value) {
  case 0:
    __ fmovs(v0, zr);
    break;
  case 1:
    __ fmovs(v0, 1.0);
    break;
  case 2:
    __ fmovs(v0, 2.0);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void TemplateTable::dconst(int value)
{
  transition(vtos, dtos);
  switch (value) {
  case 0:
    __ fmovd(v0, zr);
    break;
  case 1:
    __ fmovd(v0, 1.0);
    break;
  case 2:
    __ fmovd(v0, 2.0);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void TemplateTable::bipush()
{
  transition(vtos, itos);
  __ load_signed_byte(r0, at_bcp(1));
}

void TemplateTable::sipush()
{
  transition(vtos, itos);
  // FIXME: Is this the right way to do it?
  __ load_unsigned_short(r0, at_bcp(1));
  __ rev(r0, r0);
  __ asrw(r0, r0, 48);
}

void TemplateTable::ldc(bool wide)
{
  transition(vtos, vtos);
  Label call_ldc, notFloat, notClass, Done;

  if (wide) {
    __ get_unsigned_2_byte_index_at_bcp(r1, 1);
  } else {
    __ load_unsigned_byte(r1, at_bcp(1));
  }
  __ get_cpool_and_tags(r2, r0);

  const int base_offset = constantPoolOopDesc::header_size() * wordSize;
  const int tags_offset = typeArrayOopDesc::header_size(T_BYTE) * wordSize;

  // get type
  __ add(r1, r1, tags_offset);
  __ ldrb(r3, Address(r0, r1));

  // unresolved string - get the resolved string
  __ cmp(r3, JVM_CONSTANT_UnresolvedString);
  __ br(Assembler::EQ, call_ldc);

  // unresolved class - get the resolved class
  __ cmp(r3, JVM_CONSTANT_UnresolvedClass);
  __ br(Assembler::EQ, call_ldc);

  // unresolved class in error state - call into runtime to throw the error
  // from the first resolution attempt
  __ cmp(r3, JVM_CONSTANT_UnresolvedClassInError);
  __ br(Assembler::EQ, call_ldc);

  // resolved class - need to call vm to get java mirror of the class
  __ cmp(r3, JVM_CONSTANT_Class);
  __ br(Assembler::NE, notClass);

  __ bind(call_ldc);
  __ mov(c_rarg1, wide);
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::ldc), c_rarg1);
  __ push_ptr(r0);
  __ verify_oop(r0);
  __ b(Done);

  __ bind(notFloat);
#ifdef ASSERT
  {
    Label L;
    __ cmp(r3, JVM_CONSTANT_Integer);
    __ br(Assembler::EQ, L);
    __ cmp(r3, JVM_CONSTANT_String);
    __ br(Assembler::EQ, L);
    __ cmp(r3, JVM_CONSTANT_Object);
    __ br(Assembler::EQ, L);
    __ stop("unexpected tag type in ldc");
    __ bind(L);
  }
#endif
  // atos and itos
  Label isOop;
  __ cmp(r3, JVM_CONSTANT_Integer);
  __ br(Assembler::NE, isOop);
  __ add(r1, r1, base_offset);
  __ ldr(r0, Address(r2, r1, Address::lsl(3)));
  __ push_i(r0);
  __ b(Done);

  __ bind(isOop);
  __ add(r1, r1, base_offset);
  __ ldr(r0, Address(r2, r1, Address::lsl(3)));
  __ push_ptr(r0);

  if (VerifyOops) {
    __ verify_oop(r0);
  }

  __ bind(Done);
}

// Fast path for caching oop constants.
// %%% We should use this to handle Class and String constants also.
// %%% It will simplify the ldc/primitive path considerably.
void TemplateTable::fast_aldc(bool wide)
{
  __ call_Unimplemented();
}

void TemplateTable::ldc2_w()
{
  __ call_Unimplemented();
}

void TemplateTable::locals_index(Register reg, int offset)
{
  __ call_Unimplemented();
}

void TemplateTable::iload()
{
  __ call_Unimplemented();
}

void TemplateTable::fast_iload2()
{
  __ call_Unimplemented();
}

void TemplateTable::fast_iload()
{
  __ call_Unimplemented();
}

void TemplateTable::lload()
{
  __ call_Unimplemented();
}

void TemplateTable::fload()
{
  __ call_Unimplemented();
}

void TemplateTable::dload()
{
  __ call_Unimplemented();
}

void TemplateTable::aload()
{
  __ call_Unimplemented();
}

void TemplateTable::locals_index_wide(Register reg) {
  __ ldrh(reg, at_bcp(2));
  __ rev16w(reg, reg);
  __ neg(reg, reg);
}

void TemplateTable::wide_iload() {
  transition(vtos, itos);
  locals_index_wide(r1);
  __ ldr(r0, iaddress(r1));
}

void TemplateTable::wide_lload()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_fload()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_dload()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_aload()
{
  __ call_Unimplemented();
}

void TemplateTable::index_check(Register array, Register index)
{
  __ call_Unimplemented();
}

void TemplateTable::iaload()
{
  __ call_Unimplemented();
}

void TemplateTable::laload()
{
  __ call_Unimplemented();
}

void TemplateTable::faload()
{
  __ call_Unimplemented();
}

void TemplateTable::daload()
{
  __ call_Unimplemented();
}

void TemplateTable::aaload()
{
  __ call_Unimplemented();
}

void TemplateTable::baload()
{
  __ call_Unimplemented();
}

void TemplateTable::caload()
{
  __ call_Unimplemented();
}

// iload followed by caload frequent pair
void TemplateTable::fast_icaload()
{
  __ call_Unimplemented();
}

void TemplateTable::saload()
{
  __ call_Unimplemented();
}

void TemplateTable::iload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::lload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::fload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::dload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::aload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::aload_0()
{
  __ call_Unimplemented();
}

void TemplateTable::istore()
{
  __ call_Unimplemented();
}

void TemplateTable::lstore()
{
  __ call_Unimplemented();
}

void TemplateTable::fstore()
{
  __ call_Unimplemented();
}

void TemplateTable::dstore()
{
  __ call_Unimplemented();
}

void TemplateTable::astore()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_istore()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_lstore()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_fstore()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_dstore()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_astore()
{
  __ call_Unimplemented();
}

void TemplateTable::iastore()
{
  __ call_Unimplemented();
}

void TemplateTable::lastore()
{
  __ call_Unimplemented();
}

void TemplateTable::fastore()
{
  __ call_Unimplemented();
}

void TemplateTable::dastore()
{
  __ call_Unimplemented();
}

void TemplateTable::aastore()
{
  __ call_Unimplemented();
}

void TemplateTable::bastore()
{
  __ call_Unimplemented();
}

void TemplateTable::castore()
{
  __ call_Unimplemented();
}

void TemplateTable::sastore()
{
  __ call_Unimplemented();
}

void TemplateTable::istore(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::lstore(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::fstore(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::dstore(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::astore(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::pop()
{
  __ call_Unimplemented();
}

void TemplateTable::pop2()
{
  __ call_Unimplemented();
}

void TemplateTable::dup()
{
  __ call_Unimplemented();
}

void TemplateTable::dup_x1()
{
  __ call_Unimplemented();
}

void TemplateTable::dup_x2()
{
  __ call_Unimplemented();
}

void TemplateTable::dup2()
{
  __ call_Unimplemented();
}

void TemplateTable::dup2_x1()
{
  __ call_Unimplemented();
}

void TemplateTable::dup2_x2()
{
  __ call_Unimplemented();
}

void TemplateTable::swap()
{
  __ call_Unimplemented();
}

void TemplateTable::iop2(Operation op)
{
  __ call_Unimplemented();
}

void TemplateTable::lop2(Operation op)
{
  __ call_Unimplemented();
}

void TemplateTable::idiv()
{
  __ call_Unimplemented();
}

void TemplateTable::irem()
{
  __ call_Unimplemented();
}

void TemplateTable::lmul()
{
  __ call_Unimplemented();
}

void TemplateTable::ldiv()
{
  __ call_Unimplemented();
}

void TemplateTable::lrem()
{
  __ call_Unimplemented();
}

void TemplateTable::lshl()
{
  __ call_Unimplemented();
}

void TemplateTable::lshr()
{
  __ call_Unimplemented();
}

void TemplateTable::lushr()
{
  __ call_Unimplemented();
}

void TemplateTable::fop2(Operation op)
{
  __ call_Unimplemented();
}

void TemplateTable::dop2(Operation op)
{
  __ call_Unimplemented();
}

void TemplateTable::ineg()
{
  __ call_Unimplemented();
}

void TemplateTable::lneg()
{
  __ call_Unimplemented();
}

void TemplateTable::fneg()
{
  __ call_Unimplemented();
}

void TemplateTable::dneg()
{
  __ call_Unimplemented();
}

void TemplateTable::iinc()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_iinc()
{
  __ call_Unimplemented();
}

void TemplateTable::convert()
{
  __ call_Unimplemented();
}

void TemplateTable::lcmp()
{
  __ call_Unimplemented();
}

void TemplateTable::float_cmp(bool is_float, int unordered_result)
{
  __ call_Unimplemented();
}

void TemplateTable::branch(bool is_jsr, bool is_wide)
{
  __ call_Unimplemented();
}


void TemplateTable::if_0cmp(Condition cc)
{
  __ call_Unimplemented();
}

void TemplateTable::if_icmp(Condition cc)
{
  __ call_Unimplemented();
}

void TemplateTable::if_nullcmp(Condition cc)
{
  __ call_Unimplemented();
}

void TemplateTable::if_acmp(Condition cc)
{
  __ call_Unimplemented();
}

void TemplateTable::ret()
{
  __ call_Unimplemented();
}

void TemplateTable::wide_ret()
{
  __ call_Unimplemented();
}

void TemplateTable::tableswitch()
{
  __ call_Unimplemented();
}

void TemplateTable::lookupswitch()
{
  __ call_Unimplemented();
}

void TemplateTable::fast_linearswitch()
{
  __ call_Unimplemented();
}

void TemplateTable::fast_binaryswitch()
{
  __ call_Unimplemented();
}


void TemplateTable::_return(TosState state)
{
  transition(state, state);
  assert(_desc->calls_vm(),
         "inconsistent calls_vm information"); // call in remove_activation

  if (_desc->bytecode() == Bytecodes::_return_register_finalizer) {
    __ call_Unimplemented();
#if 0
    assert(state == vtos, "only valid state");
    __ movptr(c_rarg1, aaddress(0));
    __ load_klass(rdi, c_rarg1);
    __ movl(rdi, Address(rdi, Klass::access_flags_offset()));
    __ testl(rdi, JVM_ACC_HAS_FINALIZER);
    Label skip_register_finalizer;
    __ jcc(Assembler::zero, skip_register_finalizer);

    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::register_finalizer), c_rarg1);

    __ bind(skip_register_finalizer);
#endif
  }

  __ remove_activation(state);
  __ ret(lr);
}

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
//                                      order_constraint)
// {
//   __ call_Unimplemented();
// }


void TemplateTable::resolve_cache_and_index(int byte_no,
                                            Register result,
                                            Register Rcache,
                                            Register index,
                                            size_t index_size) {
  const Register temp = r1;
  assert_different_registers(result, Rcache, index, temp);

  Label resolved;
  if (byte_no == f1_oop) {
    // We are resolved if the f1 field contains a non-null object (CallSite, etc.)
    // This kind of CP cache entry does not need to match the flags byte, because
    // there is a 1-1 relation between bytecode type and CP entry type.
    assert(result != noreg, ""); //else do cmpptr(Address(...), (int32_t) NULL_WORD)
    __ get_cache_and_index_at_bcp(Rcache, index, 1, index_size);
    __ add(rscratch1, Rcache, index, Assembler::LSL, 3);
    __ ldr(result, Address(rscratch1,
			   constantPoolCacheOopDesc::base_offset()
			   + ConstantPoolCacheEntry::f1_offset()));
    __ cbnz(result, resolved);
  } else {
    assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
    assert(result == noreg, "");  //else change code for setting result
    __ get_cache_and_index_and_bytecode_at_bcp(Rcache, index, temp, byte_no, 1, index_size);
    __ cmp(temp, (int) bytecode());  // have we resolved this bytecode?
    __ br(Assembler::EQ, resolved);
  }

  // resolve first time through
  address entry;
  switch (bytecode()) {
  case Bytecodes::_getstatic:
  case Bytecodes::_putstatic:
  case Bytecodes::_getfield:
  case Bytecodes::_putfield:
    entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_get_put);
    break;
  case Bytecodes::_invokevirtual:
  case Bytecodes::_invokespecial:
  case Bytecodes::_invokestatic:
  case Bytecodes::_invokeinterface:
    entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invoke);
    break;
  case Bytecodes::_invokedynamic:
    entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_invokedynamic);
    break;
  case Bytecodes::_fast_aldc:
    entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_ldc);
    break;
  case Bytecodes::_fast_aldc_w:
    entry = CAST_FROM_FN_PTR(address, InterpreterRuntime::resolve_ldc);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
  __ mov(temp, (int) bytecode());
  __ call_VM(noreg, entry, temp);

  // Update registers with resolved info
  __ get_cache_and_index_at_bcp(Rcache, index, 1, index_size);
  if (result != noreg) {
    __ add(rscratch1, Rcache, index, Assembler::LSL, 3);
    __ ldr(result, Address(rscratch1,
			   constantPoolCacheOopDesc::base_offset()
			   + ConstantPoolCacheEntry::f1_offset()));
  }
  __ bind(resolved);
}

// The Rcache and index registers must be set before call
void TemplateTable::load_field_cp_cache_entry(Register obj,
                                              Register cache,
                                              Register index,
                                              Register off,
                                              Register flags,
                                              bool is_static = false)
{
  __ call_Unimplemented();
}

void TemplateTable::load_invoke_cp_cache_entry(int byte_no,
                                               Register method,
                                               Register itable_index,
                                               Register flags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal, /*unused*/
                                               bool is_invokedynamic) {
  // setup registers
  const Register index = r3;
  const Register cache = r2;
  assert_different_registers(method, flags);
  assert_different_registers(method, cache, index);
  assert_different_registers(itable_index, flags);
  assert_different_registers(itable_index, cache, index);
  // determine constant pool cache field offsets
  const int method_offset = in_bytes(
    constantPoolCacheOopDesc::base_offset() +
      (is_invokevirtual
       ? ConstantPoolCacheEntry::f2_offset()
       : ConstantPoolCacheEntry::f1_offset()));
  const int flags_offset = in_bytes(constantPoolCacheOopDesc::base_offset() +
                                    ConstantPoolCacheEntry::flags_offset());
  // access constant pool cache fields
  const int index_offset = in_bytes(constantPoolCacheOopDesc::base_offset() +
                                    ConstantPoolCacheEntry::f2_offset());

  if (byte_no == f1_oop) {
    // Resolved f1_oop goes directly into 'method' register.
    assert(is_invokedynamic, "");
    resolve_cache_and_index(byte_no, method, cache, index, sizeof(u4));
  } else {
    resolve_cache_and_index(byte_no, noreg, cache, index, sizeof(u2));
    __ ldr(method, Address(cache, index, Address::lsl(3)));
    __ add(method, method, method_offset);
  }
  if (itable_index != noreg) {
    __ ldr(itable_index, Address(cache, index, Address::lsl(3)));
    __ add(itable_index, itable_index, index_offset);
  }
  __ ldr(flags, Address(cache, index, Address::lsl(3)));
  __ add(flags,flags,  flags_offset);
}


// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void TemplateTable::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos)
{
  __ call_Unimplemented();
}

void TemplateTable::pop_and_check_object(Register r)
{
  __ call_Unimplemented();
}

void TemplateTable::getfield_or_static(int byte_no, bool is_static)
{
  __ call_Unimplemented();
}


void TemplateTable::getfield(int byte_no)
{
  __ call_Unimplemented();
}

void TemplateTable::getstatic(int byte_no)
{
  __ call_Unimplemented();
}

// The registers cache and index expected to be set before call.
// The function may destroy various registers, just not the cache and index registers.
void TemplateTable::jvmti_post_field_mod(Register cache, Register index, bool is_static)
{
  __ call_Unimplemented();
}

void TemplateTable::putfield_or_static(int byte_no, bool is_static) {
  transition(vtos, vtos);

  const Register index = r3;
  const Register obj   = r2;
  const Register off   = r1;
  const Register flags = r0;
  const Register bc    = r4;

  resolve_cache_and_index(byte_no, noreg, rcpool, index, sizeof(u2));
  jvmti_post_field_mod(rcpool, index, is_static);
  load_field_cp_cache_entry(obj, rcpool, index, off, flags, is_static);

  // [jk] not needed currently
  // volatile_barrier(Assembler::Membar_mask_bits(Assembler::LoadStore |
  //                                              Assembler::StoreStore));

  Label notVolatile, Done;
  __ mov(r5, flags);

  // field address
  const Address field(obj, off);

  Label notByte, notInt, notShort, notChar,
        notLong, notFloat, notObj, notDouble;

  __ lsr(flags, flags, ConstantPoolCacheEntry::tosBits);

  assert(btos == 0, "change code, btos != 0");
  __ ands(flags, flags, 0x0f);
  __ br(Assembler::NE, notByte);

  // btos
  {
    __ pop(btos);
    if (!is_static) pop_and_check_object(obj);
    __ str(r0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_bputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notByte);
  __ cmp(flags, atos);
  __ br(Assembler::NE, notObj);

  // atos
  {
    __ pop(atos);
    if (!is_static) pop_and_check_object(obj);
    // Store into the field
    do_oop_store(_masm, field, r0, _bs->kind(), false);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_aputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notObj);
  __ cmp(flags, itos);
  __ br(Assembler::NE, notInt);

  // itos
  {
    __ pop(itos);
    if (!is_static) pop_and_check_object(obj);
    __ str(r0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_iputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notInt);
  __ cmp(flags, ctos);
  __ br(Assembler::NE, notChar);

  // ctos
  {
    __ pop(ctos);
    if (!is_static) pop_and_check_object(obj);
    __ str(r0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_cputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notChar);
  __ cmp(flags, stos);
  __ br(Assembler::NE, notShort);

  // stos
  {
    __ pop(stos);
    if (!is_static) pop_and_check_object(obj);
    __ str(r0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_sputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notShort);
  __ cmp(flags, ltos);
  __ br(Assembler::NE, notLong);

  // ltos
  {
    __ pop(ltos);
    if (!is_static) pop_and_check_object(obj);
    __ str(r0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_lputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notLong);
  __ cmp(flags, ftos);
  __ br(Assembler::NE, notFloat);

  // ftos
  {
    __ pop(ftos);
    if (!is_static) pop_and_check_object(obj);
    __ strs(v0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_fputfield, bc, r1, true, byte_no);
    }
    __ b(Done);
  }

  __ bind(notFloat);
#ifdef ASSERT
  __ cmp(flags, dtos);
  __ br(Assembler::NE, notDouble);
#endif

  // dtos
  {
    __ pop(dtos);
    if (!is_static) pop_and_check_object(obj);
    __ strd(v0, field);
    if (!is_static) {
      patch_bytecode(Bytecodes::_fast_dputfield, bc, r1, true, byte_no);
    }
  }

#ifdef ASSERT
  __ b(Done);

  __ bind(notDouble);
  __ stop("Bad state");
#endif

  __ bind(Done);

  // Check for volatile store
  __ tbz(r5, ConstantPoolCacheEntry::volatileField, notVolatile);
  __ dmb(Assembler::SY);
  __ bind(notVolatile);
}

void TemplateTable::putfield(int byte_no)
{
  __ call_Unimplemented();
}

void TemplateTable::putstatic(int byte_no) {
  putfield_or_static(byte_no, true);
}

void TemplateTable::jvmti_post_fast_field_mod()
{
  __ call_Unimplemented();
}

void TemplateTable::fast_storefield(TosState state)
{
  __ call_Unimplemented();
}


void TemplateTable::fast_accessfield(TosState state)
{
  __ call_Unimplemented();
}

void TemplateTable::fast_xaccess(TosState state)
{
  __ call_Unimplemented();
}



//-----------------------------------------------------------------------------
// Calls

void TemplateTable::count_calls(Register method, Register temp)
{
  __ call_Unimplemented();
}

void TemplateTable::prepare_invoke(Register method, Register index, int byte_no) {
  // determine flags
  Bytecodes::Code code = bytecode();
  const bool is_invokeinterface  = code == Bytecodes::_invokeinterface;
  const bool is_invokedynamic    = code == Bytecodes::_invokedynamic;
  const bool is_invokevirtual    = code == Bytecodes::_invokevirtual;
  const bool is_invokespecial    = code == Bytecodes::_invokespecial;
  const bool load_receiver      = (code != Bytecodes::_invokestatic && code != Bytecodes::_invokedynamic);
  const bool receiver_null_check = is_invokespecial;
  const bool save_flags = is_invokeinterface || is_invokevirtual;
  // setup registers & access constant pool cache
  const Register recv   = r2;
  const Register flags  = r3;
  assert_different_registers(method, index, recv, flags);

  // save 'interpreter return address'
  __ save_bcp();

  load_invoke_cp_cache_entry(byte_no, method, index, flags, is_invokevirtual, false, is_invokedynamic);

  // load receiver if needed (note: no return address pushed yet)
  if (load_receiver) {
    assert(!is_invokedynamic, "");
    __ andr(recv, flags, 0xFF);
    __ add(rscratch1, sp, recv, Assembler::LSL, 3);
    __ sub(rscratch1, rscratch1, Interpreter::expr_offset_in_bytes(1));
    __ ldr(recv, Address(rscratch1));
    __ verify_oop(recv);
  }

  // do null check if needed
  if (receiver_null_check) {
    __ null_check(recv);
  }

  if (save_flags) {
    __ mov(rscratch2, flags);
  }

  // compute return type
  __ lsr(flags, flags, ConstantPoolCacheEntry::tosBits);
  // Make sure we don't need to mask flags for tosBits after the above shift
  ConstantPoolCacheEntry::verify_tosBits();
  // load return address
  {
    address table_addr;
    if (is_invokeinterface || is_invokedynamic)
      table_addr = (address)Interpreter::return_5_addrs_by_index_table();
    else
      table_addr = (address)Interpreter::return_3_addrs_by_index_table();
    __ mov(rscratch1, table_addr);
    __ ldr(lr, Address(rscratch1, flags, Address::lsl(3)));
  }

  // Restore flag field from the constant pool cache, and restore esi
  // for later null checks.  r13 is the bytecode pointer
  if (save_flags) {
    __ mov(flags, rscratch2);
    __ restore_bcp();  // FIXME: Probably not needed!
  }
}


void TemplateTable::invokevirtual_helper(Register index,
                                         Register recv,
                                         Register flags)
{
  __ call_Unimplemented();
}


void TemplateTable::invokevirtual(int byte_no)
{
  __ call_Unimplemented();
}


void TemplateTable::invokespecial(int byte_no)
{
  __ call_Unimplemented();
}


void TemplateTable::invokestatic(int byte_no)
{
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");
  prepare_invoke(r1, noreg, byte_no);
  // do the call
  __ verify_oop(r1);
  __ profile_call(r0);
  __ jump_from_interpreted(rmethod, r0);
}

void TemplateTable::fast_invokevfinal(int byte_no)
{
  __ call_Unimplemented();
}

void TemplateTable::invokeinterface(int byte_no)
{
  __ call_Unimplemented();
}

void TemplateTable::invokedynamic(int byte_no)
{
  __ call_Unimplemented();
}


//-----------------------------------------------------------------------------
// Allocation

void TemplateTable::_new()
{
  __ call_Unimplemented();
}

void TemplateTable::newarray() {
  transition(itos, atos);
  __ load_unsigned_byte(c_rarg1, at_bcp(1));
  __ mov(c_rarg2, r0);
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::newarray),
          c_rarg1, c_rarg2);
}

void TemplateTable::anewarray() {
  transition(itos, atos);
  __ get_unsigned_2_byte_index_at_bcp(c_rarg2, 1);
  __ get_constant_pool(c_rarg1);
  __ mov(c_rarg3, r0);
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::anewarray),
          c_rarg1, c_rarg2, c_rarg3);
}

void TemplateTable::arraylength() {
  transition(atos, itos);
  __ null_check(r0, arrayOopDesc::length_offset_in_bytes());
  __ ldr(r0, Address(r0, arrayOopDesc::length_offset_in_bytes()));
}

void TemplateTable::checkcast()
{
  __ call_Unimplemented();
}

void TemplateTable::instanceof()
{
  __ call_Unimplemented();
}

//-----------------------------------------------------------------------------
// Breakpoints
void TemplateTable::_breakpoint()
{
  __ call_Unimplemented();
}

//-----------------------------------------------------------------------------
// Exceptions

void TemplateTable::athrow()
{
  __ call_Unimplemented();
}

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
void TemplateTable::monitorenter()
{
  __ call_Unimplemented();
}


void TemplateTable::monitorexit()
{
  __ call_Unimplemented();
}


// Wide instructions
void TemplateTable::wide()
{
  __ call_Unimplemented();
}


// Multi arrays
void TemplateTable::multianewarray()
{
  __ call_Unimplemented();
}
#endif // !CC_INTERP
