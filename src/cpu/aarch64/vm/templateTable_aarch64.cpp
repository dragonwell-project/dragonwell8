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

static inline Address at_tos_p4() {
  return Address(sp,  Interpreter::expr_offset_in_bytes(4));
}

static inline Address at_tos_p5() {
  return Address(sp,  Interpreter::expr_offset_in_bytes(5));
}

// Condition conversion
static Assembler::Condition j_not(TemplateTable::Condition cc) {
  switch (cc) {
  case TemplateTable::equal        : return Assembler::NE;
  case TemplateTable::not_equal    : return Assembler::EQ;
  case TemplateTable::less         : return Assembler::GE;
  case TemplateTable::less_equal   : return Assembler::GT;
  case TemplateTable::greater      : return Assembler::LE;
  case TemplateTable::greater_equal: return Assembler::LT;
  }
  ShouldNotReachHere();
  return Assembler::EQ;
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
  if (!RewriteBytecodes)  return;
  Label L_patch_done;

  switch (bc) {
  case Bytecodes::_fast_aputfield:
  case Bytecodes::_fast_bputfield:
  case Bytecodes::_fast_cputfield:
  case Bytecodes::_fast_dputfield:
  case Bytecodes::_fast_fputfield:
  case Bytecodes::_fast_iputfield:
  case Bytecodes::_fast_lputfield:
  case Bytecodes::_fast_sputfield:
    {
      // We skip bytecode quickening for putfield instructions when
      // the put_code written to the constant pool cache is zero.
      // This is required so that every execution of this instruction
      // calls out to InterpreterRuntime::resolve_get_put to do
      // additional, required work.
      assert(byte_no == f1_byte || byte_no == f2_byte, "byte_no out of range");
      assert(load_bc_into_bc_reg, "we use bc_reg as temp");
      __ get_cache_and_index_and_bytecode_at_bcp(temp_reg, bc_reg, temp_reg, byte_no, 1);
      __ movw(bc_reg, bc);
      __ cmpw(temp_reg, (unsigned) 0);
      __ br(Assembler::EQ, L_patch_done);  // don't patch
    }
    break;
  default:
    assert(byte_no == -1, "sanity");
    // the pair bytecodes have already done the load.
    if (load_bc_into_bc_reg) {
      __ movw(bc_reg, bc);
    }
  }

  if (JvmtiExport::can_post_breakpoint()) {
    __ call_Unimplemented();
  }

#ifdef ASSERT
  Label L_okay;
  __ load_unsigned_byte(temp_reg, at_bcp(0));
  __ cmpw(temp_reg, (int) Bytecodes::java_code(bc));
  __ br(Assembler::EQ, L_okay);
  __ cmpw(temp_reg, bc_reg);
  __ br(Assembler::EQ, L_okay);
  __ stop("patching the wrong bytecode");
  __ bind(L_okay);
#endif

  // patch bytecode
  __ strb(bc_reg, at_bcp(0));
  __ bind(L_patch_done);
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
  __ load_signed_byte32(r0, at_bcp(1));
}

void TemplateTable::sipush()
{
  transition(vtos, itos);
  __ load_unsigned_short(r0, at_bcp(1));
  __ revw(r0, r0);
  __ asrw(r0, r0, 16);
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
  __ add(r3, r1, tags_offset);
  __ ldrb(r3, Address(r0, r3));

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

  __ bind(notClass);
  __ cmp(r3, JVM_CONSTANT_Float);
  __ br(Assembler::NE, notFloat);
  // ftos
  __ adds(r1, r2, r1, Assembler::LSL, 3);
  __ ldrs(v0, Address(r1, base_offset));
  __ push_f();
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
  __ adds(r1, r2, r1, Assembler::LSL, 3);
  __ ldr(r0, Address(r1, base_offset));
  __ push_i(r0);
  __ b(Done);

  __ bind(isOop);
  __ adds(r1, r2, r1, Assembler::LSL, 3);
  __ ldr(r0, Address(r1, base_offset));
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
  transition(vtos, vtos);
  Label Long, Done;
  __ get_unsigned_2_byte_index_at_bcp(r0, 1);

  __ get_cpool_and_tags(r1, r2);
  const int base_offset = constantPoolOopDesc::header_size() * wordSize;
  const int tags_offset = typeArrayOopDesc::header_size(T_BYTE) * wordSize;

  // get type
  __ lea(r2, Address(r2, r0, Address::lsl(0)));
  __ load_unsigned_byte(r2, Address(r2, tags_offset));
  __ cmpw(r2, JVM_CONSTANT_Double);
  __ br(Assembler::NE, Long);
  // dtos
  __ lea (r2, Address(r1, r0, Address::lsl(3)));
  __ ldrd(v0, Address(r2, base_offset));
  __ push_d();
  __ b(Done);

  __ bind(Long);
  // ltos
  __ lea(r0, Address(r1, r0, Address::lsl(3)));
  __ ldr(r0, Address(r0, base_offset));
  __ push_l();

  __ bind(Done);
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
  __ ldr(r0, laddress(r1, rscratch1, _masm));
}

void TemplateTable::fload()
{
  transition(vtos, ftos);
  locals_index(r1);
  // n.b. we use ldrd here because this is a 64 bit slot
  // this is comparable to the iload case
  __ ldrd(v0, faddress(r1));
}

void TemplateTable::dload()
{
  transition(vtos, dtos);
  locals_index(r1);
  __ ldrd(v0, daddress(r1, rscratch1, _masm));
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
  // destroys r1, rscratch1
  // check array
  __ null_check(array, arrayOopDesc::length_offset_in_bytes());
  // sign extend index for use by indexed load
  // __ movl2ptr(index, index);
  // check index
  __ ldrw(rscratch1, Address(array, arrayOopDesc::length_offset_in_bytes()));
  __ cmpw(index, rscratch1);
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
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1, Address(r0, r1, Address::lsl(2)));
  __ ldrw(r0, Address(r1, arrayOopDesc::base_offset_in_bytes(T_INT)));
}

