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

static inline Address iaddress(int n) {
  return Address(rlocals, Interpreter::local_offset_in_bytes(n));
}

static inline Address laddress(int n) {
  return iaddress(n + 1);
}

static inline Address faddress(int n) {
  return iaddress(n);
}

static inline Address daddress(int n) {
  return laddress(n);
}

static inline Address aaddress(int n) {
  return iaddress(n);
}

static inline Address iaddress(Register r) {
  return Address(rlocals, r, Address::lsl(3));
}

static inline Address laddress(Register r, Register scratch,
			       InterpreterMacroAssembler* _masm) {
  __ lea(scratch, Address(rlocals, r, Address::lsl(3)));
  return Address(scratch, Interpreter::local_offset_in_bytes(1));
}

static inline Address faddress(Register r) {
  return iaddress(r);
}

static inline Address daddress(Register r, Register scratch,
			       InterpreterMacroAssembler* _masm) {
  return laddress(r, scratch, _masm);
}

static inline Address aaddress(Register r) {
  return iaddress(r);
}

static inline Address at_rsp() {
  return Address(sp, 0);
}

// At top of Java expression stack which may be different than esp().  It
// isn't for category 1 objects.
static inline Address at_tos   () {
  return Address(sp,  Interpreter::expr_offset_in_bytes(0));
}

static inline Address at_tos_p1() {
  return Address(sp,  Interpreter::expr_offset_in_bytes(1));
}

static inline Address at_tos_p2() {
  return Address(sp,  Interpreter::expr_offset_in_bytes(2));
}

static inline Address at_tos_p3() {
  return Address(sp,  Interpreter::expr_offset_in_bytes(3));
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
                         bool precise) {
  assert(val == noreg || val == r0, "parameter is just for looks");
  switch (barrier) {
#ifndef SERIALGC
    case BarrierSet::G1SATBCT:
    case BarrierSet::G1SATBCTLogging:
      {
        // flatten object address if needed
        if (obj.index() == noreg && obj.offset() == 0) {
          if (obj.base() != r3) {
            __ mov(r3, obj.base());
          }
        } else {
          __ lea(r3, obj);
        }
        __ g1_write_barrier_pre(r3 /* obj */,
                                r1 /* pre_val */,
                                rthread /* thread */,
                                r10  /* tmp */,
                                val != noreg /* tosca_live */,
                                false /* expand_call */);
        if (val == noreg) {
          __ store_heap_oop_null(Address(r3, 0));
        } else {
          __ store_heap_oop(Address(r3, 0), val);
          __ g1_write_barrier_post(r3 /* store_adr */,
                                   val /* new_val */,
                                   rthread /* thread */,
                                   r10 /* tmp */,
                                   r1 /* tmp2 */);
        }

      }
      break;
#endif // SERIALGC
    case BarrierSet::CardTableModRef:
    case BarrierSet::CardTableExtension:
      {
        if (val == noreg) {
          __ store_heap_oop_null(obj);
        } else {
          __ store_heap_oop(obj, val);
          // flatten object address if needed
          if (!precise || (obj.index() == noreg && obj.offset() == 0)) {
            __ store_check(obj.base());
          } else {
            __ lea(r3, obj);
            __ store_check(r3);
          }
        }
      }
      break;
    case BarrierSet::ModRef:
    case BarrierSet::Other:
      if (val == noreg) {
        __ store_heap_oop_null(obj);
      } else {
        __ store_heap_oop(obj, val);
      }
      break;
    default      :
      ShouldNotReachHere();

  }
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
  __ ldrb(reg, at_bcp(offset));
  __ neg(reg, reg);
}

void TemplateTable::iload()
{
  transition(vtos, itos);
  if (RewriteFrequentPairs) {
    // TODO : check x86 code for what to do here
    __ call_Unimplemented();
  } else {
    locals_index(r1);
    __ ldr(r0, iaddress(r1));
  }

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
  transition(vtos, ltos);
  locals_index(r1);
  __ ldr(r0, iaddress(r1));
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
  transition(vtos, atos);
  locals_index(r1);
  __ ldr(r0, iaddress(r1));
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
  // destroys r1
  // check array
  __ null_check(array, arrayOopDesc::length_offset_in_bytes());
  // sign extend index for use by indexed load
  // __ movl2ptr(index, index);
  // check index
  __ ldrw(rscratch1, Address(array, arrayOopDesc::length_offset_in_bytes()));
  __ cmp(index, rscratch1);
  if (index != r1) {
    // ??? convention: move aberrant index into r1 for exception message
    assert(r1 != array, "different registers");
    __ mov(r1, index);
  }
  Label ok;
  __ br(Assembler::LT, ok);
  __ mov(rscratch1, Interpreter::_throw_ArrayIndexOutOfBoundsException_entry);
  __ br(rscratch1);
  __ bind(ok);
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
  transition(vtos, itos);
  __ ldr(r0, iaddress(n));
}

