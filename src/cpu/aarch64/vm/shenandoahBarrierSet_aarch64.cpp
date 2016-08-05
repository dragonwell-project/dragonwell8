/*
 * Copyright (c) 2015, Red Hat, Inc. and/or its affiliates.
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

#include "gc_implementation/shenandoah/brooksPointer.hpp"
#include "gc_implementation/shenandoah/shenandoahBarrierSet.inline.hpp"

#include "asm/macroAssembler.hpp"
#include "interpreter/interpreter.hpp"

#define __ masm->

#ifndef CC_INTERP
void ShenandoahBarrierSet::compile_resolve_oop_runtime(MacroAssembler* masm, Register dst) {
  __ enter();
  __ push_call_clobbered_registers();

  __ mov(c_rarg0, dst);
  __ mov(rscratch1, CAST_FROM_FN_PTR(address, ShenandoahBarrierSet::resolve_oop_static));
  __ blrt(rscratch1, 1, 0, 1);
  __ mov(rscratch1, r0);

  __ pop_call_clobbered_registers();
  __ mov(dst, rscratch1);
  __ leave();
}

void ShenandoahBarrierSet::interpreter_read_barrier(MacroAssembler* masm, Register dst) {
  if (ShenandoahReadBarrier) {
    Label is_null;
    __ cbz(dst, is_null);
    interpreter_read_barrier_not_null(masm, dst);
    __ bind(is_null);
  }
}

void ShenandoahBarrierSet::interpreter_read_barrier_not_null(MacroAssembler* masm, Register dst) {
  if (ShenandoahReadBarrier) {
    if (ShenandoahVerifyReadsToFromSpace) {
      compile_resolve_oop_runtime(masm, dst);
      return;
    }
    __ ldr(dst, Address(dst, BrooksPointer::BYTE_OFFSET));
  }
}

void ShenandoahBarrierSet::interpreter_write_barrier(MacroAssembler* masm, Register dst) {

  if (! ShenandoahWriteBarrier) {
    return interpreter_read_barrier(masm, dst);
  }

  assert(dst != rscratch1, "different regs");
  assert(dst != rscratch2, "Need rscratch2");

  Label done;

  Address evacuation_in_progress
    = Address(rthread, in_bytes(JavaThread::evacuation_in_progress_offset()));

  __ ldrb(rscratch2, evacuation_in_progress);
  __ membar(Assembler::LoadLoad);

  // Now check if evacuation is in progress.
  interpreter_read_barrier_not_null(masm, dst);

  __ cbzw(rscratch2, done);

  __ lsr(rscratch1, dst, ShenandoahHeapRegion::RegionSizeShift);
  __ mov(rscratch2,  ShenandoahHeap::in_cset_fast_test_addr());
  __ ldrb(rscratch2, Address(rscratch2, rscratch1));
  __ tst(rscratch2, 0x1);
  __ br(Assembler::EQ, done);

  // Save possibly live regs.
  RegSet live_regs = RegSet::range(r0, r4) - dst;
  __ push(live_regs, sp);
  __ strd(v0, __ pre(sp, 2 * -wordSize));

  // Call into runtime
  __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, ShenandoahBarrierSet::write_barrier_interp), dst);

  // Move result into dst reg.
  __ mov(dst, r0);

  // Restore possibly live regs.
  __ ldrd(v0, __ post(sp, 2 * wordSize));
  __ pop(live_regs, sp);

  __ bind(done);
}

void ShenandoahHeap::compile_prepare_oop(MacroAssembler* masm, Register obj) {
  __ add(obj, obj, BrooksPointer::BROOKS_POINTER_OBJ_SIZE * HeapWordSize);
  __ str(obj, Address(obj, -1 * HeapWordSize));
}

void ShenandoahBarrierSet::asm_acmp_barrier(MacroAssembler* masm,
                                            Register op1, Register op2) {
  Label done;
  __ br(Assembler::EQ, done);
  // The object may have been evacuated, but we won't see it without a
  // membar here.
  __ membar(Assembler::LoadStore|Assembler::LoadLoad);
  interpreter_read_barrier(masm, op1);
  interpreter_read_barrier(masm, op2);
  __ cmp(op1, op2);
  __ bind(done);
}

#endif