void TemplateTable::laload()
{
  transition(itos, ltos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1, Address(r0, r1, Address::lsl(3)));
  __ ldr(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_LONG)));
}

void TemplateTable::faload()
{
  transition(itos, ftos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1,  Address(r0, r1, Address::lsl(2)));
  __ ldrs(v0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_FLOAT)));
}

void TemplateTable::daload()
{
  transition(itos, dtos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1,  Address(r0, r1, Address::lsl(3)));
  __ ldrd(v0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_DOUBLE)));
}

void TemplateTable::aaload()
{
  transition(itos, atos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  int s = (UseCompressedOops ? 2 : 3);
  __ lea(r1, Address(r0, r1, Address::lsl(s)));
  __ load_heap_oop(r0, Address(r1, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

void TemplateTable::baload()
{
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1,  Address(r0, r1, Address::lsl(0)));
  __ load_signed_byte(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}

void TemplateTable::caload()
{
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1,  Address(r0, r1, Address::lsl(1)));
  __ load_unsigned_short(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_CHAR)));
}

// iload followed by caload frequent pair
void TemplateTable::fast_icaload()
{
  __ call_Unimplemented();
}

void TemplateTable::saload()
{
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_ptr(r0);
  // r0: array
  // r1: index
  index_check(r0, r1); // leaves index in r1, kills rscratch1
  __ lea(r1,  Address(r0, r1, Address::lsl(1)));
  __ load_signed_short(r0, Address(r1,  arrayOopDesc::base_offset_in_bytes(T_SHORT)));
}

void TemplateTable::iload(int n)
{
  transition(vtos, itos);
  __ ldr(r0, iaddress(n));
}

void TemplateTable::lload(int n)
{
  transition(vtos, ltos);
  __ ldr(r0, laddress(n));
}

void TemplateTable::fload(int n)
{
  transition(vtos, ftos);
  __ ldrs(v0, faddress(n));
}

void TemplateTable::dload(int n)
{
  transition(vtos, dtos);
  __ ldrd(v0, daddress(n));
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
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(2)));
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
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(3)));
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
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(2)));
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
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(3)));
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

  __ add(r4, r3, r2, ext::uxtw, UseCompressedOops? 2 : 3);
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
  transition(itos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(0)));
  __ strb(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_BYTE)));
}

void TemplateTable::castore()
{
  transition(itos, vtos);
  __ pop_i(r1);
  __ pop_ptr(r3);
  // r0: value
  // r1: index
  // r3: array
  index_check(r3, r1); // prefer index in r1
  __ lea(rscratch1, Address(r3, r1, Address::uxtw(1)));
  __ strh(r0, Address(rscratch1,
		      arrayOopDesc::base_offset_in_bytes(T_CHAR)));
}

void TemplateTable::sastore()
{
  castore();
}

void TemplateTable::istore(int n)
{
  transition(itos, vtos);
  __ str(r0, iaddress(n));
}

void TemplateTable::lstore(int n)
{
  transition(ltos, vtos);
  __ str(r0, laddress(n));
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
  transition(vtos, vtos);
  __ add(sp, sp, Interpreter::stackElementSize);
}

void TemplateTable::pop2()
{
  transition(vtos, vtos);
  __ add(sp, sp, 2 * Interpreter::stackElementSize);
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
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ldr(r0, at_tos());  // load b
  __ ldr(r2, at_tos_p1());  // load a
  __ str(r0, at_tos_p1());  // store b
  __ str(r2, at_tos());  // store a
  __ push(r0);			// push b
  // stack: ..., b, a, b
}

void TemplateTable::dup_x2()
{
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ ldr(r0, at_tos());  // load c
  __ ldr(r2, at_tos_p2());  // load a
  __ str(r0, at_tos_p2());  // store c in a
  __ push(r0);      // push c
  // stack: ..., c, b, c, c
  __ ldr(r0, at_tos_p2());  // load b
  __ str(r2, at_tos_p2());  // store a in b
  // stack: ..., c, a, c, c
  __ str(r0, at_tos_p1());  // store b in c
  // stack: ..., c, a, b, c
}

void TemplateTable::dup2()
{
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ldr(r0, at_tos_p1());  // load a
  __ push(r0);                  // push a
  __ ldr(r0, at_tos_p1());  // load b
  __ push(r0);                  // push b
  // stack: ..., a, b, a, b
}

void TemplateTable::dup2_x1()
{
  transition(vtos, vtos);
  // stack: ..., a, b, c
  __ ldr(r2, at_tos());  // load c
  __ ldr(r0, at_tos_p1());  // load b
  __ push(r0);                  // push b
  __ push(r2);                  // push c
  // stack: ..., a, b, c, b, c
  __ str(r2, at_tos_p3());  // store c in b
  // stack: ..., a, c, c, b, c
  __ ldr(r2, at_tos_p4());  // load a
  __ str(r2, at_tos_p2());  // store a in 2nd c
  // stack: ..., a, c, a, b, c
  __ str(r0, at_tos_p4());  // store b in a
  // stack: ..., b, c, a, b, c
}

void TemplateTable::dup2_x2()
{
  transition(vtos, vtos);
  // stack: ..., a, b, c, d
  __ ldr(r2, at_tos());  // load d
  __ ldr(r0, at_tos_p1());  // load c
  __ push(r0)            ;      // push c
  __ push(r2);                  // push d
  // stack: ..., a, b, c, d, c, d
  __ ldr(r0, at_tos_p4());  // load b
  __ str(r0, at_tos_p2());  // store b in d
  __ str(r2, at_tos_p4());  // store d in b
  // stack: ..., a, d, c, b, c, d
  __ ldr(r2, at_tos_p5());  // load a
  __ ldr(r0, at_tos_p3());  // load c
  __ str(r2, at_tos_p3());  // store a in c
  __ str(r0, at_tos_p5());  // store c in a
  // stack: ..., c, d, a, b, c, d
}