void TemplateTable::lload(int n)
{
  transition(vtos, ltos);
  __ ldr(r0, iaddress(n));
}

void TemplateTable::fload(int n)
{
}

void TemplateTable::dload(int n)
{
  __ call_Unimplemented();
}

void TemplateTable::aload(int n)
{
  transition(vtos, atos);
  __ ldr(r0, iaddress(n));
}

void TemplateTable::aload_0()
{
  // According to bytecode histograms, the pairs:
  //
  // _aload_0, _fast_igetfield
  // _aload_0, _fast_agetfield
  // _aload_0, _fast_fgetfield
  //
  // occur frequently. If RewriteFrequentPairs is set, the (slow)
  // _aload_0 bytecode checks if the next bytecode is either
  // _fast_igetfield, _fast_agetfield or _fast_fgetfield and then
  // rewrites the current bytecode into a pair bytecode; otherwise it
  // rewrites the current bytecode into _fast_aload_0 that doesn't do
  // the pair check anymore.
  //
  // Note: If the next bytecode is _getfield, the rewrite must be
  //       delayed, otherwise we may miss an opportunity for a pair.
  //
  // Also rewrite frequent pairs
  //   aload_0, aload_1
  //   aload_0, iload_1
  // These bytecodes with a small amount of code are most profitable
  // to rewrite
  if (RewriteFrequentPairs) {
    __ call_Unimplemented();
  } else {
    aload(0);
  }
}

void TemplateTable::istore()
{
  transition(itos, vtos);
  locals_index(r1);
  // FIXME: We're being very pernickerty here storing a jint in a
  // local with strw, which costs an extra instruction over what we'd
  // be able to do with a simple str.  We should just store the whole
  // word.
  __ lea(rscratch1, iaddress(r1));
  __ strw(r0, Address(rscratch1));
}

void TemplateTable::lstore()
{
  transition(ltos, vtos);
  locals_index(r1);
  __ str(r0, laddress(r1, rscratch1, _masm));
}

void TemplateTable::fstore() {
  transition(ftos, vtos);
  locals_index(r1);
  __ lea(rscratch1, iaddress(r1));
  __ strs(v0, Address(rscratch1));
}

void TemplateTable::dstore() {
  transition(dtos, vtos);
  locals_index(r1);
  __ strd(v0, daddress(r1, rscratch1, _masm));
}

void TemplateTable::astore()
{
  transition(vtos, vtos);
  __ pop_ptr(r0);
  locals_index(r1);
  __ str(r0, aaddress(r1));
}

void TemplateTable::wide_istore() {
  transition(vtos, vtos);
  __ pop_i();
  locals_index_wide(r1);
  __ lea(rscratch1, iaddress(r1));
  __ strw(r0, Address(rscratch1));
}

void TemplateTable::wide_lstore() {
  transition(vtos, vtos);
  __ pop_l();
  locals_index_wide(r1);
  __ str(r0, laddress(r1, rscratch1, _masm));
}

void TemplateTable::wide_fstore() {
  transition(vtos, vtos);
  __ pop_f();
  locals_index_wide(r1);
  __ lea(rscratch1, faddress(r1));
  __ strs(v0, rscratch1);
}

void TemplateTable::wide_dstore() {
  transition(vtos, vtos);
  __ pop_d();
  locals_index_wide(r1);
  __ strd(v0, daddress(r1, rscratch1, _masm));
}

void TemplateTable::wide_astore() {
  transition(vtos, vtos);
  __ pop_ptr(r0);
  locals_index_wide(r1);
  __ str(r0, aaddress(r1));
}

void TemplateTable::iastore() {
  transition(itos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::lsl(2)));
  __ strw(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_INT)));
}

void TemplateTable::lastore() {
  transition(ltos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::lsl(3)));
  __ str(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_LONG)));
}

