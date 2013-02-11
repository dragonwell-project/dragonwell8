/*
 * Copyright (c) 1999, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "nativeInst_aarch64.hpp"
#include "runtime/sharedRuntime.hpp"
#include "vmreg_aarch64.inline.hpp"
#ifndef SERIALGC
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif


#define __ ce->masm()->

float ConversionStub::float_zero = 0.0;
double ConversionStub::double_zero = 0.0;

void ConversionStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

void CounterOverflowStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

RangeCheckStub::RangeCheckStub(CodeEmitInfo* info, LIR_Opr index,
                               bool throw_index_out_of_bounds_exception)
  : _throw_index_out_of_bounds_exception(throw_index_out_of_bounds_exception)
  , _index(index) { Unimplemented(); }


void RangeCheckStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

void DivByZeroStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


// Implementation of NewInstanceStub

NewInstanceStub::NewInstanceStub(LIR_Opr klass_reg, LIR_Opr result, ciInstanceKlass* klass, CodeEmitInfo* info, Runtime1::StubID stub_id) { Unimplemented(); }


void NewInstanceStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


// Implementation of NewTypeArrayStub

NewTypeArrayStub::NewTypeArrayStub(LIR_Opr klass_reg, LIR_Opr length, LIR_Opr result, CodeEmitInfo* info) { Unimplemented(); }


void NewTypeArrayStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


// Implementation of NewObjectArrayStub

NewObjectArrayStub::NewObjectArrayStub(LIR_Opr klass_reg, LIR_Opr length, LIR_Opr result, CodeEmitInfo* info) { Unimplemented(); }


void NewObjectArrayStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


// Implementation of MonitorAccessStubs

MonitorEnterStub::MonitorEnterStub(LIR_Opr obj_reg, LIR_Opr lock_reg, CodeEmitInfo* info)
: MonitorAccessStub(obj_reg, lock_reg) { Unimplemented(); }


void MonitorEnterStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


void MonitorExitStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


// Implementation of patching:
// - Copy the code at given offset to an inlined buffer (first the bytes, then the number of bytes)
// - Replace original code with a call to the stub
// At Runtime:
// - call to stub, jump to runtime
// - in runtime: preserve all registers (rspecially objects, i.e., source and destination object)
// - in runtime: after initializing class, restore original code, reexecute instruction

int PatchingStub::_patch_info_offset = 0;

void PatchingStub::align_patch_site(MacroAssembler* masm) { Unimplemented(); }

void PatchingStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


void DeoptimizeStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


void ImplicitNullCheckStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


void SimpleExceptionStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }


void ArrayCopyStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

/////////////////////////////////////////////////////////////////////////////
#ifndef SERIALGC

void G1PreBarrierStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

<<<<<<< HEAD
void G1UnsafeGetObjSATBBarrierStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

=======
>>>>>>> 014bf92... Exhume c1 files.
jbyte* G1PostBarrierStub::_byte_map_base = NULL;

jbyte* G1PostBarrierStub::byte_map_base_slow() { Unimplemented(); return 0; }


void G1PostBarrierStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

#endif // SERIALGC
/////////////////////////////////////////////////////////////////////////////

#undef __
