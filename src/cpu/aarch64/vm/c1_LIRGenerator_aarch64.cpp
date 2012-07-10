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


LIR_Opr LIRGenerator::result_register_for(ValueType* type, bool callee) { Unimplemented(); return LIR_OprFact::illegalOpr; }


LIR_Opr LIRGenerator::rlock_byte(BasicType type) { Unimplemented(); return LIR_OprFact::illegalOpr; }


//--------- loading items into registers --------------------------------


// i486 instructions can inline constants
bool LIRGenerator::can_store_as_constant(Value v, BasicType type) const { Unimplemented(); return false; }


bool LIRGenerator::can_inline_as_constant(Value v) const { Unimplemented(); return false; }


bool LIRGenerator::can_inline_as_constant(LIR_Const* c) const { Unimplemented(); return false; }


LIR_Opr LIRGenerator::safepoint_poll_register() { Unimplemented(); return LIR_OprFact::illegalOpr; }


LIR_Address* LIRGenerator::generate_address(LIR_Opr base, LIR_Opr index,
                                            int shift, int disp, BasicType type) { Unimplemented(); return 0; }


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
void LIRGenerator::do_ArithmeticOp_FPU(ArithmeticOp* x) { Unimplemented(); }

// for  _ladd, _lmul, _lsub, _ldiv, _lrem
void LIRGenerator::do_ArithmeticOp_Long(ArithmeticOp* x) { Unimplemented(); }

// for: _iadd, _imul, _isub, _idiv, _irem
void LIRGenerator::do_ArithmeticOp_Int(ArithmeticOp* x) { Unimplemented(); }

void LIRGenerator::do_ArithmeticOp(ArithmeticOp* x) { Unimplemented(); }

// _ishl, _lshl, _ishr, _lshr, _iushr, _lushr
void LIRGenerator::do_ShiftOp(ShiftOp* x) { Unimplemented(); }

// _iand, _land, _ior, _lor, _ixor, _lxor
void LIRGenerator::do_LogicOp(LogicOp* x) { Unimplemented(); }

// _lcmp, _fcmpl, _fcmpg, _dcmpl, _dcmpg
void LIRGenerator::do_CompareOp(CompareOp* x) { Unimplemented(); }

void LIRGenerator::do_AttemptUpdate(Intrinsic* x) { Unimplemented(); }

void LIRGenerator::do_CompareAndSwap(Intrinsic* x, ValueType* type) { Unimplemented(); }

void LIRGenerator::do_MathIntrinsic(Intrinsic* x) { Unimplemented(); }

void LIRGenerator::do_ArrayCopy(Intrinsic* x) { Unimplemented(); }

// _i2l, _i2f, _i2d, _l2i, _l2f, _l2d, _f2i, _f2l, _f2d, _d2i, _d2l, _d2f
// _i2b, _i2c, _i2s
LIR_Opr fixed_register_for(BasicType type) { Unimplemented(); }

void LIRGenerator::do_Convert(Convert* x) { Unimplemented(); }

void LIRGenerator::do_NewInstance(NewInstance* x) { Unimplemented(); }

void LIRGenerator::do_NewTypeArray(NewTypeArray* x) { Unimplemented(); }

void LIRGenerator::do_NewObjectArray(NewObjectArray* x) { Unimplemented(); }

void LIRGenerator::do_NewMultiArray(NewMultiArray* x) { Unimplemented(); }

void LIRGenerator::do_BlockBegin(BlockBegin* x) { Unimplemented(); }

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