void TemplateTable::fastore() {
  transition(ftos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // v0: value
  // r1:  index
  // r3:  array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::lsl(2)));
  __ strs(v0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}

void TemplateTable::dastore() {
  transition(dtos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // v0: value
  // r1:  index
  // r3:  array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::lsl(3)));
  __ strd(v0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));
}

void TemplateTable::aastore() {
  Label is_null, ok_is_subtype, done;
  transition(vtos, vtos);
  // stack: ..., array, index, value
  __ ldr(r0, at_tos());    // value
  __ ldr(r2, at_tos_p1()); // index
  __ ldr(r3, at_tos_p2()); // array

  Address element_address(r4, arrayOopDesc::base_offset_in_bytes(T_OBJECT));

  index_check(r3, r2);     // kills r1
  // do array store check - check for NULL value first
  __ cbzw(r0, is_null);

  // Move subklass into r1
  __ load_klass(r1, r0);
  // Move superklass into r0
  __ load_klass(r0, r3);
  __ ldr(r0, Address(r0,
		     objArrayKlass::element_klass_offset()));
  // Compress array + index*oopSize + 12 into a single register.  Frees r2.

  __ add(r4, r3, r2, Assembler::LSL, UseCompressedOops? 2 : 3);
  __ lea(r3, element_address);

  // Generate subtype check.  Blows r2, r5
  // Superklass in r0.  Subklass in r1.
  __ gen_subtype_check(r1, ok_is_subtype);

  // Come here on failure
  // object is at TOS
  __ b(Interpreter::_throw_ArrayStoreException_entry);

  // Come here on success
  __ bind(ok_is_subtype);

  // Get the value we will store
  __ ldr(r0, at_tos());
  // Now store using the appropriate barrier
  do_oop_store(_masm, Address(r3, 0), r0, _bs->kind(), true);
  __ b(done);

  // Have a NULL in r0, r3=array, r2=index.  Store NULL at ary[idx]
  __ bind(is_null);
  __ profile_null_seen(r2);

  // Store a NULL
  do_oop_store(_masm, element_address, noreg, _bs->kind(), true);

  // Pop stack arguments
  __ bind(done);
  __ add(sp, sp, 3 * Interpreter::stackElementSize);
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
  transition(itos, vtos);
  __ str(r0, iaddress(n));
}

void TemplateTable::lstore(int n)
{
  transition(ltos, vtos);
  __ str(r0, iaddress(n));
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
  transition(vtos, vtos);
  __ pop_ptr(r0);
  __ str(r0, iaddress(n));
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
  transition(vtos, vtos);
  __ ldr(r0, Address(sp, 0));
  __ push(r0);
  // stack: ..., a, a
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
  transition(itos, itos);
  // r0 <== r1 op r0
  __ pop_i(r1);
  switch (op) {
  case add  : __ addw(r0, r1, r0); break;
  case sub  : __ subw(r0, r1, r0); break;
  case mul  : __ mulw(r0, r1, r0); break;
  case _and : __ andw(r0, r1, r0); break;
  case _or  : __ orrw(r0, r1, r0); break;
  case _xor : __ eorw(r0, r1, r0); break;
  case shl  : __ lslvw(r0, r1, r0); break;
  case shr  : __ asrvw(r0, r1, r0); break;
  case ushr : __ lsrvw(r0, r1, r0);break;
  default   : ShouldNotReachHere();
  }
}

void TemplateTable::lop2(Operation op)
{
  transition(ltos, ltos);
  // r0 <== r1 op r0
  __ pop_l(r1);
  switch (op) {
  case add  : __ add(r0, r1, r0); break;
  case sub  : __ sub(r0, r1, r0); break;
  case mul  : __ mul(r0, r1, r0); break;
  case _and : __ andr(r0, r1, r0); break;
  case _or  : __ orr(r0, r1, r0); break;
  case _xor : __ eor(r0, r1, r0); break;
  default   : ShouldNotReachHere();
  }
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
    assert(state == vtos, "only valid state");

    __ ldr(c_rarg1, aaddress(0));
    __ load_klass(r3, c_rarg1);
    __ ldrw(r3, Address(r3, Klass::access_flags_offset()));
    __ tst(r3, JVM_ACC_HAS_FINALIZER);
    Label skip_register_finalizer;
    __ br(Assembler::EQ, skip_register_finalizer);

    __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::register_finalizer), c_rarg1);

    __ bind(skip_register_finalizer);
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
                                              bool is_static = false) {
  assert_different_registers(cache, index, flags, off);

  ByteSize cp_base_offset = constantPoolCacheOopDesc::base_offset();
  // Field offset
  __ lea(rscratch1, Address(cache, index, Address::lsl(3)));
  __ ldr(off, Address(rscratch1, in_bytes(cp_base_offset +
					  ConstantPoolCacheEntry::f2_offset())));
  // Flags
  __ ldrw(flags, Address(rscratch1, in_bytes(cp_base_offset +
					   ConstantPoolCacheEntry::flags_offset())));

  // klass overwrite register
  if (is_static) {
    __ ldr(obj, Address(rscratch1, in_bytes(cp_base_offset +
					    ConstantPoolCacheEntry::f1_offset())));
  }
}