void TemplateTable::swap()
{
  transition(vtos, vtos);
  // stack: ..., a, b
  __ ldr(r2, at_tos_p1());  // load a
  __ ldr(r0, at_tos());  // load b
  __ str(r2, at_tos());  // store a in b
  __ str(r0, at_tos_p1());  // store b in a
  // stack: ..., b, a
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
  transition(itos, itos);
  __ mov(r1, r0);
  __ pop_i(r0);
  // r0 <== r0 idiv r1, r1 <== r0 irem r1
  __ corrected_idivl(r0, r1);
}

void TemplateTable::irem()
{
  transition(itos, itos);
  __ pop_i(r1);
  // r1 <== r1 idiv r0, r0 <== r1 irem r0
  __ corrected_idivl(r1, r0);
}

void TemplateTable::lmul()
{
  transition(ltos, ltos);
  __ pop_l(r1);
  __ mul(r0, r0, r1);
}

void TemplateTable::ldiv()
{
  transition(ltos, ltos);
  // explicitly check for div0
  __ ands(r0, r0, r0);
  Label no_div0;
  __ br(Assembler::NE, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);
  __ mov(r1, r0);
  __ pop_l(r0);
  // r0 <== r1 idiv r0, r1 <== r1 irem r0
  __ corrected_idivl(r0, r1);
}

void TemplateTable::lrem()
{
  transition(ltos, ltos);
  // explicitly check for div0
  __ ands(r0, r0, r0);
  Label no_div0;
  __ br(Assembler::NE, no_div0);
  __ mov(rscratch1, Interpreter::_throw_ArithmeticException_entry);
  __ br(rscratch1);
  __ bind(no_div0);  
  __ pop_l(r1);
  // r1 <== r1 idiv r0, r0 <== r1 irem r0
  __ corrected_idivl(r1, r0);
}

void TemplateTable::lshl()
{
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ lslv(r0, r1, r0);
}

void TemplateTable::lshr()
{
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ asrv(r0, r1, r0);
}

void TemplateTable::lushr()
{
  transition(itos, ltos);
  // shift count is in r0
  __ pop_l(r1);
  __ lsrv(r0, r1, r0);
}

void TemplateTable::fop2(Operation op)
{
  transition(ftos, ftos);
  switch (op) {
  case add:
    // n.b. use ldrd because this is a 64 bit slot
    __ pop_f(v1);
    __ fadds(v0, v1, v0);
    break;
  case sub:
    __ pop_f(v1);
    __ fsubs(v0, v1, v0);
    break;
  case mul:
    __ pop_f(v1);
    __ fmuls(v0, v1, v0);
    break;
  case div:
    __ pop_f(v1);
    __ fdivs(v0, v1, v0);
    break;
  case rem:
    __ fmovs(v0, v1);
    __ pop_f(v0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::frem), 2);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void TemplateTable::dop2(Operation op)
{
  transition(dtos, dtos);
  switch (op) {
  case add:
    // n.b. use ldrd because this is a 64 bit slot
    __ pop_d(v1);
    __ faddd(v0, v1, v0);
    break;
  case sub:
    __ pop_d(v1);
    __ fsubd(v0, v1, v0);
    break;
  case mul:
    __ pop_d(v1);
    __ fmuld(v0, v1, v0);
    break;
  case div:
    __ pop_d(v1);
    __ fdivd(v0, v1, v0);
    break;
  case rem:
    __ fmovd(v0, v1);
    __ pop_d(v0);
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::frem), 2);
    break;
  default:
    ShouldNotReachHere();
    break;
  }
}

void TemplateTable::ineg()
{
  transition(itos, itos);
  __ negw(r0, r0);
 
}

void TemplateTable::lneg()
{
  transition(ltos, ltos);
  __ neg(r0, r0);
}

void TemplateTable::fneg()
{
  transition(ftos, ftos);
  __ fnegs(v0, v0);
}

void TemplateTable::dneg()
{
  transition(dtos, dtos);
  __ fnegd(v0, v0);
}

void TemplateTable::iinc()
{
  transition(vtos, vtos);
  __ load_signed_byte(r1, at_bcp(2)); // get constant
  locals_index(r2);
  __ ldr(r0, iaddress(r2));
  __ addw(r0, r0, r1);
  __ str(r0, iaddress(r2));
}

void TemplateTable::wide_iinc()
{
  transition(vtos, vtos);
  __ ldrw(r1, at_bcp(4)); // get constant
  locals_index_wide(r2);
  __ revw(r1, r1);
  __ asrw(r1, r1, 16);
  __ ldr(r0, iaddress(r2));
  __ addw(r0, r0, r1);
  __ str(r0, iaddress(r2));
}

