/*
 * Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "c1/c1_Compilation.hpp"
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_Instruction.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_LIRGenerator.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_ValueStack.hpp"
#include "ci/ciArray.hpp"
#include "ci/ciObjArrayKlass.hpp"
#include "ci/ciTypeArrayKlass.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "vmreg_aarch64.inline.hpp"

#ifdef ASSERT
#define __ gen()->lir(__FILE__, __LINE__)->
#else
#define __ gen()->lir()->
#endif

// Item will be loaded into a byte register; Intel only
void LIRItem::load_byte_item() {
  load_item();
  LIR_Opr res = result();

  if (!res->is_virtual() || !_gen->is_vreg_flag_set(res, LIRGenerator::byte_reg)) {
    // make sure that it is a byte register
    assert(!value()->type()->is_float() && !value()->type()->is_double(),
           "can't load floats in byte register");
    LIR_Opr reg = _gen->rlock_byte(T_BYTE);
    __ move(res, reg);

    _result = reg;
  }
}


void LIRItem::load_nonconstant() {
  LIR_Opr r = value()->operand();
  if (r->is_constant()) {
    _result = r;
  } else {
    load_item();
  }
}

//--------------------------------------------------------------
//               LIRGenerator
//--------------------------------------------------------------


LIR_Opr LIRGenerator::exceptionOopOpr() { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::exceptionPcOpr()  { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::divInOpr()        { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::divOutOpr()       { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::remOutOpr()       { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::shiftCountOpr()   { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::syncTempOpr()     { Unimplemented(); return LIR_OprFact::illegalOpr; }
LIR_Opr LIRGenerator::getThreadTemp()   { Unimplemented(); LIR_OprFact::illegalOpr; }


LIR_Opr LIRGenerator::result_register_for(ValueType* type, bool callee) {
  LIR_Opr opr;
  switch (type->tag()) {
    case intTag:     opr = FrameMap::r0_opr;          break;
    case objectTag:  opr = FrameMap::r0_oop_opr;      break;
    case longTag:    opr = FrameMap::long0_opr;        break;
    case floatTag:   opr = FrameMap::fpu0_float_opr;  break;
    case doubleTag:  opr = FrameMap::fpu0_double_opr;  break;

    case addressTag:
    default: ShouldNotReachHere(); return LIR_OprFact::illegalOpr;
  }

  assert(opr->type_field() == as_OprType(as_BasicType(type)), "type mismatch");
  return opr;
}


LIR_Opr LIRGenerator::rlock_byte(BasicType type) { Unimplemented(); return LIR_OprFact::illegalOpr; }


//--------- loading items into registers --------------------------------


// i486 instructions can inline constants
bool LIRGenerator::can_store_as_constant(Value v, BasicType type) const { return false; }


bool LIRGenerator::can_inline_as_constant(Value v) const {
  // FIXME: Just a guess
  if (v->type()->as_IntConstant() != NULL) {
    return v->type()->as_IntConstant()->value() == 0;
  } else if (v->type()->as_LongConstant() != NULL) {
    return v->type()->as_LongConstant()->value() == 0L;
  } else if (v->type()->as_ObjectConstant() != NULL) {
    return v->type()->as_ObjectConstant()->value()->is_null_object();
  } else {
    return false;
  }
}


bool LIRGenerator::can_inline_as_constant(LIR_Const* c) const { return false; }


LIR_Opr LIRGenerator::safepoint_poll_register() { Unimplemented(); return LIR_OprFact::illegalOpr; }


LIR_Address* LIRGenerator::generate_address(LIR_Opr base, LIR_Opr index,
                                            int shift, int disp, BasicType type) {
  assert(base->is_register(), "must be");

  // accumulate fixed displacements
  if (index->is_constant()) {
    disp += index->as_constant_ptr()->as_jint() << shift;
    index = LIR_OprFact::illegalOpr;
  }

  if (index->is_register()) {
    // apply the shift and accumulate the displacement
    if (shift > 0) {
      LIR_Opr tmp = new_pointer_register();
      __ shift_left(index, shift, tmp);
      index = tmp;
    }
    if (disp != 0) {
      LIR_Opr tmp = new_pointer_register();
      if (Assembler::is_simm13(disp)) {
        __ add(tmp, LIR_OprFact::intptrConst(disp), tmp);
        index = tmp;
      } else {
        __ move(LIR_OprFact::intptrConst(disp), tmp);
        __ add(tmp, index, tmp);
        index = tmp;
      }
      disp = 0;
    }
  } else if (disp != 0 && !Assembler::is_simm13(disp)) {
    // index is illegal so replace it with the displacement loaded into a register
    index = new_pointer_register();
    __ move(LIR_OprFact::intptrConst(disp), index);
    disp = 0;
  }

  // at this point we either have base + index or base + displacement
  if (disp == 0) {
    return new LIR_Address(base, index, type);
  } else {
    assert(Assembler::is_simm13(disp), "must be");
    return new LIR_Address(base, disp, type);
  }
}


LIR_Address* LIRGenerator::emit_array_address(LIR_Opr array_opr, LIR_Opr index_opr,
                                              BasicType type, bool needs_card_mark) { Unimplemented(); return 0; }


LIR_Opr LIRGenerator::load_immediate(int x, BasicType type) { Unimplemented(); return LIR_OprFact::illegalOpr; }

void LIRGenerator::increment_counter(address counter, BasicType type, int step) { Unimplemented(); }


void LIRGenerator::increment_counter(LIR_Address* addr, int step) { Unimplemented(); }

void LIRGenerator::cmp_mem_int(LIR_Condition condition, LIR_Opr base, int disp, int c, CodeEmitInfo* info) { Unimplemented(); }

void LIRGenerator::cmp_reg_mem(LIR_Condition condition, LIR_Opr reg, LIR_Opr base, int disp, BasicType type, CodeEmitInfo* info) { Unimplemented(); }


void LIRGenerator::cmp_reg_mem(LIR_Condition condition, LIR_Opr reg, LIR_Opr base, LIR_Opr disp, BasicType type, CodeEmitInfo* info) { Unimplemented(); }


bool LIRGenerator::strength_reduce_multiply(LIR_Opr left, int c, LIR_Opr result, LIR_Opr tmp) { Unimplemented(); return false; }


void LIRGenerator::store_stack_parameter (LIR_Opr item, ByteSize offset_from_sp) { Unimplemented(); }

//----------------------------------------------------------------------
//             visitor functions
//----------------------------------------------------------------------


void LIRGenerator::do_StoreIndexed(StoreIndexed* x) { Unimplemented(); }

void LIRGenerator::do_MonitorEnter(MonitorEnter* x) { Unimplemented(); }

void LIRGenerator::do_MonitorExit(MonitorExit* x) { Unimplemented(); }

// _ineg, _lneg, _fneg, _dneg
void LIRGenerator::do_NegateOp(NegateOp* x) { Unimplemented(); }

// for  _fadd, _fmul, _fsub, _fdiv, _frem
//      _dadd, _dmul, _dsub, _ddiv, _drem
void LIRGenerator::do_ArithmeticOp_FPU(ArithmeticOp* x) {
  LIRItem left(x->x(),  this);
  LIRItem right(x->y(), this);
  LIRItem* left_arg  = &left;
  LIRItem* right_arg = &right;

  if (!(right.is_register() || right.is_constant()))
    right.load_item();
  if (!left.is_register())
    left.load_item();

  // do not load right operand if it is a constant.  only 0 and 1 are
  // loaded because there are special instructions for loading them
  // without memory access (not needed for SSE2 instructions)
  bool must_load_right = false;
  if (right.is_constant()) {
    LIR_Const* c = right.result()->as_constant_ptr();
    assert(c != NULL, "invalid constant");
    assert(c->type() == T_FLOAT || c->type() == T_DOUBLE, "invalid type");
    must_load_right = true;
  }

  if (right.is_register() || must_load_right) {
    right.load_item();
  } else {
    right.dont_load_item();
  }

  LIR_Opr reg = rlock(x);
  LIR_Opr tmp = LIR_OprFact::illegalOpr;
  if (x->is_strictfp() && (x->op() == Bytecodes::_dmul || x->op() == Bytecodes::_ddiv)) {
    tmp = new_register(T_DOUBLE);
  }

  arithmetic_op_fpu(x->op(), reg, left.result(), right.result(), NULL);

  set_result(x, round_item(reg));
}

// for  _ladd, _lmul, _lsub, _ldiv, _lrem
void LIRGenerator::do_ArithmeticOp_Long(ArithmeticOp* x) {
  // missing test if instr is commutative and if we should swap
  LIRItem left(x->x(), this);
  LIRItem right(x->y(), this);

  left.load_item();
  // don't load constants to save register
  right.load_nonconstant();
  rlock_result(x);
  arithmetic_op_long(x->op(), x->operand(), left.result(), right.result(), NULL);
}

// for: _iadd, _imul, _isub, _idiv, _irem
void LIRGenerator::do_ArithmeticOp_Int(ArithmeticOp* x) { Unimplemented(); }

void LIRGenerator::do_ArithmeticOp(ArithmeticOp* x) {
  // when an operand with use count 1 is the left operand, then it is
  // likely that no move for 2-operand-LIR-form is necessary
  if (x->is_commutative() && x->y()->as_Constant() == NULL && x->x()->use_count() > x->y()->use_count()) {
    x->swap_operands();
  }

  ValueTag tag = x->type()->tag();
  assert(x->x()->type()->tag() == tag && x->y()->type()->tag() == tag, "wrong parameters");
  switch (tag) {
    case floatTag:
    case doubleTag:  do_ArithmeticOp_FPU(x);  return;
    case longTag:    do_ArithmeticOp_Long(x); return;
    case intTag:     do_ArithmeticOp_Int(x);  return;
  }
  ShouldNotReachHere();
}

// _ishl, _lshl, _ishr, _lshr, _iushr, _lushr
void LIRGenerator::do_ShiftOp(ShiftOp* x) { Unimplemented(); }

// _iand, _land, _ior, _lor, _ixor, _lxor
void LIRGenerator::do_LogicOp(LogicOp* x) { Unimplemented(); }

// _lcmp, _fcmpl, _fcmpg, _dcmpl, _dcmpg
void LIRGenerator::do_CompareOp(CompareOp* x) { Unimplemented(); }

void LIRGenerator::do_CompareAndSwap(Intrinsic* x, ValueType* type) { Unimplemented(); }

void LIRGenerator::do_MathIntrinsic(Intrinsic* x) { Unimplemented(); }

void LIRGenerator::do_ArrayCopy(Intrinsic* x) { Unimplemented(); }

// _i2l, _i2f, _i2d, _l2i, _l2f, _l2d, _f2i, _f2l, _f2d, _d2i, _d2l, _d2f
// _i2b, _i2c, _i2s
LIR_Opr fixed_register_for(BasicType type) { Unimplemented(); }

void LIRGenerator::do_Convert(Convert* x) {
  bool fixed_input, fixed_result, round_result, needs_stub;

  switch (x->op()) {
    case Bytecodes::_i2l: // fall through
    case Bytecodes::_l2i: // fall through
    case Bytecodes::_i2b: // fall through
    case Bytecodes::_i2c: // fall through
    case Bytecodes::_i2s: fixed_input = false;       fixed_result = false;       round_result = false;      needs_stub = false; break;

    case Bytecodes::_f2d: fixed_input = false;       fixed_result = false;       round_result = false;      needs_stub = false; break;
    case Bytecodes::_d2f: fixed_input = false;       fixed_result = UseSSE == 1; round_result = UseSSE < 1; needs_stub = false; break;
    case Bytecodes::_i2f: fixed_input = false;       fixed_result = false;       round_result = UseSSE < 1; needs_stub = false; break;
    case Bytecodes::_i2d: fixed_input = false;       fixed_result = false;       round_result = false;      needs_stub = false; break;
    case Bytecodes::_f2i: fixed_input = false;       fixed_result = false;       round_result = false;      needs_stub = true;  break;
    case Bytecodes::_d2i: fixed_input = false;       fixed_result = false;       round_result = false;      needs_stub = true;  break;
    case Bytecodes::_l2f: fixed_input = false;       fixed_result = false;       round_result = UseSSE < 1; needs_stub = false; break;
    case Bytecodes::_l2d: fixed_input = false;       fixed_result = false;       round_result = UseSSE < 2; needs_stub = false; break;
    case Bytecodes::_f2l: fixed_input = true;        fixed_result = true;        round_result = false;      needs_stub = false; break;
    case Bytecodes::_d2l: fixed_input = true;        fixed_result = true;        round_result = false;      needs_stub = false; break;
    default: ShouldNotReachHere();
  }

  LIRItem value(x->value(), this);
  value.load_item();
  LIR_Opr input = value.result();
  LIR_Opr result = rlock(x);

  // arguments of lir_convert
  LIR_Opr conv_input = input;
  LIR_Opr conv_result = result;
  ConversionStub* stub = NULL;

  if (fixed_input) {
    conv_input = fixed_register_for(input->type());
    __ move(input, conv_input);
  }

  assert(fixed_result == false || round_result == false, "cannot set both");
  if (fixed_result) {
    conv_result = fixed_register_for(result->type());
  } else if (round_result) {
    result = new_register(result->type());
    set_vreg_flag(result, must_start_in_memory);
  }

  if (needs_stub) {
    stub = new ConversionStub(x->op(), conv_input, conv_result);
  }

  __ convert(x->op(), conv_input, conv_result, stub, new_register(T_INT));

  if (result != conv_result) {
    __ move(conv_result, result);
  }

  assert(result->is_virtual(), "result must be virtual register");
  set_result(x, result);
}

void LIRGenerator::do_NewInstance(NewInstance* x) { Unimplemented(); }

void LIRGenerator::do_NewTypeArray(NewTypeArray* x) {
  CodeEmitInfo* info = state_for(x, x->state());

  LIRItem length(x->length(), this);
  length.load_item_force(FrameMap::r19_opr);

  LIR_Opr reg = result_register_for(x->type());
  LIR_Opr tmp1 = FrameMap::r2_oop_opr;
  LIR_Opr tmp2 = FrameMap::r4_oop_opr;
  LIR_Opr tmp3 = FrameMap::r5_oop_opr;
  LIR_Opr tmp4 = reg;
  LIR_Opr klass_reg = FrameMap::r3_metadata_opr;
  LIR_Opr len = length.result();
  BasicType elem_type = x->elt_type();

  __ metadata2reg(ciTypeArrayKlass::make(elem_type)->constant_encoding(), klass_reg);

  CodeStub* slow_path = new NewTypeArrayStub(klass_reg, len, reg, info);
  __ allocate_array(reg, len, tmp1, tmp2, tmp3, tmp4, elem_type, klass_reg, slow_path);

  LIR_Opr result = rlock_result(x);
  __ move(reg, result);
}

void LIRGenerator::do_NewObjectArray(NewObjectArray* x) { Unimplemented(); }

void LIRGenerator::do_NewMultiArray(NewMultiArray* x) { Unimplemented(); }

void LIRGenerator::do_BlockBegin(BlockBegin* x) {
  // nothing to do for now
}

void LIRGenerator::do_CheckCast(CheckCast* x) { Unimplemented(); }

void LIRGenerator::do_InstanceOf(InstanceOf* x) { Unimplemented(); }

void LIRGenerator::do_If(If* x) { Unimplemented(); }

LIR_Opr LIRGenerator::getThreadPointer() { Unimplemented(); return LIR_OprFact::illegalOpr; }

void LIRGenerator::trace_block_entry(BlockBegin* block) { Unimplemented(); }

void LIRGenerator::volatile_field_store(LIR_Opr value, LIR_Address* address,
                                        CodeEmitInfo* info) { Unimplemented(); }

void LIRGenerator::volatile_field_load(LIR_Address* address, LIR_Opr result,
                                       CodeEmitInfo* info) { Unimplemented(); }
void LIRGenerator::get_Object_unsafe(LIR_Opr dst, LIR_Opr src, LIR_Opr offset,
                                     BasicType type, bool is_volatile) { Unimplemented(); }

void LIRGenerator::put_Object_unsafe(LIR_Opr src, LIR_Opr offset, LIR_Opr data,
                                     BasicType type, bool is_volatile) { Unimplemented(); }

void LIRGenerator::do_UnsafeGetAndSetObject(UnsafeGetAndSetObject* x) { Unimplemented(); }