void TemplateTable::load_invoke_cp_cache_entry(int byte_no,
                                               Register method,
                                               Register itable_index,
                                               Register flags,
                                               bool is_invokevirtual,
                                               bool is_invokevfinal, /*unused*/
                                               bool is_invokedynamic) {
  const Register index = r4;
  assert_different_registers(method, flags);
  assert_different_registers(method, index);
  assert_different_registers(itable_index, flags);
  assert_different_registers(itable_index, index);
  assert_different_registers(flags, index);
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
    resolve_cache_and_index(byte_no, method, rcpool, index, sizeof(u4));
  } else {
    resolve_cache_and_index(byte_no, noreg, rcpool, index, sizeof(u2));
    __ add(rscratch1, rcpool, method_offset);
    __ ldr(method, Address(rscratch1, index, Address::lsl(3)));
  }
  if (itable_index != noreg) {
    __ add(rscratch1, rcpool, index_offset);
    __ ldr(itable_index, Address(rscratch1, index, Address::lsl(3)));
  }
  __ add(flags, rcpool, flags_offset);
  __ ldr(flags, Address(flags, index, Address::lsl(3)));
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
  if (JvmtiExport::can_post_field_modification()) {
    __ call_Unimplemented();
  }
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

  __ ubfx(flags, flags, ConstantPoolCacheEntry::tosBits, 4);

  assert(btos == 0, "change code, btos != 0");
  __ cbnz(flags, notByte);

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
    __ add(rscratch1, sp, recv, ext::uxtx, 3);
    __ sub(rscratch1, rscratch1, Interpreter::expr_offset_in_bytes(1));
    __ ldr(recv, Address(rscratch1));
    __ verify_oop(recv);
  }

  // do null check if needed
  if (receiver_null_check) {
    __ null_check(recv);
  }

  // compute return type
  __ ubfx(rscratch2, flags, ConstantPoolCacheEntry::tosBits, 4);
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
    __ ldr(lr, Address(rscratch1, rscratch2, Address::lsl(3)));
  }

  // flags is a whole xword: mask it off
  if (save_flags) {
    __ andr(flags, flags, 0xffffffffu);
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
  transition(vtos, vtos);
  assert(byte_no == f2_byte, "use this argument");
  prepare_invoke(rmethod, noreg, byte_no);

  // rbx: index
  // rcx: receiver
  // rdx: flags

  __ call_Unimplemented();
  // invokevirtual_helper(rbx, rcx, rdx);
}