void TemplateTable::convert()
{
  // Checking
#ifdef ASSERT
  {
    TosState tos_in  = ilgl;
    TosState tos_out = ilgl;
    switch (bytecode()) {
    case Bytecodes::_i2l: // fall through
    case Bytecodes::_i2f: // fall through
    case Bytecodes::_i2d: // fall through
    case Bytecodes::_i2b: // fall through
    case Bytecodes::_i2c: // fall through
    case Bytecodes::_i2s: tos_in = itos; break;
    case Bytecodes::_l2i: // fall through
    case Bytecodes::_l2f: // fall through
    case Bytecodes::_l2d: tos_in = ltos; break;
    case Bytecodes::_f2i: // fall through
    case Bytecodes::_f2l: // fall through
    case Bytecodes::_f2d: tos_in = ftos; break;
    case Bytecodes::_d2i: // fall through
    case Bytecodes::_d2l: // fall through
    case Bytecodes::_d2f: tos_in = dtos; break;
    default             : ShouldNotReachHere();
    }
    switch (bytecode()) {
    case Bytecodes::_l2i: // fall through
    case Bytecodes::_f2i: // fall through
    case Bytecodes::_d2i: // fall through
    case Bytecodes::_i2b: // fall through
    case Bytecodes::_i2c: // fall through
    case Bytecodes::_i2s: tos_out = itos; break;
    case Bytecodes::_i2l: // fall through
    case Bytecodes::_f2l: // fall through
    case Bytecodes::_d2l: tos_out = ltos; break;
    case Bytecodes::_i2f: // fall through
    case Bytecodes::_l2f: // fall through
    case Bytecodes::_d2f: tos_out = ftos; break;
    case Bytecodes::_i2d: // fall through
    case Bytecodes::_l2d: // fall through
    case Bytecodes::_f2d: tos_out = dtos; break;
    default             : ShouldNotReachHere();
    }
    transition(tos_in, tos_out);
  }
#endif // ASSERT
  // static const int64_t is_nan = 0x8000000000000000L;

  // Conversion
  switch (bytecode()) {
  case Bytecodes::_i2l:
    __ sxtw(r0, r0);
    break;
  case Bytecodes::_i2f:
    __ scvtfws(v0, r0);
    break;
  case Bytecodes::_i2d:
    __ scvtfwd(v0, r0);
    break;
  case Bytecodes::_i2b:
    __ sxtbw(r0, r0);
    break;
  case Bytecodes::_i2c:
    __ uxthw(r0, r0);
    break;
  case Bytecodes::_i2s:
    __ sxthw(r0, r0);
    break;
  case Bytecodes::_l2i:
    __ uxtw(r0, r0);
    break;
  case Bytecodes::_l2f:
    __ scvtfs(v0, r0);
    break;
  case Bytecodes::_l2d:
    __ scvtfd(v0, r0);
    break;
  case Bytecodes::_f2i:
  {
    __ fcvtzsw(r0, v0);
  }
    break;
  case Bytecodes::_f2l:
  {
    __ fcvtzs(r0, v0);
  }
    break;
  case Bytecodes::_f2d:
    __ fcvts(v0, v0);
    break;
  case Bytecodes::_d2i:
  {
    __ fcvtzdw(r0, v0);
  }
    break;
  case Bytecodes::_d2l:
  {
    __ fcvtzd(r0, v0);
  }
    break;
  case Bytecodes::_d2f:
    __ fcvtd(v0, v0);
    break;
  default:
    ShouldNotReachHere();
  }
}

void TemplateTable::lcmp()
{
  transition(ltos, itos);
  Label done;
  __ pop_l(r1);
  __ cmp(r1, r0);
  __ mov(r0, (u_int64_t)-1L);
  __ br(Assembler::LT, done);
  __ mov(r0, 1UL);
  __ csel(r0, r0, zr, Assembler::NE);
  __ bind(done);
}

void TemplateTable::float_cmp(bool is_float, int unordered_result)
{
  Label done;
  if (is_float) {
    // XXX get rid of pop here, use ... reg, mem32
    __ pop_f(v1);
    __ fcmps(v1, v0);
  } else {
    // XXX get rid of pop here, use ... reg, mem64
    __ pop_d(v1);
    __ fcmpd(v1, v0);
  }
  if (unordered_result < 0) {
    // we want -1 for unordered or less than, 0 for equal and 1 for
    // greater than.
    __ mov(r0, (u_int64_t)-1L);
    // for FP LT tests less than or unordered
    __ br(Assembler::LT, done);
    __ mov(r0, 1L);
    __ csel(r0, r0, zr, Assembler::GT);
  } else {
    // we want -1 for less than, 0 for equal and 1 for unordered or
    // greater than.
    __ mov(r0, 1L);
    // for FP GT tests greater than or unordered
    __ br(Assembler::GT, done);
    __ mov(r0, (u_int64_t)-1L);
    __ csel(r0, r0, zr, Assembler::LE);
  }
  __ bind(done);
}

void TemplateTable::branch(bool is_jsr, bool is_wide)
{
  __ profile_taken_branch(r0, r1);
  const ByteSize be_offset = methodOopDesc::backedge_counter_offset() +
                             InvocationCounter::counter_offset();
  const ByteSize inv_offset = methodOopDesc::invocation_counter_offset() +
                              InvocationCounter::counter_offset();
  const int method_offset = frame::interpreter_frame_method_offset * wordSize;

  // load branch displacement
  if (!is_wide) {
    __ ldrh(r0, at_bcp(1));
    __ rev16(r0, r0);
    // Adjust the bcp by the 16-bit displacement in r0
    __ add(rbcp, rbcp, r0, ext::sxth, 0);
  } else {
    __ ldrw(r0, at_bcp(1));
    __ revw(r0, r0);
    // Adjust the bcp by the 32-bit displacement in r0
    __ add(rbcp, rbcp, r0, ext::sxtw, 0);
  }

  if (is_jsr) {
    __ call_Unimplemented();
    return;
  }

  // Normal (non-jsr) branch handling
  assert(UseLoopCounter || !UseOnStackReplacement,
         "on-stack-replacement requires loop counters");
  if (UseLoopCounter) {
    // TODO : add this
  }
  // Pre-load the next target bytecode into rscratch1
  __ load_unsigned_byte(rscratch1, Address(rbcp, 0));

  // continue with the bytecode @ target
  // rscratch1: target bytecode
  // rbcp: target bcp
  __ dispatch_only(vtos);
}


void TemplateTable::if_0cmp(Condition cc)
{
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  if (cc == equal)
    __ cbnzw(r0, not_taken);
  else if (cc == not_equal)
    __ cbzw(r0, not_taken);
  else {
    __ andsw(zr, r0, r0);
    __ br(j_not(cc), not_taken);
  }

  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

void TemplateTable::if_icmp(Condition cc)
{
  transition(itos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  __ pop_i(r1);
  __ cmpw(r1, r0, Assembler::LSL);
  __ br(j_not(cc), not_taken);
  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

void TemplateTable::if_nullcmp(Condition cc)
{
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  if (cc == equal)
    __ cbnz(r0, not_taken);
  else
    __ cbz(r0, not_taken);
  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
}

void TemplateTable::if_acmp(Condition cc)
{
  transition(atos, vtos);
  // assume branch is more often taken than not (loops use backward branches)
  Label not_taken;
  __ pop_ptr(r1);
  __ cmp(r1, r0);
  __ br(j_not(cc), not_taken);
  branch(false, false);
  __ bind(not_taken);
  __ profile_not_taken_branch(r0);
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
  __ ldr(off, Address(cache, in_bytes(cp_base_offset +
					  ConstantPoolCacheEntry::f2_offset())));
  // Flags
  __ ldrw(flags, Address(cache, in_bytes(cp_base_offset +
					   ConstantPoolCacheEntry::flags_offset())));

  // klass overwrite register
  if (is_static) {
    __ ldr(obj, Address(cache, in_bytes(cp_base_offset +
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
  Register cache  __attribute__((unused))  = rscratch2;
  assert_different_registers(method, flags, cache);
  assert_different_registers(method, index, cache);
  assert_different_registers(itable_index, flags, cache);
  assert_different_registers(itable_index, index, cache);
  assert_different_registers(flags, index, cache);
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
    __ ldr(method, Address(cache, method_offset));
  }
  if (itable_index != noreg) {
    __ ldr(itable_index, Address(cache, index_offset));
  }
  __ ldr(flags, Address(cache, flags_offset));
}


// The registers cache and index expected to be set before call.
// Correct values of the cache and index registers are preserved.
void TemplateTable::jvmti_post_field_access(Register cache, Register index,
                                            bool is_static, bool has_tos)
{
  // do the JVMTI work here to avoid disturbing the register state below
  // We use c_rarg registers here because we want to use the register used in
  // the call to the VM
  if (JvmtiExport::can_post_field_access()) {
    __ call_Unimplemented();
  }
}

void TemplateTable::pop_and_check_object(Register r)
{
  __ pop_ptr(r);
  __ null_check(r);  // for field access must check obj.
  __ verify_oop(r);
}

void TemplateTable::getfield_or_static(int byte_no, bool is_static)
{
  const Register cache = r2;
  const Register index = r3;
  const Register obj   = r4;
  const Register off   = r1;
  const Register flags = r0;
  const Register bc    = r4; // uses same reg as obj, so don't mix them

  resolve_cache_and_index(byte_no, noreg, cache, index, sizeof(u2));
  jvmti_post_field_access(cache, index, is_static, false);
  load_field_cp_cache_entry(obj, cache, index, off, flags, is_static);

  if (!is_static) {
    // obj is on the stack
    pop_and_check_object(obj);
  }

  const Address field(obj, off);

  Label Done, notByte, notInt, notShort, notChar,
              notLong, notFloat, notObj, notDouble;

  __ ubfx(flags, flags, ConstantPoolCacheEntry::tosBits, 4);

  assert(btos == 0, "change code, btos != 0");
  __ cbnz(flags, notByte);

  // btos
  __ load_signed_byte(r0, field);
  __ push(btos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_bgetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notByte);
  __ cmp(flags, atos);
  __ br(Assembler::NE, notObj);
  // atos
  __ load_heap_oop(r0, field);
  __ push(atos);
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_agetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notObj);
  __ cmp(flags, itos);
  __ br(Assembler::NE, notInt);
  // itos
  __ ldrw(r0, field);
  __ push(itos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_igetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notInt);
  __ cmp(flags, ctos);
  __ br(Assembler::NE, notChar);
  // ctos
  __ load_unsigned_short(r0, field);
  __ push(ctos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_cgetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notChar);
  __ cmp(flags, stos);
  __ br(Assembler::NE, notShort);
  // stos
  __ load_signed_short(r0, field);
  __ push(stos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_sgetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notShort);
  __ cmp(flags, ltos);
  __ br(Assembler::NE, notLong);
  // ltos
  __ ldr(r0, field);
  __ push(ltos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_lgetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notLong);
  __ cmp(flags, ftos);
  __ br(Assembler::NE, notFloat);
  // ftos
  __ ldrs(v0, field);
  __ push(ftos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_fgetfield, bc, r1);
  }
  __ b(Done);

  __ bind(notFloat);
#ifdef ASSERT
  __ cmp(flags, dtos);
  __ br(Assembler::NE, notDouble);
#endif
  // dtos
  __ ldrd(v0, field);
  __ push(dtos);
  // Rewrite bytecode to be faster
  if (!is_static) {
    patch_bytecode(Bytecodes::_fast_dgetfield, bc, r1);
  }
#ifdef ASSERT
  __ b(Done);

  __ bind(notDouble);
  __ stop("Bad state");
#endif

  __ bind(Done);
  // [jk] not needed currently
  // volatile_barrier(Assembler::Membar_mask_bits(Assembler::LoadLoad |
  //                                              Assembler::LoadStore));
}


void TemplateTable::getfield(int byte_no)
{
  getfield_or_static(byte_no, false);
}

void TemplateTable::getstatic(int byte_no)
{
  getfield_or_static(byte_no, true);
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
  const Register rcache = r19;

  resolve_cache_and_index(byte_no, noreg, rcache, index, sizeof(u2));
  jvmti_post_field_mod(rcpool, index, is_static);
  load_field_cp_cache_entry(obj, rcache, index, off, flags, is_static);

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
    __ strb(r0, field);
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
    __ strw(r0, field);
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
    __ strh(r0, field);
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
    __ strh(r0, field);
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
  putfield_or_static(byte_no, false);
}

void TemplateTable::putstatic(int byte_no) {
  putfield_or_static(byte_no, true);
}

void TemplateTable::jvmti_post_fast_field_mod()
{
  if (JvmtiExport::can_post_field_modification()) {
    __ call_Unimplemented();
  }
}

void TemplateTable::fast_storefield(TosState state)
{
  transition(state, vtos);

  ByteSize base = constantPoolCacheOopDesc::base_offset();

  jvmti_post_fast_field_mod();

  // access constant pool cache
  __ get_cache_and_index_at_bcp(r2, r1, 1);

  // test for volatile with rdx
  __ lea(r3, Address(r2, r1, Address::lsl(3)));
  __ ldrw(r3, Address(r3, in_bytes(base +
				   ConstantPoolCacheEntry::flags_offset())));

  // replace index with field offset from cache entry
  __ lea(r1, Address(r2, r1, Address::lsl(3)));
  __ ldr(r1, Address(r1, in_bytes(base + ConstantPoolCacheEntry::f2_offset())));

  // [jk] not needed currently
  // volatile_barrier(Assembler::Membar_mask_bits(Assembler::LoadStore |
  //                                              Assembler::StoreStore));

  Label notVolatile;
  __ lsl(r3, r3, ConstantPoolCacheEntry::volatileField);
  __ andw(r3, r3, 0x1);

  // Get object from stack
  pop_and_check_object(r2);

  // field address
  const Address field(r2, r1);

  // access field
  switch (bytecode()) {
  case Bytecodes::_fast_aputfield:
    do_oop_store(_masm, field, r0, _bs->kind(), false);
    break;
  case Bytecodes::_fast_lputfield:
    __ str(r0, field);
    break;
  case Bytecodes::_fast_iputfield:
    __ strw(r0, field);
    break;
  case Bytecodes::_fast_bputfield:
    __ strb(r0, field);
    break;
  case Bytecodes::_fast_sputfield:
    // fall through
  case Bytecodes::_fast_cputfield:
    __ strh(r0, field);
    break;
  case Bytecodes::_fast_fputfield:
    __ strs(v0, field);
    break;
  case Bytecodes::_fast_dputfield:
    __ strd(v0, field);
    break;
  default:
    ShouldNotReachHere();
  }

  // TODO : restore this volatile barrier call
  //
  // Check for volatile store
  // __ andw(r3, r3, r3);
  // __ br(Assembler::EQ, notVolatile);
  // volatile_barrier(Assembler::Membar_mask_bits(Assembler::StoreLoad |
  //                                             Assembler::StoreStore));
  // __ bind(notVolatile);
}


void TemplateTable::fast_accessfield(TosState state)
{
  transition(atos, state);
  // Do the JVMTI work here to avoid disturbing the register state below
  if (JvmtiExport::can_post_field_access()) {
    __ call_Unimplemented();
  }
  // access constant pool cache
  __ get_cache_and_index_at_bcp(r2, r1, 1);
  // replace index with field offset from cache entry
  // [jk] not needed currently
  // if (os::is_MP()) {
  //   __ movl(rdx, Address(rcx, rbx, Address::times_8,
  //                        in_bytes(constantPoolCacheOopDesc::base_offset() +
  //                                 ConstantPoolCacheEntry::flags_offset())));
  //   __ shrl(rdx, ConstantPoolCacheEntry::volatileField);
  //   __ andl(rdx, 0x1);
  // }
  __ lea(r1, Address(r2, r1, Address::lsl(3)));
  __ ldr(r1, Address(r1, in_bytes(constantPoolCacheOopDesc::base_offset() +
                                  ConstantPoolCacheEntry::f2_offset())));
  // r0: object
  __ verify_oop(r0);
  __ null_check(r0);
  const Address field(r0, r1);

  // access field
  switch (bytecode()) {
  case Bytecodes::_fast_agetfield:
    __ load_heap_oop(r0, field);
    __ verify_oop(r0);
    break;
  case Bytecodes::_fast_lgetfield:
    __ ldr(r0, field);
    break;
  case Bytecodes::_fast_igetfield:
    __ ldrw(r0, field);
    break;
  case Bytecodes::_fast_bgetfield:
    __ load_signed_byte(r0, field);
    break;
  case Bytecodes::_fast_sgetfield:
    __ load_signed_short(r0, field);
    break;
  case Bytecodes::_fast_cgetfield:
    __ load_unsigned_short(r0, field);
    break;
  case Bytecodes::_fast_fgetfield:
    __ ldrs(v0, field);
    break;
  case Bytecodes::_fast_dgetfield:
    __ ldrd(v0, field);
    break;
  default:
    ShouldNotReachHere();
  }
  // [jk] not needed currently
  // if (os::is_MP()) {
  //   Label notVolatile;
  //   __ testl(rdx, rdx);
  //   __ jcc(Assembler::zero, notVolatile);
  //   __ membar(Assembler::LoadLoad);
  //   __ bind(notVolatile);
  //};
}

void TemplateTable::fast_xaccess(TosState state)
{
  transition(vtos, state);

  // get receiver
  __ ldr(r0, aaddress(0));
  // access constant pool cache
  __ get_cache_and_index_at_bcp(r2, r3, 2);
  __ lea(r1, Address(r2, r3, Address::lsl(3)));
  __ ldr(r1, Address(r1, in_bytes(constantPoolCacheOopDesc::base_offset() +
				  ConstantPoolCacheEntry::f2_offset())));
  // make sure exception is reported in correct bcp range (getfield is
  // next instruction)
  __ increment(rbcp);
  __ null_check(r0);
  switch (state) {
  case itos:
    __ ldr(r0, Address(r0, r1, Address::lsl(0)));
    break;
  case atos:
    __ load_heap_oop(r0, Address(r0, r1, Address::lsl(0)));
    __ verify_oop(r0);
    break;
  case ftos:
    __ ldrs(v0, Address(r0, r1, Address::lsl(0)));
    break;
  default:
    ShouldNotReachHere();
  }

  // [jk] not needed currently
  // if (os::is_MP()) {
  //   Label notVolatile;
  //   __ movl(rdx, Address(rcx, rdx, Address::times_8,
  //                        in_bytes(constantPoolCacheOopDesc::base_offset() +
  //                                 ConstantPoolCacheEntry::flags_offset())));
  //   __ shrl(rdx, ConstantPoolCacheEntry::volatileField);
  //   __ testl(rdx, 0x1);
  //   __ jcc(Assembler::zero, notVolatile);
  //   __ membar(Assembler::LoadLoad);
  //   __ bind(notVolatile);
  // }

  __ decrement(rbcp);
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
    __ andw(recv, flags, 0xFF);
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
  // Uses temporary registers r0, r3
  assert_different_registers(index, recv, r0, r3);
  // Test for an invoke of a final method
  Label notFinal;
  __ andsw(r0, flags, (1 << ConstantPoolCacheEntry::vfinalMethod));
  __ br(Assembler::EQ, notFinal);

  const Register method = index;  // method must be rmethod
  assert(method == rmethod,
         "methodOop must be rmethod for interpreter calling convention");

  // do the call - the index is actually the method to call
  __ verify_oop(method);

  // It's final, need a null check here!
  __ null_check(recv);

  // profile this call
  __ profile_final_call(r0);

  __ jump_from_interpreted(method, r0);

  __ bind(notFinal);

  // get receiver klass
  __ null_check(recv, oopDesc::klass_offset_in_bytes());
  __ load_klass(r0, recv);

  __ verify_oop(r0);

  // profile this call
  __ profile_virtual_call(r0, rlocals, r3);

  // get target methodOop & entry point
  const int base = instanceKlass::vtable_start_offset() * wordSize;
  assert(vtableEntry::size() * wordSize == 8,
         "adjust the scaling in the code below");
  __ lea(rscratch1, Address(r0, index, Address::lsl(3)));
  __ ldr(method, Address(rscratch1, base + vtableEntry::method_offset_in_bytes()));
  __ ldr(r3, Address(method, methodOopDesc::interpreter_entry_offset()));
  __ jump_from_interpreted(method, r3);
}


void TemplateTable::invokevirtual(int byte_no)
{
  transition(vtos, vtos);
  assert(byte_no == f2_byte, "use this argument");

#ifdef ASSERT
  __ spill(rscratch1, rscratch2);
#endif // ASSERT

  prepare_invoke(rmethod, noreg, byte_no);

  // r1: index
  // r2: receiver
  // r3: flags

  invokevirtual_helper(rmethod, r2, r3);
}

void TemplateTable::invokespecial(int byte_no)
{
  transition(vtos, vtos);
  assert(byte_no == f1_byte, "use this argument");

#ifdef ASSERT
  __ spill(rscratch1, rscratch2);
#endif // ASSERT

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

#ifdef ASSERT
  __ spill(rscratch1, rscratch2);
#endif // ASSERT

  prepare_invoke(rmethod, noreg, byte_no);
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

    Label succeed;
    // if someone beat us on the allocation, try again, otherwise continue
    __ cmpxchgptr(r0, r1, RtopAddr, rscratch1, succeed, retry);
    __ bind(succeed);
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
  transition(atos, atos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  // Get cpool & tags index
  __ get_cpool_and_tags(r2, r3); // r2=cpool, r3=tags array
  __ get_unsigned_2_byte_index_at_bcp(r19, 1); // r19=index
  // See if bytecode has already been quicked
  __ add(rscratch1, r3, typeArrayOopDesc::header_size(T_BYTE) * wordSize);
  __ ldrb(r1, Address(rscratch1, r19));
  __ cmp(r1, JVM_CONSTANT_Class);
  __ br(Assembler::EQ, quicked);

  __ push(atos); // save receiver for result, and for GC
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  __ pop(r3); // restore receiver
  __ b(resolved);

  // Get superklass in r0 and subklass in r3
  __ bind(quicked);
  __ mov(r3, r0); // Save object in r3; r0 needed for subtype check
  __ lea(r0, Address(r2, r19, Address::lsl(3)));
  __ ldr(r0, Address(r0, sizeof(constantPoolOopDesc)));

  __ bind(resolved);
  __ load_klass(r19, r3);

  // Generate subtype check.  Blows r2, r5.  Object in r3.
  // Superklass in r0.  Subklass in r19.
  __ gen_subtype_check(r19, ok_is_subtype);

  // Come here on failure
  __ push(r3);
  __ b(Interpreter::_throw_ClassCastException_entry);

  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, r3);

  // Collect counts on whether this test sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ b(done);
    __ bind(is_null);
    __ profile_null_seen(r2);
  } else {
    __ bind(is_null);   // same as 'done'
  }
  __ bind(done);
  // r0 = 0: obj == NULL or  obj is not an instanceof the specified klass
  // r0 = 1: obj != NULL and obj is     an instanceof the specified klass
}

void TemplateTable::instanceof() {
  transition(atos, itos);
  Label done, is_null, ok_is_subtype, quicked, resolved;
  __ cbz(r0, is_null);

  // Get cpool & tags index
  __ get_cpool_and_tags(r2, r3); // r2=cpool, r3=tags array
  __ get_unsigned_2_byte_index_at_bcp(r19, 1); // r19=index
  // See if bytecode has already been quicked
  __ add(rscratch1, r3, typeArrayOopDesc::header_size(T_BYTE) * wordSize);
  __ ldrb(r1, Address(rscratch1, r19));
  __ cmp(r1, JVM_CONSTANT_Class);
  __ br(Assembler::EQ, quicked);

  __ push(atos); // save receiver for result, and for GC
  call_VM(r0, CAST_FROM_FN_PTR(address, InterpreterRuntime::quicken_io_cc));
  __ pop(r3); // restore receiver
  __ verify_oop(r3);
  __ load_klass(r3, r3);
  __ b(resolved);

  // Get superklass in r0 and subklass in r3
  __ bind(quicked);
  __ load_klass(r3, r0);
  __ lea(r0, Address(r2, r19, Address::lsl(3)));
  __ ldr(r0, Address(r0, sizeof(constantPoolOopDesc)));

  __ bind(resolved);

  // Generate subtype check.  Blows r2, r5
  // Superklass in r0.  Subklass in r3.
  __ gen_subtype_check(r3, ok_is_subtype);

  // Come here on failure
  __ mov(r0, 0);
  __ b(done);
  // Come here on success
  __ bind(ok_is_subtype);
  __ mov(r0, 1);

  // Collect counts on whether this test sees NULLs a lot or not.
  if (ProfileInterpreter) {
    __ b(done);
    __ bind(is_null);
    __ profile_null_seen(r2);
  } else {
    __ bind(is_null);   // same as 'done'
  }
  __ bind(done);
  // r0 = 0: obj == NULL or  obj is not an instanceof the specified klass
  // r0 = 1: obj != NULL and obj is     an instanceof the specified klass
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
  transition(atos, vtos);

  // check for NULL object
  __ null_check(r0);

  const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
  const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

  Label allocated;

  // initialize entry pointer
  __ mov(c_rarg1, zr); // points to free slot or NULL

  // find a free slot in the monitor block (result in c_rarg1)
  {
    Label entry, loop, exit;
    __ ldr(c_rarg3, monitor_block_top); // points to current entry,
                                        // starting with top-most entry
    __ lea(c_rarg2, monitor_block_bot); // points to word before bottom

    __ b(entry);

    __ bind(loop);
    // check if current entry is used
    // if not used then remember entry in c_rarg1
    __ ldr(rscratch1, Address(c_rarg3, BasicObjectLock::obj_offset_in_bytes()));
    __ csel(c_rarg1, c_rarg3, c_rarg1, Assembler::EQ);
    // check if current entry is for same object
    __ cmp(r0, rscratch1);
    // if same object then stop searching
    __ br(Assembler::EQ, exit);
    // otherwise advance to next entry
    __ add(c_rarg3, c_rarg3, entry_size);
    __ bind(entry);
    // check if bottom reached
    __ cmp(c_rarg3, c_rarg2);
    // if not at bottom then check this entry
    __ br(Assembler::NE, loop);
    __ bind(exit);
  }

  __ cbnz(c_rarg1, allocated); // check if a slot has been found and
                            // if found, continue with that on

  // allocate one if there's no free slot
  {
    Label entry, loop;
    // 1. compute new pointers            // rsp: old expression stack top
    __ ldr(c_rarg1, monitor_block_bot);   // c_rarg1: old expression stack bottom
    __ sub(sp, sp, entry_size);           // move expression stack top
    __ sub(c_rarg1, c_rarg1, entry_size); // move expression stack bottom
    __ mov(c_rarg3, sp);                 // set start value for copy loop
    __ str(c_rarg1, monitor_block_bot);   // set new monitor block bottom
    __ b(entry);
    // 2. move expression stack contents
    __ bind(loop);
    __ ldr(c_rarg2, Address(c_rarg3, entry_size)); // load expression stack
                                                   // word from old location
    __ str(c_rarg2, Address(c_rarg3, 0));          // and store it at new location
    __ add(c_rarg3, c_rarg3, wordSize);            // advance to next word
    __ bind(entry);
    __ cmp(c_rarg3, c_rarg1);        // check if bottom reached
    __ br(Assembler::NE, loop);      // if not at bottom then
                                     // copy next word
  }

  // call run-time routine
  // c_rarg1: points to monitor entry
  __ bind(allocated);

  // Increment bcp to point to the next bytecode, so exception
  // handling for async. exceptions work correctly.
  // The object has already been poped from the stack, so the
  // expression stack looks correct.
  __ increment(rbcp);

  // store object
  __ str(r0, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
  __ lock_object(c_rarg1);

  // check to make sure this monitor doesn't cause stack overflow after locking
  __ save_bcp();  // in case of exception
  __ generate_stack_overflow_check(0);

  // The bcp has already been incremented. Just need to dispatch to
  // next instruction.
  __ dispatch_next(vtos);
}


void TemplateTable::monitorexit()
{
  transition(atos, vtos);

  // check for NULL object
  __ null_check(r0);

  const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
  const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

  Label found;

  // find matching slot
  {
    Label entry, loop;
    __ ldr(c_rarg1, monitor_block_top); // points to current entry,
                                        // starting with top-most entry
    __ lea(c_rarg2, monitor_block_bot); // points to word before bottom
                                        // of monitor block
    __ b(entry);

    __ bind(loop);
    // check if current entry is for same object
    __ ldr(rscratch1, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
    __ cmp(r0, rscratch1);
    // if same object then stop searching
    __ br(Assembler::EQ, found);
    // otherwise advance to next entry
    __ add(c_rarg1, c_rarg1, entry_size);
    __ bind(entry);
    // check if bottom reached
    __ cmp(c_rarg1, c_rarg2);
    // if not at bottom then check this entry
    __ br(Assembler::NE, loop);
  }

  // error handling. Unlocking was not block-structured
  __ call_VM(noreg, CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
  __ should_not_reach_here();

  // call run-time routine
  // rsi: points to monitor entry
  __ bind(found);
  __ push_ptr(r0); // make sure object is on stack (contract with oopMaps)
  __ unlock_object(c_rarg1);
  __ pop_ptr(r0); // discard object
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