void TemplateTable::invokespecial(int byte_no)
{
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");
  prepare_invoke(rmethod, noreg, byte_no);
  // do the call
  __ verify_oop(rmethod);
  __ profile_call(r0);
  __ jump_from_interpreted(rmethod, r0);
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

void TemplateTable::_new() {
  transition(vtos, atos);

  __ get_unsigned_2_byte_index_at_bcp(r3, 1);
  Label slow_case;
  Label done;
  Label initialize_header;
  Label initialize_object; // including clearing the fields
  Label allocate_shared;

  __ get_cpool_and_tags(r4, r0);
  // Make sure the class we're about to instantiate has been resolved.
  // This is done before loading instanceKlass to be consistent with the order
  // how Constant Pool is updated (see constantPoolOopDesc::klass_at_put)
  const int tags_offset = typeArrayOopDesc::header_size(T_BYTE) * wordSize;
  __ lea(rscratch1, Address(r0, r3, Address::lsl(3)));
  __ ldr(rscratch1, Address(rscratch1, tags_offset));
  __ cmp(rscratch1, JVM_CONSTANT_Class);
  __ br(Assembler::NE, slow_case);

  // get instanceKlass
  __ lea(r4, Address(r4, r3, Address::lsl(3)));
  __ ldr(r4, Address(r4, sizeof(constantPoolOopDesc)));

  // make sure klass is initialized & doesn't have finalizer
  // make sure klass is fully initialized
  __ ldrb(rscratch1, Address(r4, instanceKlass::init_state_offset()));
  __ cmp(rscratch1, instanceKlass::fully_initialized);
  __ br(Assembler::NE, slow_case);

  // get instance_size in instanceKlass (scaled to a count of bytes)
  __ ldrw(r3,
          Address(r4,
                  Klass::layout_helper_offset()));
  // test to see if it has a finalizer or is malformed in some way
  __ tbnz(r3, exact_log2(Klass::_lh_instance_slow_path_bit), slow_case);

  // Allocate the instance
  // 1) Try to allocate in the TLAB
  // 2) if fail and the object is large allocate in the shared Eden
  // 3) if the above fails (or is not applicable), go to a slow case
  // (creates a new TLAB, etc.)

  const bool allow_shared_alloc =
    Universe::heap()->supports_inline_contig_alloc() && !CMSIncrementalMode;

  if (UseTLAB) {
    __ ldr(r0, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
    __ lea(r1, Address(r0, r3));
    __ ldr(rscratch1, Address(rthread, in_bytes(JavaThread::tlab_end_offset())));
    __ cmp(r1, rscratch1);
    __ br(Assembler::GT, allow_shared_alloc ? allocate_shared : slow_case);
    __ str(r1, Address(rthread, in_bytes(JavaThread::tlab_top_offset())));
    if (ZeroTLAB) {
      // the fields have been already cleared
      __ b(initialize_header);
    } else {
      // initialize both the header and fields
      __ b(initialize_object);
    }
  }

  // Allocation in the shared Eden, if allowed.
  //
  // rdx: instance size in bytes
  if (allow_shared_alloc) {
    __ bind(allocate_shared);

    ExternalAddress top((address)Universe::heap()->top_addr());
    ExternalAddress end((address)Universe::heap()->end_addr());

    const Register RtopAddr = r10;
    const Register RendAddr = r11;

    __ lea(RtopAddr, top);
    __ lea(RendAddr, end);
    __ ldr(r0, Address(RtopAddr, 0));

    Label retry;
    __ bind(retry);
    __ lea(r1, Address(r0, r3));
    __ ldr(rscratch1, Address(RendAddr, 0));
    __ cmp(r1, rscratch1);
    __ br(Assembler::GT, slow_case);

    // Compare r0 with the top addr, and if still equal, store the new
    // top addr in r1 at the address of the top addr pointer. Sets ZF if was
    // equal, and clears it otherwise. Use lock prefix for atomicity on MPs.
    //
    // r0: object begin
    // r1: object end
    // r3: instance size in bytes
    __ cmpxchgptr(r1, RtopAddr, rscratch1);

    // if someone beat us on the allocation, try again, otherwise continue
    __ cbnzw(rscratch1, retry);
    __ incr_allocated_bytes(rthread, r3, 0, rscratch1);
  }

  if (UseTLAB || Universe::heap()->supports_inline_contig_alloc()) {
    // The object is initialized before the header.  If the object size is
    // zero, go directly to the header initialization.
    __ bind(initialize_object);
    __ sub(r3, r3, sizeof(oopDesc));
    __ br(Assembler::EQ, initialize_header);

    // Initialize object fields
    {
      __ mov(r2, r0);
      Label loop;
      __ bind(loop);
      __ str(zr, Address(__ post(r2, BytesPerLong)));
      __ subs(r3, r3, BytesPerLong);
      __ br(Assembler::NE, loop);
    }

    // initialize object header only.
    __ bind(initialize_header);
    if (UseBiasedLocking) {
      __ ldr(rscratch1, Address(r4, Klass::prototype_header_offset()));
    } else {
      __ mov(rscratch1, (intptr_t)markOopDesc::prototype());
    }
    __ str(rscratch1, Address(r0, oopDesc::mark_offset_in_bytes()));
    __ store_klass_gap(r0, zr);  // zero klass gap for compressed oops
    __ store_klass(r0, r4);      // store klass last

    {
      SkipIfEqual skip(_masm, &DTraceAllocProbes, false);
      // Trigger dtrace event for fastpath
      __ push(atos); // save the return value
      __ call_VM_leaf(
           CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_object_alloc), r0);
      __ pop(atos); // restore the return value

    }
    __ b(done);
  }

  // slow case
  __ bind(slow_case);
  __ get_constant_pool(c_rarg1);
  __ get_unsigned_2_byte_index_at_bcp(c_rarg2, 1);
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::_new), c_rarg1, c_rarg2);
  __ verify_oop(r0);

  // continue
  __ bind(done);
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
