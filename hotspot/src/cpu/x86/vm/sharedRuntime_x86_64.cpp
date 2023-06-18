/*
 * Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.
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
#ifndef _WINDOWS
#include "alloca.h"
#endif
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/debugInfoRec.hpp"
#include "code/icBuffer.hpp"
#include "code/vtableStubs.hpp"
#include "interpreter/interpreter.hpp"
#include "oops/compiledICHolder.hpp"
#include "prims/jvmtiRedefineClassesTrace.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/vframeArray.hpp"
#include "vmreg_x86.inline.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#endif
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

#include "runtime/coroutine.hpp"

void coroutine_start(Coroutine* coroutine, jobject coroutineObj);


#define __ masm->

const int StackAlignmentInSlots = StackAlignmentInBytes / VMRegImpl::stack_slot_size;

class SimpleRuntimeFrame {

  public:

  // Most of the runtime stubs have this simple frame layout.
  // This class exists to make the layout shared in one place.
  // Offsets are for compiler stack slots, which are jints.
  enum layout {
    // The frame sender code expects that rbp will be in the "natural" place and
    // will override any oopMap setting for it. We must therefore force the layout
    // so that it agrees with the frame sender code.
    rbp_off = frame::arg_reg_save_area_bytes/BytesPerInt,
    rbp_off2,
    return_off, return_off2,
    framesize
  };
};

class RegisterSaver {
  // Capture info about frame layout.  Layout offsets are in jint
  // units because compiler frame slots are jints.
#define DEF_XMM_OFFS(regnum) xmm ## regnum ## _off = xmm_off + (regnum)*16/BytesPerInt, xmm ## regnum ## H_off
  enum layout {
    fpu_state_off = frame::arg_reg_save_area_bytes/BytesPerInt, // fxsave save area
    xmm_off       = fpu_state_off + 160/BytesPerInt,            // offset in fxsave save area
    DEF_XMM_OFFS(0),
    DEF_XMM_OFFS(1),
    DEF_XMM_OFFS(2),
    DEF_XMM_OFFS(3),
    DEF_XMM_OFFS(4),
    DEF_XMM_OFFS(5),
    DEF_XMM_OFFS(6),
    DEF_XMM_OFFS(7),
    DEF_XMM_OFFS(8),
    DEF_XMM_OFFS(9),
    DEF_XMM_OFFS(10),
    DEF_XMM_OFFS(11),
    DEF_XMM_OFFS(12),
    DEF_XMM_OFFS(13),
    DEF_XMM_OFFS(14),
    DEF_XMM_OFFS(15),
    fpu_state_end = fpu_state_off + ((FPUStateSizeInWords-1)*wordSize / BytesPerInt),
    fpu_stateH_end,
    r15_off, r15H_off,
    r14_off, r14H_off,
    r13_off, r13H_off,
    r12_off, r12H_off,
    r11_off, r11H_off,
    r10_off, r10H_off,
    r9_off,  r9H_off,
    r8_off,  r8H_off,
    rdi_off, rdiH_off,
    rsi_off, rsiH_off,
    ignore_off, ignoreH_off,  // extra copy of rbp
    rsp_off, rspH_off,
    rbx_off, rbxH_off,
    rdx_off, rdxH_off,
    rcx_off, rcxH_off,
    rax_off, raxH_off,
    // 16-byte stack alignment fill word: see MacroAssembler::push/pop_IU_state
    align_off, alignH_off,
    flags_off, flagsH_off,
    // The frame sender code expects that rbp will be in the "natural" place and
    // will override any oopMap setting for it. We must therefore force the layout
    // so that it agrees with the frame sender code.
    rbp_off, rbpH_off,        // copy of rbp we will restore
    return_off, returnH_off,  // slot for return address
    reg_save_size             // size in compiler stack slots
  };

 public:
  static OopMap* save_live_registers(MacroAssembler* masm, int additional_frame_words, int* total_frame_words, bool save_vectors = false);
  static void restore_live_registers(MacroAssembler* masm, bool restore_vectors = false);

  // Offsets into the register save area
  // Used by deoptimization when it is managing result register
  // values on its own

  static int rax_offset_in_bytes(void)    { return BytesPerInt * rax_off; }
  static int rdx_offset_in_bytes(void)    { return BytesPerInt * rdx_off; }
  static int rbx_offset_in_bytes(void)    { return BytesPerInt * rbx_off; }
  static int xmm0_offset_in_bytes(void)   { return BytesPerInt * xmm0_off; }
  static int return_offset_in_bytes(void) { return BytesPerInt * return_off; }

  // During deoptimization only the result registers need to be restored,
  // all the other values have already been extracted.
  static void restore_result_registers(MacroAssembler* masm);
};

OopMap* RegisterSaver::save_live_registers(MacroAssembler* masm, int additional_frame_words, int* total_frame_words, bool save_vectors) {
  int vect_words = 0;
#ifdef COMPILER2
  if (save_vectors) {
    assert(UseAVX > 0, "256bit vectors are supported only with AVX");
    assert(MaxVectorSize == 32, "only 256bit vectors are supported now");
    // Save upper half of YMM registes
    vect_words = 16 * 16 / wordSize;
    additional_frame_words += vect_words;
  }
#else
  assert(!save_vectors, "vectors are generated only by C2");
#endif

  // Always make the frame size 16-byte aligned
  int frame_size_in_bytes = round_to(additional_frame_words*wordSize +
                                     reg_save_size*BytesPerInt, 16);
  // OopMap frame size is in compiler stack slots (jint's) not bytes or words
  int frame_size_in_slots = frame_size_in_bytes / BytesPerInt;
  // The caller will allocate additional_frame_words
  int additional_frame_slots = additional_frame_words*wordSize / BytesPerInt;
  // CodeBlob frame size is in words.
  int frame_size_in_words = frame_size_in_bytes / wordSize;
  *total_frame_words = frame_size_in_words;

  // Save registers, fpu state, and flags.
  // We assume caller has already pushed the return address onto the
  // stack, so rsp is 8-byte aligned here.
  // We push rpb twice in this sequence because we want the real rbp
  // to be under the return like a normal enter.

  __ enter();          // rsp becomes 16-byte aligned here
  __ push_CPU_state(); // Push a multiple of 16 bytes

  if (vect_words > 0) {
    assert(vect_words*wordSize == 256, "");
    __ subptr(rsp, 256); // Save upper half of YMM registes
    __ vextractf128h(Address(rsp,  0),xmm0);
    __ vextractf128h(Address(rsp, 16),xmm1);
    __ vextractf128h(Address(rsp, 32),xmm2);
    __ vextractf128h(Address(rsp, 48),xmm3);
    __ vextractf128h(Address(rsp, 64),xmm4);
    __ vextractf128h(Address(rsp, 80),xmm5);
    __ vextractf128h(Address(rsp, 96),xmm6);
    __ vextractf128h(Address(rsp,112),xmm7);
    __ vextractf128h(Address(rsp,128),xmm8);
    __ vextractf128h(Address(rsp,144),xmm9);
    __ vextractf128h(Address(rsp,160),xmm10);
    __ vextractf128h(Address(rsp,176),xmm11);
    __ vextractf128h(Address(rsp,192),xmm12);
    __ vextractf128h(Address(rsp,208),xmm13);
    __ vextractf128h(Address(rsp,224),xmm14);
    __ vextractf128h(Address(rsp,240),xmm15);
  }
  if (frame::arg_reg_save_area_bytes != 0) {
    // Allocate argument register save area
    __ subptr(rsp, frame::arg_reg_save_area_bytes);
  }

  // Set an oopmap for the call site.  This oopmap will map all
  // oop-registers and debug-info registers as callee-saved.  This
  // will allow deoptimization at this safepoint to find all possible
  // debug-info recordings, as well as let GC find all oops.

  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map = new OopMap(frame_size_in_slots, 0);

#define STACK_OFFSET(x) VMRegImpl::stack2reg((x) + additional_frame_slots)

  map->set_callee_saved(STACK_OFFSET( rax_off ), rax->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( rcx_off ), rcx->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( rdx_off ), rdx->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( rbx_off ), rbx->as_VMReg());
  // rbp location is known implicitly by the frame sender code, needs no oopmap
  // and the location where rbp was saved by is ignored
  map->set_callee_saved(STACK_OFFSET( rsi_off ), rsi->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( rdi_off ), rdi->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r8_off  ), r8->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r9_off  ), r9->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r10_off ), r10->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r11_off ), r11->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r12_off ), r12->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r13_off ), r13->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r14_off ), r14->as_VMReg());
  map->set_callee_saved(STACK_OFFSET( r15_off ), r15->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm0_off ), xmm0->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm1_off ), xmm1->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm2_off ), xmm2->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm3_off ), xmm3->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm4_off ), xmm4->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm5_off ), xmm5->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm6_off ), xmm6->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm7_off ), xmm7->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm8_off ), xmm8->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm9_off ), xmm9->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm10_off), xmm10->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm11_off), xmm11->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm12_off), xmm12->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm13_off), xmm13->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm14_off), xmm14->as_VMReg());
  map->set_callee_saved(STACK_OFFSET(xmm15_off), xmm15->as_VMReg());

  // %%% These should all be a waste but we'll keep things as they were for now
  if (true) {
    map->set_callee_saved(STACK_OFFSET( raxH_off ), rax->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( rcxH_off ), rcx->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( rdxH_off ), rdx->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( rbxH_off ), rbx->as_VMReg()->next());
    // rbp location is known implicitly by the frame sender code, needs no oopmap
    map->set_callee_saved(STACK_OFFSET( rsiH_off ), rsi->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( rdiH_off ), rdi->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r8H_off  ), r8->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r9H_off  ), r9->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r10H_off ), r10->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r11H_off ), r11->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r12H_off ), r12->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r13H_off ), r13->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r14H_off ), r14->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET( r15H_off ), r15->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm0H_off ), xmm0->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm1H_off ), xmm1->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm2H_off ), xmm2->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm3H_off ), xmm3->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm4H_off ), xmm4->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm5H_off ), xmm5->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm6H_off ), xmm6->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm7H_off ), xmm7->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm8H_off ), xmm8->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm9H_off ), xmm9->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm10H_off), xmm10->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm11H_off), xmm11->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm12H_off), xmm12->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm13H_off), xmm13->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm14H_off), xmm14->as_VMReg()->next());
    map->set_callee_saved(STACK_OFFSET(xmm15H_off), xmm15->as_VMReg()->next());
  }

  return map;
}

void RegisterSaver::restore_live_registers(MacroAssembler* masm, bool restore_vectors) {
  if (frame::arg_reg_save_area_bytes != 0) {
    // Pop arg register save area
    __ addptr(rsp, frame::arg_reg_save_area_bytes);
  }
#ifdef COMPILER2
  if (restore_vectors) {
    // Restore upper half of YMM registes.
    assert(UseAVX > 0, "256bit vectors are supported only with AVX");
    assert(MaxVectorSize == 32, "only 256bit vectors are supported now");
    __ vinsertf128h(xmm0, Address(rsp,  0));
    __ vinsertf128h(xmm1, Address(rsp, 16));
    __ vinsertf128h(xmm2, Address(rsp, 32));
    __ vinsertf128h(xmm3, Address(rsp, 48));
    __ vinsertf128h(xmm4, Address(rsp, 64));
    __ vinsertf128h(xmm5, Address(rsp, 80));
    __ vinsertf128h(xmm6, Address(rsp, 96));
    __ vinsertf128h(xmm7, Address(rsp,112));
    __ vinsertf128h(xmm8, Address(rsp,128));
    __ vinsertf128h(xmm9, Address(rsp,144));
    __ vinsertf128h(xmm10, Address(rsp,160));
    __ vinsertf128h(xmm11, Address(rsp,176));
    __ vinsertf128h(xmm12, Address(rsp,192));
    __ vinsertf128h(xmm13, Address(rsp,208));
    __ vinsertf128h(xmm14, Address(rsp,224));
    __ vinsertf128h(xmm15, Address(rsp,240));
    __ addptr(rsp, 256);
  }
#else
  assert(!restore_vectors, "vectors are generated only by C2");
#endif
  // Recover CPU state
  __ pop_CPU_state();
  // Get the rbp described implicitly by the calling convention (no oopMap)
  __ pop(rbp);
}

void RegisterSaver::restore_result_registers(MacroAssembler* masm) {

  // Just restore result register. Only used by deoptimization. By
  // now any callee save register that needs to be restored to a c2
  // caller of the deoptee has been extracted into the vframeArray
  // and will be stuffed into the c2i adapter we create for later
  // restoration so only result registers need to be restored here.

  // Restore fp result register
  __ movdbl(xmm0, Address(rsp, xmm0_offset_in_bytes()));
  // Restore integer result register
  __ movptr(rax, Address(rsp, rax_offset_in_bytes()));
  __ movptr(rdx, Address(rsp, rdx_offset_in_bytes()));

  // Pop all of the register save are off the stack except the return address
  __ addptr(rsp, return_offset_in_bytes());
}

// Is vector's size (in bytes) bigger than a size saved by default?
// 16 bytes XMM registers are saved by default using fxsave/fxrstor instructions.
bool SharedRuntime::is_wide_vector(int size) {
  return size > 16;
}

// The java_calling_convention describes stack locations as ideal slots on
// a frame with no abi restrictions. Since we must observe abi restrictions
// (like the placement of the register window) the slots must be biased by
// the following value.
static int reg2offset_in(VMReg r) {
  // Account for saved rbp and return address
  // This should really be in_preserve_stack_slots
  return (r->reg2stack() + 4) * VMRegImpl::stack_slot_size;
}

static int reg2offset_out(VMReg r) {
  return (r->reg2stack() + SharedRuntime::out_preserve_stack_slots()) * VMRegImpl::stack_slot_size;
}

// ---------------------------------------------------------------------------
// Read the array of BasicTypes from a signature, and compute where the
// arguments should go.  Values in the VMRegPair regs array refer to 4-byte
// quantities.  Values less than VMRegImpl::stack0 are registers, those above
// refer to 4-byte stack slots.  All stack slots are based off of the stack pointer
// as framesizes are fixed.
// VMRegImpl::stack0 refers to the first slot 0(sp).
// and VMRegImpl::stack0+1 refers to the memory word 4-byes higher.  Register
// up to RegisterImpl::number_of_registers) are the 64-bit
// integer registers.

// Note: the INPUTS in sig_bt are in units of Java argument words, which are
// either 32-bit or 64-bit depending on the build.  The OUTPUTS are in 32-bit
// units regardless of build. Of course for i486 there is no 64 bit build

// The Java calling convention is a "shifted" version of the C ABI.
// By skipping the first C ABI register we can call non-static jni methods
// with small numbers of arguments without having to shuffle the arguments
// at all. Since we control the java ABI we ought to at least get some
// advantage out of it.

int SharedRuntime::java_calling_convention(const BasicType *sig_bt,
                                           VMRegPair *regs,
                                           int total_args_passed,
                                           int is_outgoing) {

  // Create the mapping between argument positions and
  // registers.
  static const Register INT_ArgReg[Argument::n_int_register_parameters_j] = {
    j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5
  };
  static const XMMRegister FP_ArgReg[Argument::n_float_register_parameters_j] = {
    j_farg0, j_farg1, j_farg2, j_farg3,
    j_farg4, j_farg5, j_farg6, j_farg7
  };


  uint int_args = 0;
  uint fp_args = 0;
  uint stk_args = 0; // inc by 2 each time

  for (int i = 0; i < total_args_passed; i++) {
    switch (sig_bt[i]) {
    case T_BOOLEAN:
    case T_CHAR:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
      if (int_args < Argument::n_int_register_parameters_j) {
        regs[i].set1(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_VOID:
      // halves of T_LONG or T_DOUBLE
      assert(i != 0 && (sig_bt[i - 1] == T_LONG || sig_bt[i - 1] == T_DOUBLE), "expecting half");
      regs[i].set_bad();
      break;
    case T_LONG:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      // fall through
    case T_OBJECT:
    case T_ARRAY:
    case T_ADDRESS:
      if (int_args < Argument::n_int_register_parameters_j) {
        regs[i].set2(INT_ArgReg[int_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_FLOAT:
      if (fp_args < Argument::n_float_register_parameters_j) {
        regs[i].set1(FP_ArgReg[fp_args++]->as_VMReg());
      } else {
        regs[i].set1(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    case T_DOUBLE:
      assert(sig_bt[i + 1] == T_VOID, "expecting half");
      if (fp_args < Argument::n_float_register_parameters_j) {
        regs[i].set2(FP_ArgReg[fp_args++]->as_VMReg());
      } else {
        regs[i].set2(VMRegImpl::stack2reg(stk_args));
        stk_args += 2;
      }
      break;
    default:
      ShouldNotReachHere();
      break;
    }
  }

  return round_to(stk_args, 2);
}

// Patch the callers callsite with entry to compiled code if it exists.
static void patch_callers_callsite(MacroAssembler *masm) {
  Label L;
  __ cmpptr(Address(rbx, in_bytes(Method::code_offset())), (int32_t)NULL_WORD);
  __ jcc(Assembler::equal, L);

  // Save the current stack pointer
  __ mov(r13, rsp);
  // Schedule the branch target address early.
  // Call into the VM to patch the caller, then jump to compiled callee
  // rax isn't live so capture return address while we easily can
  __ movptr(rax, Address(rsp, 0));

  // align stack so push_CPU_state doesn't fault
  __ andptr(rsp, -(StackAlignmentInBytes));
  __ push_CPU_state();

  // VM needs caller's callsite
  // VM needs target method
  // This needs to be a long call since we will relocate this adapter to
  // the codeBuffer and it may not reach

  // Allocate argument register save area
  if (frame::arg_reg_save_area_bytes != 0) {
    __ subptr(rsp, frame::arg_reg_save_area_bytes);
  }
  __ mov(c_rarg0, rbx);
  __ mov(c_rarg1, rax);
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::fixup_callers_callsite)));

  // De-allocate argument register save area
  if (frame::arg_reg_save_area_bytes != 0) {
    __ addptr(rsp, frame::arg_reg_save_area_bytes);
  }

  __ pop_CPU_state();
  // restore sp
  __ mov(rsp, r13);
  __ bind(L);
}


static void gen_c2i_adapter(MacroAssembler *masm,
                            int total_args_passed,
                            int comp_args_on_stack,
                            const BasicType *sig_bt,
                            const VMRegPair *regs,
                            Label& skip_fixup) {
  // Before we get into the guts of the C2I adapter, see if we should be here
  // at all.  We've come from compiled code and are attempting to jump to the
  // interpreter, which means the caller made a static call to get here
  // (vcalls always get a compiled target if there is one).  Check for a
  // compiled target.  If there is one, we need to patch the caller's call.
  patch_callers_callsite(masm);

  __ bind(skip_fixup);

  // Since all args are passed on the stack, total_args_passed *
  // Interpreter::stackElementSize is the space we need. Plus 1 because
  // we also account for the return address location since
  // we store it first rather than hold it in rax across all the shuffling

  int extraspace = (total_args_passed * Interpreter::stackElementSize) + wordSize;

  // stack is aligned, keep it that way
  extraspace = round_to(extraspace, 2*wordSize);

  // Get return address
  __ pop(rax);

  // set senderSP value
  __ mov(r13, rsp);

  __ subptr(rsp, extraspace);

  // Store the return address in the expected location
  __ movptr(Address(rsp, 0), rax);

  // Now write the args into the outgoing interpreter space
  for (int i = 0; i < total_args_passed; i++) {
    if (sig_bt[i] == T_VOID) {
      assert(i > 0 && (sig_bt[i-1] == T_LONG || sig_bt[i-1] == T_DOUBLE), "missing half");
      continue;
    }

    // offset to start parameters
    int st_off   = (total_args_passed - i) * Interpreter::stackElementSize;
    int next_off = st_off - Interpreter::stackElementSize;

    // Say 4 args:
    // i   st_off
    // 0   32 T_LONG
    // 1   24 T_VOID
    // 2   16 T_OBJECT
    // 3    8 T_BOOL
    // -    0 return address
    //
    // However to make thing extra confusing. Because we can fit a long/double in
    // a single slot on a 64 bt vm and it would be silly to break them up, the interpreter
    // leaves one slot empty and only stores to a single slot. In this case the
    // slot that is occupied is the T_VOID slot. See I said it was confusing.

    VMReg r_1 = regs[i].first();
    VMReg r_2 = regs[i].second();
    if (!r_1->is_valid()) {
      assert(!r_2->is_valid(), "");
      continue;
    }
    if (r_1->is_stack()) {
      // memory to memory use rax
      int ld_off = r_1->reg2stack() * VMRegImpl::stack_slot_size + extraspace;
      if (!r_2->is_valid()) {
        // sign extend??
        __ movl(rax, Address(rsp, ld_off));
        __ movptr(Address(rsp, st_off), rax);

      } else {

        __ movq(rax, Address(rsp, ld_off));

        // Two VMREgs|OptoRegs can be T_OBJECT, T_ADDRESS, T_DOUBLE, T_LONG
        // T_DOUBLE and T_LONG use two slots in the interpreter
        if ( sig_bt[i] == T_LONG || sig_bt[i] == T_DOUBLE) {
          // ld_off == LSW, ld_off+wordSize == MSW
          // st_off == MSW, next_off == LSW
          __ movq(Address(rsp, next_off), rax);
#ifdef ASSERT
          // Overwrite the unused slot with known junk
          __ mov64(rax, CONST64(0xdeadffffdeadaaaa));
          __ movptr(Address(rsp, st_off), rax);
#endif /* ASSERT */
        } else {
          __ movq(Address(rsp, st_off), rax);
        }
      }
    } else if (r_1->is_Register()) {
      Register r = r_1->as_Register();
      if (!r_2->is_valid()) {
        // must be only an int (or less ) so move only 32bits to slot
        // why not sign extend??
        __ movl(Address(rsp, st_off), r);
      } else {
        // Two VMREgs|OptoRegs can be T_OBJECT, T_ADDRESS, T_DOUBLE, T_LONG
        // T_DOUBLE and T_LONG use two slots in the interpreter
        if ( sig_bt[i] == T_LONG || sig_bt[i] == T_DOUBLE) {
          // long/double in gpr
#ifdef ASSERT
          // Overwrite the unused slot with known junk
          __ mov64(rax, CONST64(0xdeadffffdeadaaab));
          __ movptr(Address(rsp, st_off), rax);
#endif /* ASSERT */
          __ movq(Address(rsp, next_off), r);
        } else {
          __ movptr(Address(rsp, st_off), r);
        }
      }
    } else {
      assert(r_1->is_XMMRegister(), "");
      if (!r_2->is_valid()) {
        // only a float use just part of the slot
        __ movflt(Address(rsp, st_off), r_1->as_XMMRegister());
      } else {
#ifdef ASSERT
        // Overwrite the unused slot with known junk
        __ mov64(rax, CONST64(0xdeadffffdeadaaac));
        __ movptr(Address(rsp, st_off), rax);
#endif /* ASSERT */
        __ movdbl(Address(rsp, next_off), r_1->as_XMMRegister());
      }
    }
  }

  // Schedule the branch target address early.
  __ movptr(rcx, Address(rbx, in_bytes(Method::interpreter_entry_offset())));
  __ jmp(rcx);
}

static void range_check(MacroAssembler* masm, Register pc_reg, Register temp_reg,
                        address code_start, address code_end,
                        Label& L_ok) {
  Label L_fail;
  __ lea(temp_reg, ExternalAddress(code_start));
  __ cmpptr(pc_reg, temp_reg);
  __ jcc(Assembler::belowEqual, L_fail);
  __ lea(temp_reg, ExternalAddress(code_end));
  __ cmpptr(pc_reg, temp_reg);
  __ jcc(Assembler::below, L_ok);
  __ bind(L_fail);
}

static void gen_i2c_adapter(MacroAssembler *masm,
                            int total_args_passed,
                            int comp_args_on_stack,
                            const BasicType *sig_bt,
                            const VMRegPair *regs) {

  // Note: r13 contains the senderSP on entry. We must preserve it since
  // we may do a i2c -> c2i transition if we lose a race where compiled
  // code goes non-entrant while we get args ready.
  // In addition we use r13 to locate all the interpreter args as
  // we must align the stack to 16 bytes on an i2c entry else we
  // lose alignment we expect in all compiled code and register
  // save code can segv when fxsave instructions find improperly
  // aligned stack pointer.

  // Adapters can be frameless because they do not require the caller
  // to perform additional cleanup work, such as correcting the stack pointer.
  // An i2c adapter is frameless because the *caller* frame, which is interpreted,
  // routinely repairs its own stack pointer (from interpreter_frame_last_sp),
  // even if a callee has modified the stack pointer.
  // A c2i adapter is frameless because the *callee* frame, which is interpreted,
  // routinely repairs its caller's stack pointer (from sender_sp, which is set
  // up via the senderSP register).
  // In other words, if *either* the caller or callee is interpreted, we can
  // get the stack pointer repaired after a call.
  // This is why c2i and i2c adapters cannot be indefinitely composed.
  // In particular, if a c2i adapter were to somehow call an i2c adapter,
  // both caller and callee would be compiled methods, and neither would
  // clean up the stack pointer changes performed by the two adapters.
  // If this happens, control eventually transfers back to the compiled
  // caller, but with an uncorrected stack, causing delayed havoc.

  // Pick up the return address
  __ movptr(rax, Address(rsp, 0));

  if (VerifyAdapterCalls &&
      (Interpreter::code() != NULL || StubRoutines::code1() != NULL)) {
    // So, let's test for cascading c2i/i2c adapters right now.
    //  assert(Interpreter::contains($return_addr) ||
    //         StubRoutines::contains($return_addr),
    //         "i2c adapter must return to an interpreter frame");
    __ block_comment("verify_i2c { ");
    Label L_ok;
    if (Interpreter::code() != NULL)
      range_check(masm, rax, r11,
                  Interpreter::code()->code_start(), Interpreter::code()->code_end(),
                  L_ok);
    if (StubRoutines::code1() != NULL)
      range_check(masm, rax, r11,
                  StubRoutines::code1()->code_begin(), StubRoutines::code1()->code_end(),
                  L_ok);
    if (StubRoutines::code2() != NULL)
      range_check(masm, rax, r11,
                  StubRoutines::code2()->code_begin(), StubRoutines::code2()->code_end(),
                  L_ok);
    const char* msg = "i2c adapter must return to an interpreter frame";
    __ block_comment(msg);
    __ stop(msg);
    __ bind(L_ok);
    __ block_comment("} verify_i2ce ");
  }

  // Must preserve original SP for loading incoming arguments because
  // we need to align the outgoing SP for compiled code.
  __ movptr(r11, rsp);

  // Cut-out for having no stack args.  Since up to 2 int/oop args are passed
  // in registers, we will occasionally have no stack args.
  int comp_words_on_stack = 0;
  if (comp_args_on_stack) {
    // Sig words on the stack are greater-than VMRegImpl::stack0.  Those in
    // registers are below.  By subtracting stack0, we either get a negative
    // number (all values in registers) or the maximum stack slot accessed.

    // Convert 4-byte c2 stack slots to words.
    comp_words_on_stack = round_to(comp_args_on_stack*VMRegImpl::stack_slot_size, wordSize)>>LogBytesPerWord;
    // Round up to miminum stack alignment, in wordSize
    comp_words_on_stack = round_to(comp_words_on_stack, 2);
    __ subptr(rsp, comp_words_on_stack * wordSize);
  }


  // Ensure compiled code always sees stack at proper alignment
  __ andptr(rsp, -16);

  // push the return address and misalign the stack that youngest frame always sees
  // as far as the placement of the call instruction
  __ push(rax);

  // Put saved SP in another register
  const Register saved_sp = rax;
  __ movptr(saved_sp, r11);

  // Will jump to the compiled code just as if compiled code was doing it.
  // Pre-load the register-jump target early, to schedule it better.
  __ movptr(r11, Address(rbx, in_bytes(Method::from_compiled_offset())));

  // Now generate the shuffle code.  Pick up all register args and move the
  // rest through the floating point stack top.
  for (int i = 0; i < total_args_passed; i++) {
    if (sig_bt[i] == T_VOID) {
      // Longs and doubles are passed in native word order, but misaligned
      // in the 32-bit build.
      assert(i > 0 && (sig_bt[i-1] == T_LONG || sig_bt[i-1] == T_DOUBLE), "missing half");
      continue;
    }

    // Pick up 0, 1 or 2 words from SP+offset.

    assert(!regs[i].second()->is_valid() || regs[i].first()->next() == regs[i].second(),
            "scrambled load targets?");
    // Load in argument order going down.
    int ld_off = (total_args_passed - i)*Interpreter::stackElementSize;
    // Point to interpreter value (vs. tag)
    int next_off = ld_off - Interpreter::stackElementSize;
    //
    //
    //
    VMReg r_1 = regs[i].first();
    VMReg r_2 = regs[i].second();
    if (!r_1->is_valid()) {
      assert(!r_2->is_valid(), "");
      continue;
    }
    if (r_1->is_stack()) {
      // Convert stack slot to an SP offset (+ wordSize to account for return address )
      int st_off = regs[i].first()->reg2stack()*VMRegImpl::stack_slot_size + wordSize;

      // We can use r13 as a temp here because compiled code doesn't need r13 as an input
      // and if we end up going thru a c2i because of a miss a reasonable value of r13
      // will be generated.
      if (!r_2->is_valid()) {
        // sign extend???
        __ movl(r13, Address(saved_sp, ld_off));
        __ movptr(Address(rsp, st_off), r13);
      } else {
        //
        // We are using two optoregs. This can be either T_OBJECT, T_ADDRESS, T_LONG, or T_DOUBLE
        // the interpreter allocates two slots but only uses one for thr T_LONG or T_DOUBLE case
        // So we must adjust where to pick up the data to match the interpreter.
        //
        // Interpreter local[n] == MSW, local[n+1] == LSW however locals
        // are accessed as negative so LSW is at LOW address

        // ld_off is MSW so get LSW
        const int offset = (sig_bt[i]==T_LONG||sig_bt[i]==T_DOUBLE)?
                           next_off : ld_off;
        __ movq(r13, Address(saved_sp, offset));
        // st_off is LSW (i.e. reg.first())
        __ movq(Address(rsp, st_off), r13);
      }
    } else if (r_1->is_Register()) {  // Register argument
      Register r = r_1->as_Register();
      assert(r != rax, "must be different");
      if (r_2->is_valid()) {
        //
        // We are using two VMRegs. This can be either T_OBJECT, T_ADDRESS, T_LONG, or T_DOUBLE
        // the interpreter allocates two slots but only uses one for thr T_LONG or T_DOUBLE case
        // So we must adjust where to pick up the data to match the interpreter.

        const int offset = (sig_bt[i]==T_LONG||sig_bt[i]==T_DOUBLE)?
                           next_off : ld_off;

        // this can be a misaligned move
        __ movq(r, Address(saved_sp, offset));
      } else {
        // sign extend and use a full word?
        __ movl(r, Address(saved_sp, ld_off));
      }
    } else {
      if (!r_2->is_valid()) {
        __ movflt(r_1->as_XMMRegister(), Address(saved_sp, ld_off));
      } else {
        __ movdbl(r_1->as_XMMRegister(), Address(saved_sp, next_off));
      }
    }
  }

  // 6243940 We might end up in handle_wrong_method if
  // the callee is deoptimized as we race thru here. If that
  // happens we don't want to take a safepoint because the
  // caller frame will look interpreted and arguments are now
  // "compiled" so it is much better to make this transition
  // invisible to the stack walking code. Unfortunately if
  // we try and find the callee by normal means a safepoint
  // is possible. So we stash the desired callee in the thread
  // and the vm will find there should this case occur.

  __ movptr(Address(r15_thread, JavaThread::callee_target_offset()), rbx);

  // put Method* where a c2i would expect should we end up there
  // only needed becaus eof c2 resolve stubs return Method* as a result in
  // rax
  __ mov(rax, rbx);
  __ jmp(r11);
}

// ---------------------------------------------------------------
AdapterHandlerEntry* SharedRuntime::generate_i2c2i_adapters(MacroAssembler *masm,
                                                            int total_args_passed,
                                                            int comp_args_on_stack,
                                                            const BasicType *sig_bt,
                                                            const VMRegPair *regs,
                                                            AdapterFingerPrint* fingerprint) {
  address i2c_entry = __ pc();

  gen_i2c_adapter(masm, total_args_passed, comp_args_on_stack, sig_bt, regs);

  // -------------------------------------------------------------------------
  // Generate a C2I adapter.  On entry we know rbx holds the Method* during calls
  // to the interpreter.  The args start out packed in the compiled layout.  They
  // need to be unpacked into the interpreter layout.  This will almost always
  // require some stack space.  We grow the current (compiled) stack, then repack
  // the args.  We  finally end in a jump to the generic interpreter entry point.
  // On exit from the interpreter, the interpreter will restore our SP (lest the
  // compiled code, which relys solely on SP and not RBP, get sick).

  address c2i_unverified_entry = __ pc();
  Label skip_fixup;
  Label ok;

  Register holder = rax;
  Register receiver = j_rarg0;
  Register temp = rbx;

  {
    __ load_klass(temp, receiver);
    __ cmpptr(temp, Address(holder, CompiledICHolder::holder_klass_offset()));
    __ movptr(rbx, Address(holder, CompiledICHolder::holder_metadata_offset()));
    __ jcc(Assembler::equal, ok);
    __ jump(RuntimeAddress(SharedRuntime::get_ic_miss_stub()));

    __ bind(ok);
    // Method might have been compiled since the call site was patched to
    // interpreted if that is the case treat it as a miss so we can get
    // the call site corrected.
    __ cmpptr(Address(rbx, in_bytes(Method::code_offset())), (int32_t)NULL_WORD);
    __ jcc(Assembler::equal, skip_fixup);
    __ jump(RuntimeAddress(SharedRuntime::get_ic_miss_stub()));
  }

  address c2i_entry = __ pc();

  gen_c2i_adapter(masm, total_args_passed, comp_args_on_stack, sig_bt, regs, skip_fixup);

  __ flush();
  return AdapterHandlerLibrary::new_entry(fingerprint, i2c_entry, c2i_entry, c2i_unverified_entry);
}

int SharedRuntime::c_calling_convention(const BasicType *sig_bt,
                                         VMRegPair *regs,
                                         VMRegPair *regs2,
                                         int total_args_passed) {
  assert(regs2 == NULL, "not needed on x86");
// We return the amount of VMRegImpl stack slots we need to reserve for all
// the arguments NOT counting out_preserve_stack_slots.

// NOTE: These arrays will have to change when c1 is ported
#ifdef _WIN64
    static const Register INT_ArgReg[Argument::n_int_register_parameters_c] = {
      c_rarg0, c_rarg1, c_rarg2, c_rarg3
    };
    static const XMMRegister FP_ArgReg[Argument::n_float_register_parameters_c] = {
      c_farg0, c_farg1, c_farg2, c_farg3
    };
#else
    static const Register INT_ArgReg[Argument::n_int_register_parameters_c] = {
      c_rarg0, c_rarg1, c_rarg2, c_rarg3, c_rarg4, c_rarg5
    };
    static const XMMRegister FP_ArgReg[Argument::n_float_register_parameters_c] = {
      c_farg0, c_farg1, c_farg2, c_farg3,
      c_farg4, c_farg5, c_farg6, c_farg7
    };
#endif // _WIN64


    uint int_args = 0;
    uint fp_args = 0;
    uint stk_args = 0; // inc by 2 each time

    for (int i = 0; i < total_args_passed; i++) {
      switch (sig_bt[i]) {
      case T_BOOLEAN:
      case T_CHAR:
      case T_BYTE:
      case T_SHORT:
      case T_INT:
        if (int_args < Argument::n_int_register_parameters_c) {
          regs[i].set1(INT_ArgReg[int_args++]->as_VMReg());
#ifdef _WIN64
          fp_args++;
          // Allocate slots for callee to stuff register args the stack.
          stk_args += 2;
#endif
        } else {
          regs[i].set1(VMRegImpl::stack2reg(stk_args));
          stk_args += 2;
        }
        break;
      case T_LONG:
        assert(sig_bt[i + 1] == T_VOID, "expecting half");
        // fall through
      case T_OBJECT:
      case T_ARRAY:
      case T_ADDRESS:
      case T_METADATA:
        if (int_args < Argument::n_int_register_parameters_c) {
          regs[i].set2(INT_ArgReg[int_args++]->as_VMReg());
#ifdef _WIN64
          fp_args++;
          stk_args += 2;
#endif
        } else {
          regs[i].set2(VMRegImpl::stack2reg(stk_args));
          stk_args += 2;
        }
        break;
      case T_FLOAT:
        if (fp_args < Argument::n_float_register_parameters_c) {
          regs[i].set1(FP_ArgReg[fp_args++]->as_VMReg());
#ifdef _WIN64
          int_args++;
          // Allocate slots for callee to stuff register args the stack.
          stk_args += 2;
#endif
        } else {
          regs[i].set1(VMRegImpl::stack2reg(stk_args));
          stk_args += 2;
        }
        break;
      case T_DOUBLE:
        assert(sig_bt[i + 1] == T_VOID, "expecting half");
        if (fp_args < Argument::n_float_register_parameters_c) {
          regs[i].set2(FP_ArgReg[fp_args++]->as_VMReg());
#ifdef _WIN64
          int_args++;
          // Allocate slots for callee to stuff register args the stack.
          stk_args += 2;
#endif
        } else {
          regs[i].set2(VMRegImpl::stack2reg(stk_args));
          stk_args += 2;
        }
        break;
      case T_VOID: // Halves of longs and doubles
        assert(i != 0 && (sig_bt[i - 1] == T_LONG || sig_bt[i - 1] == T_DOUBLE), "expecting half");
        regs[i].set_bad();
        break;
      default:
        ShouldNotReachHere();
        break;
      }
    }
#ifdef _WIN64
  // windows abi requires that we always allocate enough stack space
  // for 4 64bit registers to be stored down.
  if (stk_args < 8) {
    stk_args = 8;
  }
#endif // _WIN64

  return stk_args;
}

// On 64 bit we will store integer like items to the stack as
// 64 bits items (sparc abi) even though java would only store
// 32bits for a parameter. On 32bit it will simply be 32 bits
// So this routine will do 32->32 on 32bit and 32->64 on 64bit
static void move32_64(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {
  if (src.first()->is_stack()) {
    if (dst.first()->is_stack()) {
      // stack to stack
      __ movslq(rax, Address(rbp, reg2offset_in(src.first())));
      __ movq(Address(rsp, reg2offset_out(dst.first())), rax);
    } else {
      // stack to reg
      __ movslq(dst.first()->as_Register(), Address(rbp, reg2offset_in(src.first())));
    }
  } else if (dst.first()->is_stack()) {
    // reg to stack
    // Do we really have to sign extend???
    // __ movslq(src.first()->as_Register(), src.first()->as_Register());
    __ movq(Address(rsp, reg2offset_out(dst.first())), src.first()->as_Register());
  } else {
    // Do we really have to sign extend???
    // __ movslq(dst.first()->as_Register(), src.first()->as_Register());
    if (dst.first() != src.first()) {
      __ movq(dst.first()->as_Register(), src.first()->as_Register());
    }
  }
}

static void move_ptr(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {
  if (src.first()->is_stack()) {
    if (dst.first()->is_stack()) {
      // stack to stack
      __ movq(rax, Address(rbp, reg2offset_in(src.first())));
      __ movq(Address(rsp, reg2offset_out(dst.first())), rax);
    } else {
      // stack to reg
      __ movq(dst.first()->as_Register(), Address(rbp, reg2offset_in(src.first())));
    }
  } else if (dst.first()->is_stack()) {
    // reg to stack
    __ movq(Address(rsp, reg2offset_out(dst.first())), src.first()->as_Register());
  } else {
    if (dst.first() != src.first()) {
      __ movq(dst.first()->as_Register(), src.first()->as_Register());
    }
  }
}

// An oop arg. Must pass a handle not the oop itself
static void object_move(MacroAssembler* masm,
                        OopMap* map,
                        int oop_handle_offset,
                        int framesize_in_slots,
                        VMRegPair src,
                        VMRegPair dst,
                        bool is_receiver,
                        int* receiver_offset) {

  // must pass a handle. First figure out the location we use as a handle

  Register rHandle = dst.first()->is_stack() ? rax : dst.first()->as_Register();

  // See if oop is NULL if it is we need no handle

  if (src.first()->is_stack()) {

    // Oop is already on the stack as an argument
    int offset_in_older_frame = src.first()->reg2stack() + SharedRuntime::out_preserve_stack_slots();
    map->set_oop(VMRegImpl::stack2reg(offset_in_older_frame + framesize_in_slots));
    if (is_receiver) {
      *receiver_offset = (offset_in_older_frame + framesize_in_slots) * VMRegImpl::stack_slot_size;
    }

    __ cmpptr(Address(rbp, reg2offset_in(src.first())), (int32_t)NULL_WORD);
    __ lea(rHandle, Address(rbp, reg2offset_in(src.first())));
    // conditionally move a NULL
    __ cmovptr(Assembler::equal, rHandle, Address(rbp, reg2offset_in(src.first())));
  } else {

    // Oop is in an a register we must store it to the space we reserve
    // on the stack for oop_handles and pass a handle if oop is non-NULL

    const Register rOop = src.first()->as_Register();
    int oop_slot;
    if (rOop == j_rarg0)
      oop_slot = 0;
    else if (rOop == j_rarg1)
      oop_slot = 1;
    else if (rOop == j_rarg2)
      oop_slot = 2;
    else if (rOop == j_rarg3)
      oop_slot = 3;
    else if (rOop == j_rarg4)
      oop_slot = 4;
    else {
      assert(rOop == j_rarg5, "wrong register");
      oop_slot = 5;
    }

    oop_slot = oop_slot * VMRegImpl::slots_per_word + oop_handle_offset;
    int offset = oop_slot*VMRegImpl::stack_slot_size;

    map->set_oop(VMRegImpl::stack2reg(oop_slot));
    // Store oop in handle area, may be NULL
    __ movptr(Address(rsp, offset), rOop);
    if (is_receiver) {
      *receiver_offset = offset;
    }

    __ cmpptr(rOop, (int32_t)NULL_WORD);
    __ lea(rHandle, Address(rsp, offset));
    // conditionally move a NULL from the handle area where it was just stored
    __ cmovptr(Assembler::equal, rHandle, Address(rsp, offset));
  }

  // If arg is on the stack then place it otherwise it is already in correct reg.
  if (dst.first()->is_stack()) {
    __ movptr(Address(rsp, reg2offset_out(dst.first())), rHandle);
  }
}

// A float arg may have to do float reg int reg conversion
static void float_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {
  assert(!src.second()->is_valid() && !dst.second()->is_valid(), "bad float_move");

  // The calling conventions assures us that each VMregpair is either
  // all really one physical register or adjacent stack slots.
  // This greatly simplifies the cases here compared to sparc.

  if (src.first()->is_stack()) {
    if (dst.first()->is_stack()) {
      __ movl(rax, Address(rbp, reg2offset_in(src.first())));
      __ movptr(Address(rsp, reg2offset_out(dst.first())), rax);
    } else {
      // stack to reg
      assert(dst.first()->is_XMMRegister(), "only expect xmm registers as parameters");
      __ movflt(dst.first()->as_XMMRegister(), Address(rbp, reg2offset_in(src.first())));
    }
  } else if (dst.first()->is_stack()) {
    // reg to stack
    assert(src.first()->is_XMMRegister(), "only expect xmm registers as parameters");
    __ movflt(Address(rsp, reg2offset_out(dst.first())), src.first()->as_XMMRegister());
  } else {
    // reg to reg
    // In theory these overlap but the ordering is such that this is likely a nop
    if ( src.first() != dst.first()) {
      __ movdbl(dst.first()->as_XMMRegister(),  src.first()->as_XMMRegister());
    }
  }
}

// A long move
static void long_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {

  // The calling conventions assures us that each VMregpair is either
  // all really one physical register or adjacent stack slots.
  // This greatly simplifies the cases here compared to sparc.

  if (src.is_single_phys_reg() ) {
    if (dst.is_single_phys_reg()) {
      if (dst.first() != src.first()) {
        __ mov(dst.first()->as_Register(), src.first()->as_Register());
      }
    } else {
      assert(dst.is_single_reg(), "not a stack pair");
      __ movq(Address(rsp, reg2offset_out(dst.first())), src.first()->as_Register());
    }
  } else if (dst.is_single_phys_reg()) {
    assert(src.is_single_reg(),  "not a stack pair");
    __ movq(dst.first()->as_Register(), Address(rbp, reg2offset_out(src.first())));
  } else {
    assert(src.is_single_reg() && dst.is_single_reg(), "not stack pairs");
    __ movq(rax, Address(rbp, reg2offset_in(src.first())));
    __ movq(Address(rsp, reg2offset_out(dst.first())), rax);
  }
}

// A double move
static void double_move(MacroAssembler* masm, VMRegPair src, VMRegPair dst) {

  // The calling conventions assures us that each VMregpair is either
  // all really one physical register or adjacent stack slots.
  // This greatly simplifies the cases here compared to sparc.

  if (src.is_single_phys_reg() ) {
    if (dst.is_single_phys_reg()) {
      // In theory these overlap but the ordering is such that this is likely a nop
      if ( src.first() != dst.first()) {
        __ movdbl(dst.first()->as_XMMRegister(), src.first()->as_XMMRegister());
      }
    } else {
      assert(dst.is_single_reg(), "not a stack pair");
      __ movdbl(Address(rsp, reg2offset_out(dst.first())), src.first()->as_XMMRegister());
    }
  } else if (dst.is_single_phys_reg()) {
    assert(src.is_single_reg(),  "not a stack pair");
    __ movdbl(dst.first()->as_XMMRegister(), Address(rbp, reg2offset_out(src.first())));
  } else {
    assert(src.is_single_reg() && dst.is_single_reg(), "not stack pairs");
    __ movq(rax, Address(rbp, reg2offset_in(src.first())));
    __ movq(Address(rsp, reg2offset_out(dst.first())), rax);
  }
}


void SharedRuntime::save_native_result(MacroAssembler *masm, BasicType ret_type, int frame_slots) {
  // We always ignore the frame_slots arg and just use the space just below frame pointer
  // which by this time is free to use
  switch (ret_type) {
  case T_FLOAT:
    __ movflt(Address(rbp, -wordSize), xmm0);
    break;
  case T_DOUBLE:
    __ movdbl(Address(rbp, -wordSize), xmm0);
    break;
  case T_VOID:  break;
  default: {
    __ movptr(Address(rbp, -wordSize), rax);
    }
  }
}

void SharedRuntime::restore_native_result(MacroAssembler *masm, BasicType ret_type, int frame_slots) {
  // We always ignore the frame_slots arg and just use the space just below frame pointer
  // which by this time is free to use
  switch (ret_type) {
  case T_FLOAT:
    __ movflt(xmm0, Address(rbp, -wordSize));
    break;
  case T_DOUBLE:
    __ movdbl(xmm0, Address(rbp, -wordSize));
    break;
  case T_VOID:  break;
  default: {
    __ movptr(rax, Address(rbp, -wordSize));
    }
  }
}

static void save_args(MacroAssembler *masm, int arg_count, int first_arg, VMRegPair *args) {
    for ( int i = first_arg ; i < arg_count ; i++ ) {
      if (args[i].first()->is_Register()) {
        __ push(args[i].first()->as_Register());
      } else if (args[i].first()->is_XMMRegister()) {
        __ subptr(rsp, 2*wordSize);
        __ movdbl(Address(rsp, 0), args[i].first()->as_XMMRegister());
      }
    }
}

static void restore_args(MacroAssembler *masm, int arg_count, int first_arg, VMRegPair *args) {
    for ( int i = arg_count - 1 ; i >= first_arg ; i-- ) {
      if (args[i].first()->is_Register()) {
        __ pop(args[i].first()->as_Register());
      } else if (args[i].first()->is_XMMRegister()) {
        __ movdbl(args[i].first()->as_XMMRegister(), Address(rsp, 0));
        __ addptr(rsp, 2*wordSize);
      }
    }
}


static void save_or_restore_arguments(MacroAssembler* masm,
                                      const int stack_slots,
                                      const int total_in_args,
                                      const int arg_save_area,
                                      OopMap* map,
                                      VMRegPair* in_regs,
                                      BasicType* in_sig_bt) {
  // if map is non-NULL then the code should store the values,
  // otherwise it should load them.
  int slot = arg_save_area;
  // Save down double word first
  for ( int i = 0; i < total_in_args; i++) {
    if (in_regs[i].first()->is_XMMRegister() && in_sig_bt[i] == T_DOUBLE) {
      int offset = slot * VMRegImpl::stack_slot_size;
      slot += VMRegImpl::slots_per_word;
      assert(slot <= stack_slots, "overflow");
      if (map != NULL) {
        __ movdbl(Address(rsp, offset), in_regs[i].first()->as_XMMRegister());
      } else {
        __ movdbl(in_regs[i].first()->as_XMMRegister(), Address(rsp, offset));
      }
    }
    if (in_regs[i].first()->is_Register() &&
        (in_sig_bt[i] == T_LONG || in_sig_bt[i] == T_ARRAY)) {
      int offset = slot * VMRegImpl::stack_slot_size;
      if (map != NULL) {
        __ movq(Address(rsp, offset), in_regs[i].first()->as_Register());
        if (in_sig_bt[i] == T_ARRAY) {
          map->set_oop(VMRegImpl::stack2reg(slot));;
        }
      } else {
        __ movq(in_regs[i].first()->as_Register(), Address(rsp, offset));
      }
      slot += VMRegImpl::slots_per_word;
    }
  }
  // Save or restore single word registers
  for ( int i = 0; i < total_in_args; i++) {
    if (in_regs[i].first()->is_Register()) {
      int offset = slot * VMRegImpl::stack_slot_size;
      slot++;
      assert(slot <= stack_slots, "overflow");

      // Value is in an input register pass we must flush it to the stack
      const Register reg = in_regs[i].first()->as_Register();
      switch (in_sig_bt[i]) {
        case T_BOOLEAN:
        case T_CHAR:
        case T_BYTE:
        case T_SHORT:
        case T_INT:
          if (map != NULL) {
            __ movl(Address(rsp, offset), reg);
          } else {
            __ movl(reg, Address(rsp, offset));
          }
          break;
        case T_ARRAY:
        case T_LONG:
          // handled above
          break;
        case T_OBJECT:
        default: ShouldNotReachHere();
      }
    } else if (in_regs[i].first()->is_XMMRegister()) {
      if (in_sig_bt[i] == T_FLOAT) {
        int offset = slot * VMRegImpl::stack_slot_size;
        slot++;
        assert(slot <= stack_slots, "overflow");
        if (map != NULL) {
          __ movflt(Address(rsp, offset), in_regs[i].first()->as_XMMRegister());
        } else {
          __ movflt(in_regs[i].first()->as_XMMRegister(), Address(rsp, offset));
        }
      }
    } else if (in_regs[i].first()->is_stack()) {
      if (in_sig_bt[i] == T_ARRAY && map != NULL) {
        int offset_in_older_frame = in_regs[i].first()->reg2stack() + SharedRuntime::out_preserve_stack_slots();
        map->set_oop(VMRegImpl::stack2reg(offset_in_older_frame + stack_slots));
      }
    }
  }
}


// Check GC_locker::needs_gc and enter the runtime if it's true.  This
// keeps a new JNI critical region from starting until a GC has been
// forced.  Save down any oops in registers and describe them in an
// OopMap.
static void check_needs_gc_for_critical_native(MacroAssembler* masm,
                                               int stack_slots,
                                               int total_c_args,
                                               int total_in_args,
                                               int arg_save_area,
                                               OopMapSet* oop_maps,
                                               VMRegPair* in_regs,
                                               BasicType* in_sig_bt) {
  __ block_comment("check GC_locker::needs_gc");
  Label cont;
  __ cmp8(ExternalAddress((address)GC_locker::needs_gc_address()), false);
  __ jcc(Assembler::equal, cont);

  // Save down any incoming oops and call into the runtime to halt for a GC

  OopMap* map = new OopMap(stack_slots * 2, 0 /* arg_slots*/);
  save_or_restore_arguments(masm, stack_slots, total_in_args,
                            arg_save_area, map, in_regs, in_sig_bt);

  address the_pc = __ pc();
  oop_maps->add_gc_map( __ offset(), map);
  __ set_last_Java_frame(rsp, noreg, the_pc);

  __ block_comment("block_for_jni_critical");
  __ movptr(c_rarg0, r15_thread);
  __ mov(r12, rsp); // remember sp
  __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
  __ andptr(rsp, -16); // align stack as required by ABI
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::block_for_jni_critical)));
  __ mov(rsp, r12); // restore sp
  __ reinit_heapbase();

  __ reset_last_Java_frame(false);

  save_or_restore_arguments(masm, stack_slots, total_in_args,
                            arg_save_area, NULL, in_regs, in_sig_bt);

  __ bind(cont);
#ifdef ASSERT
  if (StressCriticalJNINatives) {
    // Stress register saving
    OopMap* map = new OopMap(stack_slots * 2, 0 /* arg_slots*/);
    save_or_restore_arguments(masm, stack_slots, total_in_args,
                              arg_save_area, map, in_regs, in_sig_bt);
    // Destroy argument registers
    for (int i = 0; i < total_in_args - 1; i++) {
      if (in_regs[i].first()->is_Register()) {
        const Register reg = in_regs[i].first()->as_Register();
        __ xorptr(reg, reg);
      } else if (in_regs[i].first()->is_XMMRegister()) {
        __ xorpd(in_regs[i].first()->as_XMMRegister(), in_regs[i].first()->as_XMMRegister());
      } else if (in_regs[i].first()->is_FloatRegister()) {
        ShouldNotReachHere();
      } else if (in_regs[i].first()->is_stack()) {
        // Nothing to do
      } else {
        ShouldNotReachHere();
      }
      if (in_sig_bt[i] == T_LONG || in_sig_bt[i] == T_DOUBLE) {
        i++;
      }
    }

    save_or_restore_arguments(masm, stack_slots, total_in_args,
                              arg_save_area, NULL, in_regs, in_sig_bt);
  }
#endif
}

// Unpack an array argument into a pointer to the body and the length
// if the array is non-null, otherwise pass 0 for both.
static void unpack_array_argument(MacroAssembler* masm, VMRegPair reg, BasicType in_elem_type, VMRegPair body_arg, VMRegPair length_arg) {
  Register tmp_reg = rax;
  assert(!body_arg.first()->is_Register() || body_arg.first()->as_Register() != tmp_reg,
         "possible collision");
  assert(!length_arg.first()->is_Register() || length_arg.first()->as_Register() != tmp_reg,
         "possible collision");

  __ block_comment("unpack_array_argument {");

  // Pass the length, ptr pair
  Label is_null, done;
  VMRegPair tmp;
  tmp.set_ptr(tmp_reg->as_VMReg());
  if (reg.first()->is_stack()) {
    // Load the arg up from the stack
    move_ptr(masm, reg, tmp);
    reg = tmp;
  }
  __ testptr(reg.first()->as_Register(), reg.first()->as_Register());
  __ jccb(Assembler::equal, is_null);
  __ lea(tmp_reg, Address(reg.first()->as_Register(), arrayOopDesc::base_offset_in_bytes(in_elem_type)));
  move_ptr(masm, tmp, body_arg);
  // load the length relative to the body.
  __ movl(tmp_reg, Address(tmp_reg, arrayOopDesc::length_offset_in_bytes() -
                           arrayOopDesc::base_offset_in_bytes(in_elem_type)));
  move32_64(masm, tmp, length_arg);
  __ jmpb(done);
  __ bind(is_null);
  // Pass zeros
  __ xorptr(tmp_reg, tmp_reg);
  move_ptr(masm, tmp, body_arg);
  move32_64(masm, tmp, length_arg);
  __ bind(done);

  __ block_comment("} unpack_array_argument");
}


// Different signatures may require very different orders for the move
// to avoid clobbering other arguments.  There's no simple way to
// order them safely.  Compute a safe order for issuing stores and
// break any cycles in those stores.  This code is fairly general but
// it's not necessary on the other platforms so we keep it in the
// platform dependent code instead of moving it into a shared file.
// (See bugs 7013347 & 7145024.)
// Note that this code is specific to LP64.
class ComputeMoveOrder: public StackObj {
  class MoveOperation: public ResourceObj {
    friend class ComputeMoveOrder;
   private:
    VMRegPair        _src;
    VMRegPair        _dst;
    int              _src_index;
    int              _dst_index;
    bool             _processed;
    MoveOperation*  _next;
    MoveOperation*  _prev;

    static int get_id(VMRegPair r) {
      return r.first()->value();
    }

   public:
    MoveOperation(int src_index, VMRegPair src, int dst_index, VMRegPair dst):
      _src(src)
    , _src_index(src_index)
    , _dst(dst)
    , _dst_index(dst_index)
    , _next(NULL)
    , _prev(NULL)
    , _processed(false) {
    }

    VMRegPair src() const              { return _src; }
    int src_id() const                 { return get_id(src()); }
    int src_index() const              { return _src_index; }
    VMRegPair dst() const              { return _dst; }
    void set_dst(int i, VMRegPair dst) { _dst_index = i, _dst = dst; }
    int dst_index() const              { return _dst_index; }
    int dst_id() const                 { return get_id(dst()); }
    MoveOperation* next() const       { return _next; }
    MoveOperation* prev() const       { return _prev; }
    void set_processed()               { _processed = true; }
    bool is_processed() const          { return _processed; }

    // insert
    void break_cycle(VMRegPair temp_register) {
      // create a new store following the last store
      // to move from the temp_register to the original
      MoveOperation* new_store = new MoveOperation(-1, temp_register, dst_index(), dst());

      // break the cycle of links and insert new_store at the end
      // break the reverse link.
      MoveOperation* p = prev();
      assert(p->next() == this, "must be");
      _prev = NULL;
      p->_next = new_store;
      new_store->_prev = p;

      // change the original store to save it's value in the temp.
      set_dst(-1, temp_register);
    }

    void link(GrowableArray<MoveOperation*>& killer) {
      // link this store in front the store that it depends on
      MoveOperation* n = killer.at_grow(src_id(), NULL);
      if (n != NULL) {
        assert(_next == NULL && n->_prev == NULL, "shouldn't have been set yet");
        _next = n;
        n->_prev = this;
      }
    }
  };

 private:
  GrowableArray<MoveOperation*> edges;

 public:
  ComputeMoveOrder(int total_in_args, VMRegPair* in_regs, int total_c_args, VMRegPair* out_regs,
                    BasicType* in_sig_bt, GrowableArray<int>& arg_order, VMRegPair tmp_vmreg) {
    // Move operations where the dest is the stack can all be
    // scheduled first since they can't interfere with the other moves.
    for (int i = total_in_args - 1, c_arg = total_c_args - 1; i >= 0; i--, c_arg--) {
      if (in_sig_bt[i] == T_ARRAY) {
        c_arg--;
        if (out_regs[c_arg].first()->is_stack() &&
            out_regs[c_arg + 1].first()->is_stack()) {
          arg_order.push(i);
          arg_order.push(c_arg);
        } else {
          if (out_regs[c_arg].first()->is_stack() ||
              in_regs[i].first() == out_regs[c_arg].first()) {
            add_edge(i, in_regs[i].first(), c_arg, out_regs[c_arg + 1]);
          } else {
            add_edge(i, in_regs[i].first(), c_arg, out_regs[c_arg]);
          }
        }
      } else if (in_sig_bt[i] == T_VOID) {
        arg_order.push(i);
        arg_order.push(c_arg);
      } else {
        if (out_regs[c_arg].first()->is_stack() ||
            in_regs[i].first() == out_regs[c_arg].first()) {
          arg_order.push(i);
          arg_order.push(c_arg);
        } else {
          add_edge(i, in_regs[i].first(), c_arg, out_regs[c_arg]);
        }
      }
    }
    // Break any cycles in the register moves and emit the in the
    // proper order.
    GrowableArray<MoveOperation*>* stores = get_store_order(tmp_vmreg);
    for (int i = 0; i < stores->length(); i++) {
      arg_order.push(stores->at(i)->src_index());
      arg_order.push(stores->at(i)->dst_index());
    }
 }

  // Collected all the move operations
  void add_edge(int src_index, VMRegPair src, int dst_index, VMRegPair dst) {
    if (src.first() == dst.first()) return;
    edges.append(new MoveOperation(src_index, src, dst_index, dst));
  }

  // Walk the edges breaking cycles between moves.  The result list
  // can be walked in order to produce the proper set of loads
  GrowableArray<MoveOperation*>* get_store_order(VMRegPair temp_register) {
    // Record which moves kill which values
    GrowableArray<MoveOperation*> killer;
    for (int i = 0; i < edges.length(); i++) {
      MoveOperation* s = edges.at(i);
      assert(killer.at_grow(s->dst_id(), NULL) == NULL, "only one killer");
      killer.at_put_grow(s->dst_id(), s, NULL);
    }
    assert(killer.at_grow(MoveOperation::get_id(temp_register), NULL) == NULL,
           "make sure temp isn't in the registers that are killed");

    // create links between loads and stores
    for (int i = 0; i < edges.length(); i++) {
      edges.at(i)->link(killer);
    }

    // at this point, all the move operations are chained together
    // in a doubly linked list.  Processing it backwards finds
    // the beginning of the chain, forwards finds the end.  If there's
    // a cycle it can be broken at any point,  so pick an edge and walk
    // backward until the list ends or we end where we started.
    GrowableArray<MoveOperation*>* stores = new GrowableArray<MoveOperation*>();
    for (int e = 0; e < edges.length(); e++) {
      MoveOperation* s = edges.at(e);
      if (!s->is_processed()) {
        MoveOperation* start = s;
        // search for the beginning of the chain or cycle
        while (start->prev() != NULL && start->prev() != s) {
          start = start->prev();
        }
        if (start->prev() == s) {
          start->break_cycle(temp_register);
        }
        // walk the chain forward inserting to store list
        while (start != NULL) {
          stores->append(start);
          start->set_processed();
          start = start->next();
        }
      }
    }
    return stores;
  }
};

static void verify_oop_args(MacroAssembler* masm,
                            methodHandle method,
                            const BasicType* sig_bt,
                            const VMRegPair* regs) {
  Register temp_reg = rbx;  // not part of any compiled calling seq
  if (VerifyOops) {
    for (int i = 0; i < method->size_of_parameters(); i++) {
      if (sig_bt[i] == T_OBJECT ||
          sig_bt[i] == T_ARRAY) {
        VMReg r = regs[i].first();
        assert(r->is_valid(), "bad oop arg");
        if (r->is_stack()) {
          __ movptr(temp_reg, Address(rsp, r->reg2stack() * VMRegImpl::stack_slot_size + wordSize));
          __ verify_oop(temp_reg);
        } else {
          __ verify_oop(r->as_Register());
        }
      }
    }
  }
}

static void gen_special_dispatch(MacroAssembler* masm,
                                 methodHandle method,
                                 const BasicType* sig_bt,
                                 const VMRegPair* regs) {
  verify_oop_args(masm, method, sig_bt, regs);
  vmIntrinsics::ID iid = method->intrinsic_id();

  // Now write the args into the outgoing interpreter space
  bool     has_receiver   = false;
  Register receiver_reg   = noreg;
  int      member_arg_pos = -1;
  Register member_reg     = noreg;
  int      ref_kind       = MethodHandles::signature_polymorphic_intrinsic_ref_kind(iid);
  if (ref_kind != 0) {
    member_arg_pos = method->size_of_parameters() - 1;  // trailing MemberName argument
    member_reg = rbx;  // known to be free at this point
    has_receiver = MethodHandles::ref_kind_has_receiver(ref_kind);
  } else if (iid == vmIntrinsics::_invokeBasic) {
    has_receiver = true;
  } else {
    fatal(err_msg_res("unexpected intrinsic id %d", iid));
  }

  if (member_reg != noreg) {
    // Load the member_arg into register, if necessary.
    SharedRuntime::check_member_name_argument_is_last_argument(method, sig_bt, regs);
    VMReg r = regs[member_arg_pos].first();
    if (r->is_stack()) {
      __ movptr(member_reg, Address(rsp, r->reg2stack() * VMRegImpl::stack_slot_size + wordSize));
    } else {
      // no data motion is needed
      member_reg = r->as_Register();
    }
  }

  if (has_receiver) {
    // Make sure the receiver is loaded into a register.
    assert(method->size_of_parameters() > 0, "oob");
    assert(sig_bt[0] == T_OBJECT, "receiver argument must be an object");
    VMReg r = regs[0].first();
    assert(r->is_valid(), "bad receiver arg");
    if (r->is_stack()) {
      // Porting note:  This assumes that compiled calling conventions always
      // pass the receiver oop in a register.  If this is not true on some
      // platform, pick a temp and load the receiver from stack.
      fatal("receiver always in a register");
      receiver_reg = j_rarg0;  // known to be free at this point
      __ movptr(receiver_reg, Address(rsp, r->reg2stack() * VMRegImpl::stack_slot_size + wordSize));
    } else {
      // no data motion is needed
      receiver_reg = r->as_Register();
    }
  }

  // Figure out which address we are really jumping to:
  MethodHandles::generate_method_handle_dispatch(masm, iid,
                                                 receiver_reg, member_reg, /*for_compiler_entry:*/ true);
}

void create_switchTo_contents(MacroAssembler *masm, int start, OopMapSet* oop_maps, int &stack_slots, int total_in_args, BasicType *in_sig_bt, VMRegPair *in_regs, BasicType ret_type, bool terminate);

void generate_thread_fix(MacroAssembler *masm, Method *method) {
  // we can have a check here at the codegen time, so no cost in runtime.
  if (EnableCoroutine) {
    if (WispStealCandidate(method->method_holder()->name(), method->name(), method->signature()).is_steal_candidate()) {
      // as X86 calling conventions, the thread register r15 is one of the callee saved registers.
      // see: https://www.cs.cmu.edu/~aplatzer/course/Compilers11/calling_conventions.pdf, callee saved registers
      // the r15 register will be saved at method entry, and restored implicitly at method exit
      // so we have to fix it manually here after the method returns.
      WISP_CALLING_CONVENTION_V2J_UPDATE;
    }
  }
}

// ---------------------------------------------------------------------------
// Generate a native wrapper for a given method.  The method takes arguments
// in the Java compiled code convention, marshals them to the native
// convention (handlizes oops, etc), transitions to native, makes the call,
// returns to java state (possibly blocking), unhandlizes any result and
// returns.
//
// Critical native functions are a shorthand for the use of
// GetPrimtiveArrayCritical and disallow the use of any other JNI
// functions.  The wrapper is expected to unpack the arguments before
// passing them to the callee and perform checks before and after the
// native call to ensure that they GC_locker
// lock_critical/unlock_critical semantics are followed.  Some other
// parts of JNI setup are skipped like the tear down of the JNI handle
// block and the check for pending exceptions it's impossible for them
// to be thrown.
//
// They are roughly structured like this:
//    if (GC_locker::needs_gc())
//      SharedRuntime::block_for_jni_critical();
//    tranistion to thread_in_native
//    unpack arrray arguments and call native entry point
//    check for safepoint in progress
//    check if any thread suspend flags are set
//      call into JVM and possible unlock the JNI critical
//      if a GC was suppressed while in the critical native.
//    transition back to thread_in_Java
//    return to caller
//
nmethod* SharedRuntime::generate_native_wrapper(MacroAssembler* masm,
                                                methodHandle method,
                                                int compile_id,
                                                BasicType* in_sig_bt,
                                                VMRegPair* in_regs,
                                                BasicType ret_type) {
  if (method->is_method_handle_intrinsic()) {
    vmIntrinsics::ID iid = method->intrinsic_id();
    intptr_t start = (intptr_t)__ pc();
    int vep_offset = ((intptr_t)__ pc()) - start;
    gen_special_dispatch(masm,
                         method,
                         in_sig_bt,
                         in_regs);
    int frame_complete = ((intptr_t)__ pc()) - start;  // not complete, period
    __ flush();
    int stack_slots = SharedRuntime::out_preserve_stack_slots();  // no out slots at all, actually
    return nmethod::new_native_nmethod(method,
                                       compile_id,
                                       masm->code(),
                                       vep_offset,
                                       frame_complete,
                                       stack_slots / VMRegImpl::slots_per_word,
                                       in_ByteSize(-1),
                                       in_ByteSize(-1),
                                       (OopMapSet*)NULL);
  }
  bool is_critical_native = true;
  address native_func = method->critical_native_function();
  if (native_func == NULL) {
    native_func = method->native_function();
    is_critical_native = false;
  }
  assert(native_func != NULL, "must have function");

  // An OopMap for lock (and class if static)
  OopMapSet *oop_maps = new OopMapSet();
  intptr_t start = (intptr_t)__ pc();

  // We have received a description of where all the java arg are located
  // on entry to the wrapper. We need to convert these args to where
  // the jni function will expect them. To figure out where they go
  // we convert the java signature to a C signature by inserting
  // the hidden arguments as arg[0] and possibly arg[1] (static method)

  const int total_in_args = method->size_of_parameters();
  int total_c_args = total_in_args;
  if (!is_critical_native) {
    total_c_args += 1;
    if (method->is_static()) {
      total_c_args++;
    }
  } else {
    for (int i = 0; i < total_in_args; i++) {
      if (in_sig_bt[i] == T_ARRAY) {
        total_c_args++;
      }
    }
  }

  BasicType* out_sig_bt = NEW_RESOURCE_ARRAY(BasicType, total_c_args);
  VMRegPair* out_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_c_args);
  BasicType* in_elem_bt = NULL;

  int argc = 0;
  if (!is_critical_native) {
    out_sig_bt[argc++] = T_ADDRESS;
    if (method->is_static()) {
      out_sig_bt[argc++] = T_OBJECT;
    }

    for (int i = 0; i < total_in_args ; i++ ) {
      out_sig_bt[argc++] = in_sig_bt[i];
    }
  } else {
    Thread* THREAD = Thread::current();
    in_elem_bt = NEW_RESOURCE_ARRAY(BasicType, total_in_args);
    SignatureStream ss(method->signature());
    for (int i = 0; i < total_in_args ; i++ ) {
      if (in_sig_bt[i] == T_ARRAY) {
        // Arrays are passed as int, elem* pair
        out_sig_bt[argc++] = T_INT;
        out_sig_bt[argc++] = T_ADDRESS;
        Symbol* atype = ss.as_symbol(CHECK_NULL);
        const char* at = atype->as_C_string();
        if (strlen(at) == 2) {
          assert(at[0] == '[', "must be");
          switch (at[1]) {
            case 'B': in_elem_bt[i]  = T_BYTE; break;
            case 'C': in_elem_bt[i]  = T_CHAR; break;
            case 'D': in_elem_bt[i]  = T_DOUBLE; break;
            case 'F': in_elem_bt[i]  = T_FLOAT; break;
            case 'I': in_elem_bt[i]  = T_INT; break;
            case 'J': in_elem_bt[i]  = T_LONG; break;
            case 'S': in_elem_bt[i]  = T_SHORT; break;
            case 'Z': in_elem_bt[i]  = T_BOOLEAN; break;
            default: ShouldNotReachHere();
          }
        }
      } else {
        out_sig_bt[argc++] = in_sig_bt[i];
        in_elem_bt[i] = T_VOID;
      }
      if (in_sig_bt[i] != T_VOID) {
        assert(in_sig_bt[i] == ss.type(), "must match");
        ss.next();
      }
    }
  }

  // Now figure out where the args must be stored and how much stack space
  // they require.
  int out_arg_slots;
  out_arg_slots = c_calling_convention(out_sig_bt, out_regs, NULL, total_c_args);

  // Compute framesize for the wrapper.  We need to handlize all oops in
  // incoming registers

  // Calculate the total number of stack slots we will need.

  // First count the abi requirement plus all of the outgoing args
  int stack_slots = SharedRuntime::out_preserve_stack_slots() + out_arg_slots;

  // Now the space for the inbound oop handle area
  int total_save_slots = 6 * VMRegImpl::slots_per_word;  // 6 arguments passed in registers
  if (is_critical_native) {
    // Critical natives may have to call out so they need a save area
    // for register arguments.
    int double_slots = 0;
    int single_slots = 0;
    for ( int i = 0; i < total_in_args; i++) {
      if (in_regs[i].first()->is_Register()) {
        const Register reg = in_regs[i].first()->as_Register();
        switch (in_sig_bt[i]) {
          case T_BOOLEAN:
          case T_BYTE:
          case T_SHORT:
          case T_CHAR:
          case T_INT:  single_slots++; break;
          case T_ARRAY:  // specific to LP64 (7145024)
          case T_LONG: double_slots++; break;
          default:  ShouldNotReachHere();
        }
      } else if (in_regs[i].first()->is_XMMRegister()) {
        switch (in_sig_bt[i]) {
          case T_FLOAT:  single_slots++; break;
          case T_DOUBLE: double_slots++; break;
          default:  ShouldNotReachHere();
        }
      } else if (in_regs[i].first()->is_FloatRegister()) {
        ShouldNotReachHere();
      }
    }
    total_save_slots = double_slots * 2 + single_slots;
    // align the save area
    if (double_slots != 0) {
      stack_slots = round_to(stack_slots, 2);
    }
  }

  int oop_handle_offset = stack_slots;
  stack_slots += total_save_slots;

  // Now any space we need for handlizing a klass if static method

  int klass_slot_offset = 0;
  int klass_offset = -1;
  int lock_slot_offset = 0;
  bool is_static = false;

  if (method->is_static()) {
    klass_slot_offset = stack_slots;
    stack_slots += VMRegImpl::slots_per_word;
    klass_offset = klass_slot_offset * VMRegImpl::stack_slot_size;
    is_static = true;
  }

  // Plus a lock if needed

  if (method->is_synchronized()) {
    lock_slot_offset = stack_slots;
    stack_slots += VMRegImpl::slots_per_word;
  }

  // Now a place (+2) to save return values or temp during shuffling
  // + 4 for return address (which we own) and saved rbp
  stack_slots += 6;

  // Ok The space we have allocated will look like:
  //
  //
  // FP-> |                     |
  //      |---------------------|
  //      | 2 slots for moves   |
  //      |---------------------|
  //      | lock box (if sync)  |
  //      |---------------------| <- lock_slot_offset
  //      | klass (if static)   |
  //      |---------------------| <- klass_slot_offset
  //      | oopHandle area      |
  //      |---------------------| <- oop_handle_offset (6 java arg registers)
  //      | outbound memory     |
  //      | based arguments     |
  //      |                     |
  //      |---------------------|
  //      |                     |
  // SP-> | out_preserved_slots |
  //
  //


  // Now compute actual number of stack words we need rounding to make
  // stack properly aligned.
  stack_slots = round_to(stack_slots, StackAlignmentInSlots);

  int stack_size = stack_slots * VMRegImpl::stack_slot_size;

  // First thing make an ic check to see if we should even be here

  // We are free to use all registers as temps without saving them and
  // restoring them except rbp. rbp is the only callee save register
  // as far as the interpreter and the compiler(s) are concerned.


  const Register ic_reg = rax;
  const Register receiver = j_rarg0;

  Label hit;
  Label exception_pending;

  assert_different_registers(ic_reg, receiver, rscratch1);
  __ verify_oop(receiver);
  __ load_klass(rscratch1, receiver);
  __ cmpq(ic_reg, rscratch1);
  __ jcc(Assembler::equal, hit);

  __ jump(RuntimeAddress(SharedRuntime::get_ic_miss_stub()));

  // Verified entry point must be aligned
  __ align(8);

  __ bind(hit);

  int vep_offset = ((intptr_t)__ pc()) - start;

  // The instruction at the verified entry point must be 5 bytes or longer
  // because it can be patched on the fly by make_non_entrant. The stack bang
  // instruction fits that requirement.

  // Generate stack overflow check

  if (UseStackBanging) {
    __ bang_stack_with_offset(StackShadowPages*os::vm_page_size());
  } else {
    // need a 5 byte instruction to allow MT safe patching to non-entrant
    __ fat_nop();
  }

  if (EnableCoroutine) {
    // the coroutine support methods have a hand-coded fast version that will handle the most common cases
    if (method->intrinsic_id() == vmIntrinsics::_switchTo) {
      create_switchTo_contents(masm, start, oop_maps, stack_slots, total_in_args, in_sig_bt, in_regs, ret_type, false);
    } else if (method->intrinsic_id() == vmIntrinsics::_switchToAndTerminate ||
        method->intrinsic_id() == vmIntrinsics::_switchToAndExit) {
      create_switchTo_contents(masm, start, oop_maps, stack_slots, total_in_args, in_sig_bt, in_regs, ret_type, true);
    }
  }

  // Generate a new frame for the wrapper.
  __ enter();
  // -2 because return address is already present and so is saved rbp
  __ subptr(rsp, stack_size - 2*wordSize);

  // Frame is now completed as far as size and linkage.
  int frame_complete = ((intptr_t)__ pc()) - start;

    if (UseRTMLocking) {
      // Abort RTM transaction before calling JNI
      // because critical section will be large and will be
      // aborted anyway. Also nmethod could be deoptimized.
      __ xabort(0);
    }

#ifdef ASSERT
    {
      Label L;
      __ mov(rax, rsp);
      __ andptr(rax, -16); // must be 16 byte boundary (see amd64 ABI)
      __ cmpptr(rax, rsp);
      __ jcc(Assembler::equal, L);
      __ stop("improperly aligned stack");
      __ bind(L);
    }
#endif /* ASSERT */


  // We use r14 as the oop handle for the receiver/klass
  // It is callee save so it survives the call to native

  const Register oop_handle_reg = r14;

  if (is_critical_native) {
    check_needs_gc_for_critical_native(masm, stack_slots, total_c_args, total_in_args,
                                       oop_handle_offset, oop_maps, in_regs, in_sig_bt);
  }

  //
  // We immediately shuffle the arguments so that any vm call we have to
  // make from here on out (sync slow path, jvmti, etc.) we will have
  // captured the oops from our caller and have a valid oopMap for
  // them.

  // -----------------
  // The Grand Shuffle

  // The Java calling convention is either equal (linux) or denser (win64) than the
  // c calling convention. However the because of the jni_env argument the c calling
  // convention always has at least one more (and two for static) arguments than Java.
  // Therefore if we move the args from java -> c backwards then we will never have
  // a register->register conflict and we don't have to build a dependency graph
  // and figure out how to break any cycles.
  //

  // Record esp-based slot for receiver on stack for non-static methods
  int receiver_offset = -1;

  // This is a trick. We double the stack slots so we can claim
  // the oops in the caller's frame. Since we are sure to have
  // more args than the caller doubling is enough to make
  // sure we can capture all the incoming oop args from the
  // caller.
  //
  OopMap* map = new OopMap(stack_slots * 2, 0 /* arg_slots*/);

  // Mark location of rbp (someday)
  // map->set_callee_saved(VMRegImpl::stack2reg( stack_slots - 2), stack_slots * 2, 0, vmreg(rbp));

  // Use eax, ebx as temporaries during any memory-memory moves we have to do
  // All inbound args are referenced based on rbp and all outbound args via rsp.


#ifdef ASSERT
  bool reg_destroyed[RegisterImpl::number_of_registers];
  bool freg_destroyed[XMMRegisterImpl::number_of_registers];
  for ( int r = 0 ; r < RegisterImpl::number_of_registers ; r++ ) {
    reg_destroyed[r] = false;
  }
  for ( int f = 0 ; f < XMMRegisterImpl::number_of_registers ; f++ ) {
    freg_destroyed[f] = false;
  }

#endif /* ASSERT */

  // This may iterate in two different directions depending on the
  // kind of native it is.  The reason is that for regular JNI natives
  // the incoming and outgoing registers are offset upwards and for
  // critical natives they are offset down.
  GrowableArray<int> arg_order(2 * total_in_args);
  VMRegPair tmp_vmreg;
  tmp_vmreg.set2(rbx->as_VMReg());

  if (!is_critical_native) {
    for (int i = total_in_args - 1, c_arg = total_c_args - 1; i >= 0; i--, c_arg--) {
      arg_order.push(i);
      arg_order.push(c_arg);
    }
  } else {
    // Compute a valid move order, using tmp_vmreg to break any cycles
    ComputeMoveOrder cmo(total_in_args, in_regs, total_c_args, out_regs, in_sig_bt, arg_order, tmp_vmreg);
  }

  int temploc = -1;
  for (int ai = 0; ai < arg_order.length(); ai += 2) {
    int i = arg_order.at(ai);
    int c_arg = arg_order.at(ai + 1);
    __ block_comment(err_msg("move %d -> %d", i, c_arg));
    if (c_arg == -1) {
      assert(is_critical_native, "should only be required for critical natives");
      // This arg needs to be moved to a temporary
      __ mov(tmp_vmreg.first()->as_Register(), in_regs[i].first()->as_Register());
      in_regs[i] = tmp_vmreg;
      temploc = i;
      continue;
    } else if (i == -1) {
      assert(is_critical_native, "should only be required for critical natives");
      // Read from the temporary location
      assert(temploc != -1, "must be valid");
      i = temploc;
      temploc = -1;
    }
#ifdef ASSERT
    if (in_regs[i].first()->is_Register()) {
      assert(!reg_destroyed[in_regs[i].first()->as_Register()->encoding()], "destroyed reg!");
    } else if (in_regs[i].first()->is_XMMRegister()) {
      assert(!freg_destroyed[in_regs[i].first()->as_XMMRegister()->encoding()], "destroyed reg!");
    }
    if (out_regs[c_arg].first()->is_Register()) {
      reg_destroyed[out_regs[c_arg].first()->as_Register()->encoding()] = true;
    } else if (out_regs[c_arg].first()->is_XMMRegister()) {
      freg_destroyed[out_regs[c_arg].first()->as_XMMRegister()->encoding()] = true;
    }
#endif /* ASSERT */
    switch (in_sig_bt[i]) {
      case T_ARRAY:
        if (is_critical_native) {
          unpack_array_argument(masm, in_regs[i], in_elem_bt[i], out_regs[c_arg + 1], out_regs[c_arg]);
          c_arg++;
#ifdef ASSERT
          if (out_regs[c_arg].first()->is_Register()) {
            reg_destroyed[out_regs[c_arg].first()->as_Register()->encoding()] = true;
          } else if (out_regs[c_arg].first()->is_XMMRegister()) {
            freg_destroyed[out_regs[c_arg].first()->as_XMMRegister()->encoding()] = true;
          }
#endif
          break;
        }
      case T_OBJECT:
        assert(!is_critical_native, "no oop arguments");
        object_move(masm, map, oop_handle_offset, stack_slots, in_regs[i], out_regs[c_arg],
                    ((i == 0) && (!is_static)),
                    &receiver_offset);
        break;
      case T_VOID:
        break;

      case T_FLOAT:
        float_move(masm, in_regs[i], out_regs[c_arg]);
          break;

      case T_DOUBLE:
        assert( i + 1 < total_in_args &&
                in_sig_bt[i + 1] == T_VOID &&
                out_sig_bt[c_arg+1] == T_VOID, "bad arg list");
        double_move(masm, in_regs[i], out_regs[c_arg]);
        break;

      case T_LONG :
        long_move(masm, in_regs[i], out_regs[c_arg]);
        break;

      case T_ADDRESS: assert(false, "found T_ADDRESS in java args");

      default:
        move32_64(masm, in_regs[i], out_regs[c_arg]);
    }
  }

  int c_arg;

  // Pre-load a static method's oop into r14.  Used both by locking code and
  // the normal JNI call code.
  if (!is_critical_native) {
    // point c_arg at the first arg that is already loaded in case we
    // need to spill before we call out
    c_arg = total_c_args - total_in_args;

    if (method->is_static()) {

      //  load oop into a register
      __ movoop(oop_handle_reg, JNIHandles::make_local(method->method_holder()->java_mirror()));

      // Now handlize the static class mirror it's known not-null.
      __ movptr(Address(rsp, klass_offset), oop_handle_reg);
      map->set_oop(VMRegImpl::stack2reg(klass_slot_offset));

      // Now get the handle
      __ lea(oop_handle_reg, Address(rsp, klass_offset));
      // store the klass handle as second argument
      __ movptr(c_rarg1, oop_handle_reg);
      // and protect the arg if we must spill
      c_arg--;
    }
  } else {
    // For JNI critical methods we need to save all registers in save_args.
    c_arg = 0;
  }

  // Change state to native (we save the return address in the thread, since it might not
  // be pushed on the stack when we do a a stack traversal). It is enough that the pc()
  // points into the right code segment. It does not have to be the correct return pc.
  // We use the same pc/oopMap repeatedly when we call out

  intptr_t the_pc = (intptr_t) __ pc();
  oop_maps->add_gc_map(the_pc - start, map);

  __ set_last_Java_frame(rsp, noreg, (address)the_pc);


  // We have all of the arguments setup at this point. We must not touch any register
  // argument registers at this point (what if we save/restore them there are no oop?

  {
    SkipIfEqual skip(masm, &DTraceMethodProbes, false);
    // protect the args we've loaded
    save_args(masm, total_c_args, c_arg, out_regs);
    __ mov_metadata(c_rarg1, method());
    __ call_VM_leaf(
      CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry),
      r15_thread, c_rarg1);
    restore_args(masm, total_c_args, c_arg, out_regs);
  }

  // RedefineClasses() tracing support for obsolete method entry
  if (RC_TRACE_IN_RANGE(0x00001000, 0x00002000)) {
    // protect the args we've loaded
    save_args(masm, total_c_args, c_arg, out_regs);
    __ mov_metadata(c_rarg1, method());
    __ call_VM_leaf(
      CAST_FROM_FN_PTR(address, SharedRuntime::rc_trace_method_entry),
      r15_thread, c_rarg1);
    restore_args(masm, total_c_args, c_arg, out_regs);
  }

  // Lock a synchronized method

  // Register definitions used by locking and unlocking

  const Register swap_reg = rax;  // Must use rax for cmpxchg instruction
  const Register obj_reg  = rbx;  // Will contain the oop
  const Register lock_reg = r13;  // Address of compiler lock object (BasicLock)
  const Register old_hdr  = r13;  // value of old header at unlock time

  Label slow_path_lock;
  Label lock_done;

  if (method->is_synchronized()) {
    assert(!is_critical_native, "unhandled");


    const int mark_word_offset = BasicLock::displaced_header_offset_in_bytes();

    // Get the handle (the 2nd argument)
    __ mov(oop_handle_reg, c_rarg1);

    // Get address of the box

    __ lea(lock_reg, Address(rsp, lock_slot_offset * VMRegImpl::stack_slot_size));

    // Load the oop from the handle
    __ movptr(obj_reg, Address(oop_handle_reg, 0));

    if (UseBiasedLocking) {
      __ biased_locking_enter(lock_reg, obj_reg, swap_reg, rscratch1, false, lock_done, &slow_path_lock);
    }

    // Load immediate 1 into swap_reg %rax
    __ movl(swap_reg, 1);

    // Load (object->mark() | 1) into swap_reg %rax
    __ orptr(swap_reg, Address(obj_reg, 0));

    // Save (object->mark() | 1) into BasicLock's displaced header
    __ movptr(Address(lock_reg, mark_word_offset), swap_reg);

    if (os::is_MP()) {
      __ lock();
    }

    // src -> dest iff dest == rax else rax <- dest
    __ cmpxchgptr(lock_reg, Address(obj_reg, 0));
    __ jcc(Assembler::equal, lock_done);

    // Hmm should this move to the slow path code area???

    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 3) == 0, and
    //  2) rsp <= mark < mark + os::pagesize()
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - rsp) & (3 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 2 bits clear.
    // NOTE: the oopMark is in swap_reg %rax as the result of cmpxchg

    __ subptr(swap_reg, rsp);
    __ andptr(swap_reg, 3 - os::vm_page_size());

    // Save the test result, for recursive case, the result is zero
    __ movptr(Address(lock_reg, mark_word_offset), swap_reg);
    __ jcc(Assembler::notEqual, slow_path_lock);

    // Slow path will re-enter here

    __ bind(lock_done);
  }


  // Finally just about ready to make the JNI call


  // get JNIEnv* which is first argument to native
  if (!is_critical_native) {
    __ lea(c_rarg0, Address(r15_thread, in_bytes(JavaThread::jni_environment_offset())));
  }

  if (EnableCoroutine) {
    __ movptr(r11, Address(r15_thread, JavaThread::coroutine_list_offset()));
    __ incrementl(Address(r11, Coroutine::native_call_counter_offset()));
  }

  // Now set thread in native
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()), _thread_in_native);

  __ call(RuntimeAddress(native_func));

  // In wisp, this coroutine may be stolen by another thread inside the `native_func` call.
  // If this `native_func` is one of the several native functions we supported,
  // we will add thread fix code for them.
  generate_thread_fix(masm, method());

  // Verify or restore cpu control state after JNI call
  __ restore_cpu_control_state_after_jni();

  // Unpack native results.
  switch (ret_type) {
  case T_BOOLEAN: __ c2bool(rax);            break;
  case T_CHAR   : __ movzwl(rax, rax);      break;
  case T_BYTE   : __ sign_extend_byte (rax); break;
  case T_SHORT  : __ sign_extend_short(rax); break;
  case T_INT    : /* nothing to do */        break;
  case T_DOUBLE :
  case T_FLOAT  :
    // Result is in xmm0 we'll save as needed
    break;
  case T_ARRAY:                 // Really a handle
  case T_OBJECT:                // Really a handle
      break; // can't de-handlize until after safepoint check
  case T_VOID: break;
  case T_LONG: break;
  default       : ShouldNotReachHere();
  }

  // Switch thread to "native transition" state before reading the synchronization state.
  // This additional state is necessary because reading and testing the synchronization
  // state is not atomic w.r.t. GC, as this scenario demonstrates:
  //     Java thread A, in _thread_in_native state, loads _not_synchronized and is preempted.
  //     VM thread changes sync state to synchronizing and suspends threads for GC.
  //     Thread A is resumed to finish this native method, but doesn't block here since it
  //     didn't see any synchronization is progress, and escapes.
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()), _thread_in_native_trans);

  if(os::is_MP()) {
    if (UseMembar) {
      // Force this write out before the read below
      __ membar(Assembler::Membar_mask_bits(
           Assembler::LoadLoad | Assembler::LoadStore |
           Assembler::StoreLoad | Assembler::StoreStore));
    } else {
      // Write serialization page so VM thread can do a pseudo remote membar.
      // We use the current thread pointer to calculate a thread specific
      // offset to write to within the page. This minimizes bus traffic
      // due to cache line collision.
      __ serialize_memory(r15_thread, rcx);
    }
  }

  Label after_transition;

  // check for safepoint operation in progress and/or pending suspend requests
  {
    Label Continue;

    __ cmp32(ExternalAddress((address)SafepointSynchronize::address_of_state()),
             SafepointSynchronize::_not_synchronized);

    Label L;
    __ jcc(Assembler::notEqual, L);
    __ cmpl(Address(r15_thread, JavaThread::suspend_flags_offset()), 0);
    __ jcc(Assembler::equal, Continue);
    __ bind(L);

    // Don't use call_VM as it will see a possible pending exception and forward it
    // and never return here preventing us from clearing _last_native_pc down below.
    // Also can't use call_VM_leaf either as it will check to see if rsi & rdi are
    // preserved and correspond to the bcp/locals pointers. So we do a runtime call
    // by hand.
    //
    save_native_result(masm, ret_type, stack_slots);
    __ mov(c_rarg0, r15_thread);
    __ mov(r12, rsp); // remember sp
    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
    __ andptr(rsp, -16); // align stack as required by ABI
    if (!is_critical_native) {
      __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans)));
    } else {
      __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans_and_transition)));
    }
    __ mov(rsp, r12); // restore sp
    __ reinit_heapbase();
    // Restore any method result value
    restore_native_result(masm, ret_type, stack_slots);

    if (is_critical_native) {
      // The call above performed the transition to thread_in_Java so
      // skip the transition logic below.
      __ jmpb(after_transition);
    }

    __ bind(Continue);
  }

  // change thread state
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()), _thread_in_Java);
  __ bind(after_transition);

  Label reguard;
  Label reguard_done;
  __ cmpl(Address(r15_thread, JavaThread::stack_guard_state_offset()), JavaThread::stack_guard_yellow_disabled);
  __ jcc(Assembler::equal, reguard);
  __ bind(reguard_done);

  // native result if any is live

  // Unlock
  Label unlock_done;
  Label slow_path_unlock;
  if (method->is_synchronized()) {

    // Get locked oop from the handle we passed to jni
    __ movptr(obj_reg, Address(oop_handle_reg, 0));

    Label done;

    if (UseBiasedLocking) {
      __ biased_locking_exit(obj_reg, old_hdr, done);
    }

    // Simple recursive lock?

    __ cmpptr(Address(rsp, lock_slot_offset * VMRegImpl::stack_slot_size), (int32_t)NULL_WORD);
    __ jcc(Assembler::equal, done);

    // Must save rax if if it is live now because cmpxchg must use it
    if (ret_type != T_FLOAT && ret_type != T_DOUBLE && ret_type != T_VOID) {
      save_native_result(masm, ret_type, stack_slots);
    }


    // get address of the stack lock
    __ lea(rax, Address(rsp, lock_slot_offset * VMRegImpl::stack_slot_size));
    //  get old displaced header
    __ movptr(old_hdr, Address(rax, 0));

    // Atomic swap old header if oop still contains the stack lock
    if (os::is_MP()) {
      __ lock();
    }
    __ cmpxchgptr(old_hdr, Address(obj_reg, 0));
    __ jcc(Assembler::notEqual, slow_path_unlock);

    // slow path re-enters here
    __ bind(unlock_done);
    if (ret_type != T_FLOAT && ret_type != T_DOUBLE && ret_type != T_VOID) {
      restore_native_result(masm, ret_type, stack_slots);
    }

    __ bind(done);

  }
  {
    SkipIfEqual skip(masm, &DTraceMethodProbes, false);
    save_native_result(masm, ret_type, stack_slots);
    __ mov_metadata(c_rarg1, method());
    __ call_VM_leaf(
         CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit),
         r15_thread, c_rarg1);
    restore_native_result(masm, ret_type, stack_slots);
  }

  __ reset_last_Java_frame(false);

  // Unbox oop result, e.g. JNIHandles::resolve value.
  if (ret_type == T_OBJECT || ret_type == T_ARRAY) {
    __ resolve_jobject(rax /* value */,
                       r15_thread /* thread */,
                       rcx /* tmp */);
  }

  if (!is_critical_native) {
    // reset handle block
    __ movptr(rcx, Address(r15_thread, JavaThread::active_handles_offset()));
    __ movl(Address(rcx, JNIHandleBlock::top_offset_in_bytes()), (int32_t)NULL_WORD);
  }

  // pop our frame

  __ leave();

  if (!is_critical_native) {
    // Any exception pending?
    __ cmpptr(Address(r15_thread, in_bytes(Thread::pending_exception_offset())), (int32_t)NULL_WORD);
    __ jcc(Assembler::notEqual, exception_pending);
  }

  // Return
  if (EnableCoroutine &&
      (method->intrinsic_id() == vmIntrinsics::_switchToAndTerminate ||
       method->intrinsic_id() == vmIntrinsics::_switchToAndExit)) {

    Label normal;
    __ lea(rcx, RuntimeAddress((unsigned char*)coroutine_start));
    __ cmpq(Address(rsp, 0), rcx);
    __ jcc(Assembler::notEqual, normal);

    __ movq(c_rarg0, Address(rsp, HeapWordSize * 2));
    __ movq(c_rarg1, Address(rsp, HeapWordSize * 3));

    __ bind(normal);

    __ ret(0);        // <-- this will jump to the stored IP of the target coroutine
  }

  __ ret(0);

  // Unexpected paths are out of line and go here

  if (!is_critical_native) {
    // forward the exception
    __ bind(exception_pending);

    // and forward the exception
    __ jump(RuntimeAddress(StubRoutines::forward_exception_entry()));
  }

  // Slow path locking & unlocking
  if (method->is_synchronized()) {

    // BEGIN Slow path lock
    __ bind(slow_path_lock);

    // has last_Java_frame setup. No exceptions so do vanilla call not call_VM
    // args are (oop obj, BasicLock* lock, JavaThread* thread)

    // protect the args we've loaded
    save_args(masm, total_c_args, c_arg, out_regs);

    __ mov(c_rarg0, obj_reg);
    __ mov(c_rarg1, lock_reg);
    __ mov(c_rarg2, r15_thread);

    // Not a leaf but we have last_Java_frame setup as we want
    __ call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::complete_monitor_locking_C), 3);
    restore_args(masm, total_c_args, c_arg, out_regs);

    if (EnableCoroutine) {
      // the r15 has been restored in restore_args so we need fix it.
      WISP_COMPILER_RESTORE_FORCE_UPDATE;
    }

#ifdef ASSERT
    { Label L;
    __ cmpptr(Address(r15_thread, in_bytes(Thread::pending_exception_offset())), (int32_t)NULL_WORD);
    __ jcc(Assembler::equal, L);
    __ stop("no pending exception allowed on exit from monitorenter");
    __ bind(L);
    }
#endif
    __ jmp(lock_done);

    // END Slow path lock

    // BEGIN Slow path unlock
    __ bind(slow_path_unlock);

    // If we haven't already saved the native result we must save it now as xmm registers
    // are still exposed.

    if (ret_type == T_FLOAT || ret_type == T_DOUBLE ) {
      save_native_result(masm, ret_type, stack_slots);
    }

    __ lea(c_rarg1, Address(rsp, lock_slot_offset * VMRegImpl::stack_slot_size));

    __ mov(c_rarg0, obj_reg);
    __ mov(r12, rsp); // remember sp
    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
    __ andptr(rsp, -16); // align stack as required by ABI

    // Save pending exception around call to VM (which contains an EXCEPTION_MARK)
    // NOTE that obj_reg == rbx currently
    __ movptr(rbx, Address(r15_thread, in_bytes(Thread::pending_exception_offset())));
    __ movptr(Address(r15_thread, in_bytes(Thread::pending_exception_offset())), (int32_t)NULL_WORD);

    if (UseWispMonitor) {
      __ mov(c_rarg2, r15_thread);
      __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::complete_wisp_monitor_unlocking_C)));
    } else {
      __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::complete_monitor_unlocking_C)));
    }
    __ mov(rsp, r12); // restore sp
    __ reinit_heapbase();
#ifdef ASSERT
    {
      Label L;
      __ cmpptr(Address(r15_thread, in_bytes(Thread::pending_exception_offset())), (int)NULL_WORD);
      __ jcc(Assembler::equal, L);
      __ stop("no pending exception allowed on exit complete_monitor_unlocking_C");
      __ bind(L);
    }
#endif /* ASSERT */

    __ movptr(Address(r15_thread, in_bytes(Thread::pending_exception_offset())), rbx);

    if (ret_type == T_FLOAT || ret_type == T_DOUBLE ) {
      restore_native_result(masm, ret_type, stack_slots);
    }
    __ jmp(unlock_done);

    // END Slow path unlock

  } // synchronized

  // SLOW PATH Reguard the stack if needed

  __ bind(reguard);
  save_native_result(masm, ret_type, stack_slots);
  __ mov(r12, rsp); // remember sp
  __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
  __ andptr(rsp, -16); // align stack as required by ABI
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages)));
  __ mov(rsp, r12); // restore sp
  __ reinit_heapbase();
  restore_native_result(masm, ret_type, stack_slots);
  // and continue
  __ jmp(reguard_done);



  __ flush();

  nmethod *nm = nmethod::new_native_nmethod(method,
                                            compile_id,
                                            masm->code(),
                                            vep_offset,
                                            frame_complete,
                                            stack_slots / VMRegImpl::slots_per_word,
                                            (is_static ? in_ByteSize(klass_offset) : in_ByteSize(receiver_offset)),
                                            in_ByteSize(lock_slot_offset*VMRegImpl::stack_slot_size),
                                            oop_maps);

  if (is_critical_native) {
    nm->set_lazy_critical_native(true);
  }

  return nm;

}

#ifdef HAVE_DTRACE_H
// ---------------------------------------------------------------------------
// Generate a dtrace nmethod for a given signature.  The method takes arguments
// in the Java compiled code convention, marshals them to the native
// abi and then leaves nops at the position you would expect to call a native
// function. When the probe is enabled the nops are replaced with a trap
// instruction that dtrace inserts and the trace will cause a notification
// to dtrace.
//
// The probes are only able to take primitive types and java/lang/String as
// arguments.  No other java types are allowed. Strings are converted to utf8
// strings so that from dtrace point of view java strings are converted to C
// strings. There is an arbitrary fixed limit on the total space that a method
// can use for converting the strings. (256 chars per string in the signature).
// So any java string larger then this is truncated.

static int  fp_offset[ConcreteRegisterImpl::number_of_registers] = { 0 };
static bool offsets_initialized = false;


nmethod *SharedRuntime::generate_dtrace_nmethod(MacroAssembler *masm,
                                                methodHandle method) {


  // generate_dtrace_nmethod is guarded by a mutex so we are sure to
  // be single threaded in this method.
  assert(AdapterHandlerLibrary_lock->owned_by_self(), "must be");

  if (!offsets_initialized) {
    fp_offset[c_rarg0->as_VMReg()->value()] = -1 * wordSize;
    fp_offset[c_rarg1->as_VMReg()->value()] = -2 * wordSize;
    fp_offset[c_rarg2->as_VMReg()->value()] = -3 * wordSize;
    fp_offset[c_rarg3->as_VMReg()->value()] = -4 * wordSize;
    fp_offset[c_rarg4->as_VMReg()->value()] = -5 * wordSize;
    fp_offset[c_rarg5->as_VMReg()->value()] = -6 * wordSize;

    fp_offset[c_farg0->as_VMReg()->value()] = -7 * wordSize;
    fp_offset[c_farg1->as_VMReg()->value()] = -8 * wordSize;
    fp_offset[c_farg2->as_VMReg()->value()] = -9 * wordSize;
    fp_offset[c_farg3->as_VMReg()->value()] = -10 * wordSize;
    fp_offset[c_farg4->as_VMReg()->value()] = -11 * wordSize;
    fp_offset[c_farg5->as_VMReg()->value()] = -12 * wordSize;
    fp_offset[c_farg6->as_VMReg()->value()] = -13 * wordSize;
    fp_offset[c_farg7->as_VMReg()->value()] = -14 * wordSize;

    offsets_initialized = true;
  }
  // Fill in the signature array, for the calling-convention call.
  int total_args_passed = method->size_of_parameters();

  BasicType* in_sig_bt  = NEW_RESOURCE_ARRAY(BasicType, total_args_passed);
  VMRegPair  *in_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_args_passed);

  // The signature we are going to use for the trap that dtrace will see
  // java/lang/String is converted. We drop "this" and any other object
  // is converted to NULL.  (A one-slot java/lang/Long object reference
  // is converted to a two-slot long, which is why we double the allocation).
  BasicType* out_sig_bt = NEW_RESOURCE_ARRAY(BasicType, total_args_passed * 2);
  VMRegPair* out_regs   = NEW_RESOURCE_ARRAY(VMRegPair, total_args_passed * 2);

  int i=0;
  int total_strings = 0;
  int first_arg_to_pass = 0;
  int total_c_args = 0;

  // Skip the receiver as dtrace doesn't want to see it
  if( !method->is_static() ) {
    in_sig_bt[i++] = T_OBJECT;
    first_arg_to_pass = 1;
  }

  // We need to convert the java args to where a native (non-jni) function
  // would expect them. To figure out where they go we convert the java
  // signature to a C signature.

  SignatureStream ss(method->signature());
  for ( ; !ss.at_return_type(); ss.next()) {
    BasicType bt = ss.type();
    in_sig_bt[i++] = bt;  // Collect remaining bits of signature
    out_sig_bt[total_c_args++] = bt;
    if( bt == T_OBJECT) {
      Symbol* s = ss.as_symbol_or_null();   // symbol is created
      if (s == vmSymbols::java_lang_String()) {
        total_strings++;
        out_sig_bt[total_c_args-1] = T_ADDRESS;
      } else if (s == vmSymbols::java_lang_Boolean() ||
                 s == vmSymbols::java_lang_Character() ||
                 s == vmSymbols::java_lang_Byte() ||
                 s == vmSymbols::java_lang_Short() ||
                 s == vmSymbols::java_lang_Integer() ||
                 s == vmSymbols::java_lang_Float()) {
        out_sig_bt[total_c_args-1] = T_INT;
      } else if (s == vmSymbols::java_lang_Long() ||
                 s == vmSymbols::java_lang_Double()) {
        out_sig_bt[total_c_args-1] = T_LONG;
        out_sig_bt[total_c_args++] = T_VOID;
      }
    } else if ( bt == T_LONG || bt == T_DOUBLE ) {
      in_sig_bt[i++] = T_VOID;   // Longs & doubles take 2 Java slots
      // We convert double to long
      out_sig_bt[total_c_args-1] = T_LONG;
      out_sig_bt[total_c_args++] = T_VOID;
    } else if ( bt == T_FLOAT) {
      // We convert float to int
      out_sig_bt[total_c_args-1] = T_INT;
    }
  }

  assert(i==total_args_passed, "validly parsed signature");

  // Now get the compiled-Java layout as input arguments
  int comp_args_on_stack;
  comp_args_on_stack = SharedRuntime::java_calling_convention(
      in_sig_bt, in_regs, total_args_passed, false);

  // Now figure out where the args must be stored and how much stack space
  // they require (neglecting out_preserve_stack_slots but space for storing
  // the 1st six register arguments). It's weird see int_stk_helper.

  int out_arg_slots;
  out_arg_slots = c_calling_convention(out_sig_bt, out_regs, NULL, total_c_args);

  // Calculate the total number of stack slots we will need.

  // First count the abi requirement plus all of the outgoing args
  int stack_slots = SharedRuntime::out_preserve_stack_slots() + out_arg_slots;

  // Now space for the string(s) we must convert
  int* string_locs   = NEW_RESOURCE_ARRAY(int, total_strings + 1);
  for (i = 0; i < total_strings ; i++) {
    string_locs[i] = stack_slots;
    stack_slots += max_dtrace_string_size / VMRegImpl::stack_slot_size;
  }

  // Plus the temps we might need to juggle register args
  // regs take two slots each
  stack_slots += (Argument::n_int_register_parameters_c +
                  Argument::n_float_register_parameters_c) * 2;


  // + 4 for return address (which we own) and saved rbp,

  stack_slots += 4;

  // Ok The space we have allocated will look like:
  //
  //
  // FP-> |                     |
  //      |---------------------|
  //      | string[n]           |
  //      |---------------------| <- string_locs[n]
  //      | string[n-1]         |
  //      |---------------------| <- string_locs[n-1]
  //      | ...                 |
  //      | ...                 |
  //      |---------------------| <- string_locs[1]
  //      | string[0]           |
  //      |---------------------| <- string_locs[0]
  //      | outbound memory     |
  //      | based arguments     |
  //      |                     |
  //      |---------------------|
  //      |                     |
  // SP-> | out_preserved_slots |
  //
  //

  // Now compute actual number of stack words we need rounding to make
  // stack properly aligned.
  stack_slots = round_to(stack_slots, 4 * VMRegImpl::slots_per_word);

  int stack_size = stack_slots * VMRegImpl::stack_slot_size;

  intptr_t start = (intptr_t)__ pc();

  // First thing make an ic check to see if we should even be here

  // We are free to use all registers as temps without saving them and
  // restoring them except rbp. rbp, is the only callee save register
  // as far as the interpreter and the compiler(s) are concerned.

  const Register ic_reg = rax;
  const Register receiver = rcx;
  Label hit;
  Label exception_pending;


  __ verify_oop(receiver);
  __ cmpl(ic_reg, Address(receiver, oopDesc::klass_offset_in_bytes()));
  __ jcc(Assembler::equal, hit);

  __ jump(RuntimeAddress(SharedRuntime::get_ic_miss_stub()));

  // verified entry must be aligned for code patching.
  // and the first 5 bytes must be in the same cache line
  // if we align at 8 then we will be sure 5 bytes are in the same line
  __ align(8);

  __ bind(hit);

  int vep_offset = ((intptr_t)__ pc()) - start;


  // The instruction at the verified entry point must be 5 bytes or longer
  // because it can be patched on the fly by make_non_entrant. The stack bang
  // instruction fits that requirement.

  // Generate stack overflow check

  if (UseStackBanging) {
    if (stack_size <= StackShadowPages*os::vm_page_size()) {
      __ bang_stack_with_offset(StackShadowPages*os::vm_page_size());
    } else {
      __ movl(rax, stack_size);
      __ bang_stack_size(rax, rbx);
    }
  } else {
    // need a 5 byte instruction to allow MT safe patching to non-entrant
    __ fat_nop();
  }

  assert(((uintptr_t)__ pc() - start - vep_offset) >= 5,
         "valid size for make_non_entrant");

  // Generate a new frame for the wrapper.
  __ enter();

  // -4 because return address is already present and so is saved rbp,
  if (stack_size - 2*wordSize != 0) {
    __ subq(rsp, stack_size - 2*wordSize);
  }

  // Frame is now completed as far a size and linkage.

  int frame_complete = ((intptr_t)__ pc()) - start;

  int c_arg, j_arg;

  // State of input register args

  bool  live[ConcreteRegisterImpl::number_of_registers];

  live[j_rarg0->as_VMReg()->value()] = false;
  live[j_rarg1->as_VMReg()->value()] = false;
  live[j_rarg2->as_VMReg()->value()] = false;
  live[j_rarg3->as_VMReg()->value()] = false;
  live[j_rarg4->as_VMReg()->value()] = false;
  live[j_rarg5->as_VMReg()->value()] = false;

  live[j_farg0->as_VMReg()->value()] = false;
  live[j_farg1->as_VMReg()->value()] = false;
  live[j_farg2->as_VMReg()->value()] = false;
  live[j_farg3->as_VMReg()->value()] = false;
  live[j_farg4->as_VMReg()->value()] = false;
  live[j_farg5->as_VMReg()->value()] = false;
  live[j_farg6->as_VMReg()->value()] = false;
  live[j_farg7->as_VMReg()->value()] = false;


  bool rax_is_zero = false;

  // All args (except strings) destined for the stack are moved first
  for (j_arg = first_arg_to_pass, c_arg = 0 ;
       j_arg < total_args_passed ; j_arg++, c_arg++ ) {
    VMRegPair src = in_regs[j_arg];
    VMRegPair dst = out_regs[c_arg];

    // Get the real reg value or a dummy (rsp)

    int src_reg = src.first()->is_reg() ?
                  src.first()->value() :
                  rsp->as_VMReg()->value();

    bool useless =  in_sig_bt[j_arg] == T_ARRAY ||
                    (in_sig_bt[j_arg] == T_OBJECT &&
                     out_sig_bt[c_arg] != T_INT &&
                     out_sig_bt[c_arg] != T_ADDRESS &&
                     out_sig_bt[c_arg] != T_LONG);

    live[src_reg] = !useless;

    if (dst.first()->is_stack()) {

      // Even though a string arg in a register is still live after this loop
      // after the string conversion loop (next) it will be dead so we take
      // advantage of that now for simpler code to manage live.

      live[src_reg] = false;
      switch (in_sig_bt[j_arg]) {

        case T_ARRAY:
        case T_OBJECT:
          {
            Address stack_dst(rsp, reg2offset_out(dst.first()));

            if (out_sig_bt[c_arg] == T_INT || out_sig_bt[c_arg] == T_LONG) {
              // need to unbox a one-word value
              Register in_reg = rax;
              if ( src.first()->is_reg() ) {
                in_reg = src.first()->as_Register();
              } else {
                __ movq(rax, Address(rbp, reg2offset_in(src.first())));
                rax_is_zero = false;
              }
              Label skipUnbox;
              __ movptr(Address(rsp, reg2offset_out(dst.first())),
                        (int32_t)NULL_WORD);
              __ testq(in_reg, in_reg);
              __ jcc(Assembler::zero, skipUnbox);

              BasicType bt = out_sig_bt[c_arg];
              int box_offset = java_lang_boxing_object::value_offset_in_bytes(bt);
              Address src1(in_reg, box_offset);
              if ( bt == T_LONG ) {
                __ movq(in_reg,  src1);
                __ movq(stack_dst, in_reg);
                assert(out_sig_bt[c_arg+1] == T_VOID, "must be");
                ++c_arg; // skip over T_VOID to keep the loop indices in sync
              } else {
                __ movl(in_reg,  src1);
                __ movl(stack_dst, in_reg);
              }

              __ bind(skipUnbox);
            } else if (out_sig_bt[c_arg] != T_ADDRESS) {
              // Convert the arg to NULL
              if (!rax_is_zero) {
                __ xorq(rax, rax);
                rax_is_zero = true;
              }
              __ movq(stack_dst, rax);
            }
          }
          break;

        case T_VOID:
          break;

        case T_FLOAT:
          // This does the right thing since we know it is destined for the
          // stack
          float_move(masm, src, dst);
          break;

        case T_DOUBLE:
          // This does the right thing since we know it is destined for the
          // stack
          double_move(masm, src, dst);
          break;

        case T_LONG :
          long_move(masm, src, dst);
          break;

        case T_ADDRESS: assert(false, "found T_ADDRESS in java args");

        default:
          move32_64(masm, src, dst);
      }
    }

  }

  // If we have any strings we must store any register based arg to the stack
  // This includes any still live xmm registers too.

  int sid = 0;

  if (total_strings > 0 ) {
    for (j_arg = first_arg_to_pass, c_arg = 0 ;
         j_arg < total_args_passed ; j_arg++, c_arg++ ) {
      VMRegPair src = in_regs[j_arg];
      VMRegPair dst = out_regs[c_arg];

      if (src.first()->is_reg()) {
        Address src_tmp(rbp, fp_offset[src.first()->value()]);

        // string oops were left untouched by the previous loop even if the
        // eventual (converted) arg is destined for the stack so park them
        // away now (except for first)

        if (out_sig_bt[c_arg] == T_ADDRESS) {
          Address utf8_addr = Address(
              rsp, string_locs[sid++] * VMRegImpl::stack_slot_size);
          if (sid != 1) {
            // The first string arg won't be killed until after the utf8
            // conversion
            __ movq(utf8_addr, src.first()->as_Register());
          }
        } else if (dst.first()->is_reg()) {
          if (in_sig_bt[j_arg] == T_FLOAT || in_sig_bt[j_arg] == T_DOUBLE) {

            // Convert the xmm register to an int and store it in the reserved
            // location for the eventual c register arg
            XMMRegister f = src.first()->as_XMMRegister();
            if (in_sig_bt[j_arg] == T_FLOAT) {
              __ movflt(src_tmp, f);
            } else {
              __ movdbl(src_tmp, f);
            }
          } else {
            // If the arg is an oop type we don't support don't bother to store
            // it remember string was handled above.
            bool useless =  in_sig_bt[j_arg] == T_ARRAY ||
                            (in_sig_bt[j_arg] == T_OBJECT &&
                             out_sig_bt[c_arg] != T_INT &&
                             out_sig_bt[c_arg] != T_LONG);

            if (!useless) {
              __ movq(src_tmp, src.first()->as_Register());
            }
          }
        }
      }
      if (in_sig_bt[j_arg] == T_OBJECT && out_sig_bt[c_arg] == T_LONG) {
        assert(out_sig_bt[c_arg+1] == T_VOID, "must be");
        ++c_arg; // skip over T_VOID to keep the loop indices in sync
      }
    }

    // Now that the volatile registers are safe, convert all the strings
    sid = 0;

    for (j_arg = first_arg_to_pass, c_arg = 0 ;
         j_arg < total_args_passed ; j_arg++, c_arg++ ) {
      if (out_sig_bt[c_arg] == T_ADDRESS) {
        // It's a string
        Address utf8_addr = Address(
            rsp, string_locs[sid++] * VMRegImpl::stack_slot_size);
        // The first string we find might still be in the original java arg
        // register

        VMReg src = in_regs[j_arg].first();

        // We will need to eventually save the final argument to the trap
        // in the von-volatile location dedicated to src. This is the offset
        // from fp we will use.
        int src_off = src->is_reg() ?
            fp_offset[src->value()] : reg2offset_in(src);

        // This is where the argument will eventually reside
        VMRegPair dst = out_regs[c_arg];

        if (src->is_reg()) {
          if (sid == 1) {
            __ movq(c_rarg0, src->as_Register());
          } else {
            __ movq(c_rarg0, utf8_addr);
          }
        } else {
          // arg is still in the original location
          __ movq(c_rarg0, Address(rbp, reg2offset_in(src)));
        }
        Label done, convert;

        // see if the oop is NULL
        __ testq(c_rarg0, c_rarg0);
        __ jcc(Assembler::notEqual, convert);

        if (dst.first()->is_reg()) {
          // Save the ptr to utf string in the origina src loc or the tmp
          // dedicated to it
          __ movq(Address(rbp, src_off), c_rarg0);
        } else {
          __ movq(Address(rsp, reg2offset_out(dst.first())), c_rarg0);
        }
        __ jmp(done);

        __ bind(convert);

        __ lea(c_rarg1, utf8_addr);
        if (dst.first()->is_reg()) {
          __ movq(Address(rbp, src_off), c_rarg1);
        } else {
          __ movq(Address(rsp, reg2offset_out(dst.first())), c_rarg1);
        }
        // And do the conversion
        __ call(RuntimeAddress(
                CAST_FROM_FN_PTR(address, SharedRuntime::get_utf)));

        __ bind(done);
      }
      if (in_sig_bt[j_arg] == T_OBJECT && out_sig_bt[c_arg] == T_LONG) {
        assert(out_sig_bt[c_arg+1] == T_VOID, "must be");
        ++c_arg; // skip over T_VOID to keep the loop indices in sync
      }
    }
    // The get_utf call killed all the c_arg registers
    live[c_rarg0->as_VMReg()->value()] = false;
    live[c_rarg1->as_VMReg()->value()] = false;
    live[c_rarg2->as_VMReg()->value()] = false;
    live[c_rarg3->as_VMReg()->value()] = false;
    live[c_rarg4->as_VMReg()->value()] = false;
    live[c_rarg5->as_VMReg()->value()] = false;

    live[c_farg0->as_VMReg()->value()] = false;
    live[c_farg1->as_VMReg()->value()] = false;
    live[c_farg2->as_VMReg()->value()] = false;
    live[c_farg3->as_VMReg()->value()] = false;
    live[c_farg4->as_VMReg()->value()] = false;
    live[c_farg5->as_VMReg()->value()] = false;
    live[c_farg6->as_VMReg()->value()] = false;
    live[c_farg7->as_VMReg()->value()] = false;
  }

  // Now we can finally move the register args to their desired locations

  rax_is_zero = false;

  for (j_arg = first_arg_to_pass, c_arg = 0 ;
       j_arg < total_args_passed ; j_arg++, c_arg++ ) {

    VMRegPair src = in_regs[j_arg];
    VMRegPair dst = out_regs[c_arg];

    // Only need to look for args destined for the interger registers (since we
    // convert float/double args to look like int/long outbound)
    if (dst.first()->is_reg()) {
      Register r =  dst.first()->as_Register();

      // Check if the java arg is unsupported and thereofre useless
      bool useless =  in_sig_bt[j_arg] == T_ARRAY ||
                      (in_sig_bt[j_arg] == T_OBJECT &&
                       out_sig_bt[c_arg] != T_INT &&
                       out_sig_bt[c_arg] != T_ADDRESS &&
                       out_sig_bt[c_arg] != T_LONG);


      // If we're going to kill an existing arg save it first
      if (live[dst.first()->value()]) {
        // you can't kill yourself
        if (src.first() != dst.first()) {
          __ movq(Address(rbp, fp_offset[dst.first()->value()]), r);
        }
      }
      if (src.first()->is_reg()) {
        if (live[src.first()->value()] ) {
          if (in_sig_bt[j_arg] == T_FLOAT) {
            __ movdl(r, src.first()->as_XMMRegister());
          } else if (in_sig_bt[j_arg] == T_DOUBLE) {
            __ movdq(r, src.first()->as_XMMRegister());
          } else if (r != src.first()->as_Register()) {
            if (!useless) {
              __ movq(r, src.first()->as_Register());
            }
          }
        } else {
          // If the arg is an oop type we don't support don't bother to store
          // it
          if (!useless) {
            if (in_sig_bt[j_arg] == T_DOUBLE ||
                in_sig_bt[j_arg] == T_LONG  ||
                in_sig_bt[j_arg] == T_OBJECT ) {
              __ movq(r, Address(rbp, fp_offset[src.first()->value()]));
            } else {
              __ movl(r, Address(rbp, fp_offset[src.first()->value()]));
            }
          }
        }
        live[src.first()->value()] = false;
      } else if (!useless) {
        // full sized move even for int should be ok
        __ movq(r, Address(rbp, reg2offset_in(src.first())));
      }

      // At this point r has the original java arg in the final location
      // (assuming it wasn't useless). If the java arg was an oop
      // we have a bit more to do

      if (in_sig_bt[j_arg] == T_ARRAY || in_sig_bt[j_arg] == T_OBJECT ) {
        if (out_sig_bt[c_arg] == T_INT || out_sig_bt[c_arg] == T_LONG) {
          // need to unbox a one-word value
          Label skip;
          __ testq(r, r);
          __ jcc(Assembler::equal, skip);
          BasicType bt = out_sig_bt[c_arg];
          int box_offset = java_lang_boxing_object::value_offset_in_bytes(bt);
          Address src1(r, box_offset);
          if ( bt == T_LONG ) {
            __ movq(r, src1);
          } else {
            __ movl(r, src1);
          }
          __ bind(skip);

        } else if (out_sig_bt[c_arg] != T_ADDRESS) {
          // Convert the arg to NULL
          __ xorq(r, r);
        }
      }

      // dst can longer be holding an input value
      live[dst.first()->value()] = false;
    }
    if (in_sig_bt[j_arg] == T_OBJECT && out_sig_bt[c_arg] == T_LONG) {
      assert(out_sig_bt[c_arg+1] == T_VOID, "must be");
      ++c_arg; // skip over T_VOID to keep the loop indices in sync
    }
  }


  // Ok now we are done. Need to place the nop that dtrace wants in order to
  // patch in the trap
  int patch_offset = ((intptr_t)__ pc()) - start;

  __ nop();


  // Return

  __ leave();
  __ ret(0);

  __ flush();

  nmethod *nm = nmethod::new_dtrace_nmethod(
      method, masm->code(), vep_offset, patch_offset, frame_complete,
      stack_slots / VMRegImpl::slots_per_word);
  return nm;

}

#endif // HAVE_DTRACE_H

// this function returns the adjust size (in number of words) to a c2i adapter
// activation for use during deoptimization
int Deoptimization::last_frame_adjust(int callee_parameters, int callee_locals ) {
  return (callee_locals - callee_parameters) * Interpreter::stackElementWords;
}


uint SharedRuntime::out_preserve_stack_slots() {
  return 0;
}

//------------------------------generate_deopt_blob----------------------------
void SharedRuntime::generate_deopt_blob() {
  // Allocate space for the code
  ResourceMark rm;
  // Setup code generation tools
  CodeBuffer buffer("deopt_blob", 2048, 1024);
  MacroAssembler* masm = new MacroAssembler(&buffer);
  int frame_size_in_words;
  OopMap* map = NULL;
  OopMapSet *oop_maps = new OopMapSet();

  // -------------
  // This code enters when returning to a de-optimized nmethod.  A return
  // address has been pushed on the the stack, and return values are in
  // registers.
  // If we are doing a normal deopt then we were called from the patched
  // nmethod from the point we returned to the nmethod. So the return
  // address on the stack is wrong by NativeCall::instruction_size
  // We will adjust the value so it looks like we have the original return
  // address on the stack (like when we eagerly deoptimized).
  // In the case of an exception pending when deoptimizing, we enter
  // with a return address on the stack that points after the call we patched
  // into the exception handler. We have the following register state from,
  // e.g., the forward exception stub (see stubGenerator_x86_64.cpp).
  //    rax: exception oop
  //    rbx: exception handler
  //    rdx: throwing pc
  // So in this case we simply jam rdx into the useless return address and
  // the stack looks just like we want.
  //
  // At this point we need to de-opt.  We save the argument return
  // registers.  We call the first C routine, fetch_unroll_info().  This
  // routine captures the return values and returns a structure which
  // describes the current frame size and the sizes of all replacement frames.
  // The current frame is compiled code and may contain many inlined
  // functions, each with their own JVM state.  We pop the current frame, then
  // push all the new frames.  Then we call the C routine unpack_frames() to
  // populate these frames.  Finally unpack_frames() returns us the new target
  // address.  Notice that callee-save registers are BLOWN here; they have
  // already been captured in the vframeArray at the time the return PC was
  // patched.
  address start = __ pc();
  Label cont;

  // Prolog for non exception case!

  // Save everything in sight.
  map = RegisterSaver::save_live_registers(masm, 0, &frame_size_in_words);

  // Normal deoptimization.  Save exec mode for unpack_frames.
  __ movl(r14, Deoptimization::Unpack_deopt); // callee-saved
  __ jmp(cont);

  int reexecute_offset = __ pc() - start;

  // Reexecute case
  // return address is the pc describes what bci to do re-execute at

  // No need to update map as each call to save_live_registers will produce identical oopmap
  (void) RegisterSaver::save_live_registers(masm, 0, &frame_size_in_words);

  __ movl(r14, Deoptimization::Unpack_reexecute); // callee-saved
  __ jmp(cont);

  int exception_offset = __ pc() - start;

  // Prolog for exception case

  // all registers are dead at this entry point, except for rax, and
  // rdx which contain the exception oop and exception pc
  // respectively.  Set them in TLS and fall thru to the
  // unpack_with_exception_in_tls entry point.

  __ movptr(Address(r15_thread, JavaThread::exception_pc_offset()), rdx);
  __ movptr(Address(r15_thread, JavaThread::exception_oop_offset()), rax);

  int exception_in_tls_offset = __ pc() - start;

  // new implementation because exception oop is now passed in JavaThread

  // Prolog for exception case
  // All registers must be preserved because they might be used by LinearScan
  // Exceptiop oop and throwing PC are passed in JavaThread
  // tos: stack at point of call to method that threw the exception (i.e. only
  // args are on the stack, no return address)

  // make room on stack for the return address
  // It will be patched later with the throwing pc. The correct value is not
  // available now because loading it from memory would destroy registers.
  __ push(0);

  // Save everything in sight.
  map = RegisterSaver::save_live_registers(masm, 0, &frame_size_in_words);

  // Now it is safe to overwrite any register

  // Deopt during an exception.  Save exec mode for unpack_frames.
  __ movl(r14, Deoptimization::Unpack_exception); // callee-saved

  // load throwing pc from JavaThread and patch it as the return address
  // of the current frame. Then clear the field in JavaThread

  __ movptr(rdx, Address(r15_thread, JavaThread::exception_pc_offset()));
  __ movptr(Address(rbp, wordSize), rdx);
  __ movptr(Address(r15_thread, JavaThread::exception_pc_offset()), (int32_t)NULL_WORD);

#ifdef ASSERT
  // verify that there is really an exception oop in JavaThread
  __ movptr(rax, Address(r15_thread, JavaThread::exception_oop_offset()));
  __ verify_oop(rax);

  // verify that there is no pending exception
  Label no_pending_exception;
  __ movptr(rax, Address(r15_thread, Thread::pending_exception_offset()));
  __ testptr(rax, rax);
  __ jcc(Assembler::zero, no_pending_exception);
  __ stop("must not have pending exception here");
  __ bind(no_pending_exception);
#endif

  __ bind(cont);

  // Call C code.  Need thread and this frame, but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.
  //
  // UnrollBlock* fetch_unroll_info(JavaThread* thread)

  // fetch_unroll_info needs to call last_java_frame().

  __ set_last_Java_frame(noreg, noreg, NULL);
#ifdef ASSERT
  { Label L;
    __ cmpptr(Address(r15_thread,
                    JavaThread::last_Java_fp_offset()),
            (int32_t)0);
    __ jcc(Assembler::equal, L);
    __ stop("SharedRuntime::generate_deopt_blob: last_Java_fp not cleared");
    __ bind(L);
  }
#endif // ASSERT
  __ mov(c_rarg0, r15_thread);
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, Deoptimization::fetch_unroll_info)));

  // Need to have an oopmap that tells fetch_unroll_info where to
  // find any register it might need.
  oop_maps->add_gc_map(__ pc() - start, map);

  __ reset_last_Java_frame(false);

  // Load UnrollBlock* into rdi
  __ mov(rdi, rax);

   Label noException;
  __ cmpl(r14, Deoptimization::Unpack_exception);   // Was exception pending?
  __ jcc(Assembler::notEqual, noException);
  __ movptr(rax, Address(r15_thread, JavaThread::exception_oop_offset()));
  // QQQ this is useless it was NULL above
  __ movptr(rdx, Address(r15_thread, JavaThread::exception_pc_offset()));
  __ movptr(Address(r15_thread, JavaThread::exception_oop_offset()), (int32_t)NULL_WORD);
  __ movptr(Address(r15_thread, JavaThread::exception_pc_offset()), (int32_t)NULL_WORD);

  __ verify_oop(rax);

  // Overwrite the result registers with the exception results.
  __ movptr(Address(rsp, RegisterSaver::rax_offset_in_bytes()), rax);
  // I think this is useless
  __ movptr(Address(rsp, RegisterSaver::rdx_offset_in_bytes()), rdx);

  __ bind(noException);

  // Only register save data is on the stack.
  // Now restore the result registers.  Everything else is either dead
  // or captured in the vframeArray.
  RegisterSaver::restore_result_registers(masm);

  // All of the register save area has been popped of the stack. Only the
  // return address remains.

  // Pop all the frames we must move/replace.
  //
  // Frame picture (youngest to oldest)
  // 1: self-frame (no frame link)
  // 2: deopting frame  (no frame link)
  // 3: caller of deopting frame (could be compiled/interpreted).
  //
  // Note: by leaving the return address of self-frame on the stack
  // and using the size of frame 2 to adjust the stack
  // when we are done the return to frame 3 will still be on the stack.

  // Pop deoptimized frame
  __ movl(rcx, Address(rdi, Deoptimization::UnrollBlock::size_of_deoptimized_frame_offset_in_bytes()));
  __ addptr(rsp, rcx);

  // rsp should be pointing at the return address to the caller (3)

  // Pick up the initial fp we should save
  // restore rbp before stack bang because if stack overflow is thrown it needs to be pushed (and preserved)
  __ movptr(rbp, Address(rdi, Deoptimization::UnrollBlock::initial_info_offset_in_bytes()));

#ifdef ASSERT
  // Compilers generate code that bang the stack by as much as the
  // interpreter would need. So this stack banging should never
  // trigger a fault. Verify that it does not on non product builds.
  if (UseStackBanging) {
    __ movl(rbx, Address(rdi, Deoptimization::UnrollBlock::total_frame_sizes_offset_in_bytes()));
    __ bang_stack_size(rbx, rcx);
  }
#endif

  // Load address of array of frame pcs into rcx
  __ movptr(rcx, Address(rdi, Deoptimization::UnrollBlock::frame_pcs_offset_in_bytes()));

  // Trash the old pc
  __ addptr(rsp, wordSize);

  // Load address of array of frame sizes into rsi
  __ movptr(rsi, Address(rdi, Deoptimization::UnrollBlock::frame_sizes_offset_in_bytes()));

  // Load counter into rdx
  __ movl(rdx, Address(rdi, Deoptimization::UnrollBlock::number_of_frames_offset_in_bytes()));

  // Now adjust the caller's stack to make up for the extra locals
  // but record the original sp so that we can save it in the skeletal interpreter
  // frame and the stack walking of interpreter_sender will get the unextended sp
  // value and not the "real" sp value.

  const Register sender_sp = r8;

  __ mov(sender_sp, rsp);
  __ movl(rbx, Address(rdi,
                       Deoptimization::UnrollBlock::
                       caller_adjustment_offset_in_bytes()));
  __ subptr(rsp, rbx);

  // Push interpreter frames in a loop
  Label loop;
  __ bind(loop);
  __ movptr(rbx, Address(rsi, 0));      // Load frame size
#ifdef CC_INTERP
  __ subptr(rbx, 4*wordSize);           // we'll push pc and ebp by hand and
#ifdef ASSERT
  __ push(0xDEADDEAD);                  // Make a recognizable pattern
  __ push(0xDEADDEAD);
#else /* ASSERT */
  __ subptr(rsp, 2*wordSize);           // skip the "static long no_param"
#endif /* ASSERT */
#else
  __ subptr(rbx, 2*wordSize);           // We'll push pc and ebp by hand
#endif // CC_INTERP
  __ pushptr(Address(rcx, 0));          // Save return address
  __ enter();                           // Save old & set new ebp
  __ subptr(rsp, rbx);                  // Prolog
#ifdef CC_INTERP
  __ movptr(Address(rbp,
                  -(sizeof(BytecodeInterpreter)) + in_bytes(byte_offset_of(BytecodeInterpreter, _sender_sp))),
            sender_sp); // Make it walkable
#else /* CC_INTERP */
  // This value is corrected by layout_activation_impl
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD );
  __ movptr(Address(rbp, frame::interpreter_frame_sender_sp_offset * wordSize), sender_sp); // Make it walkable
#endif /* CC_INTERP */
  __ mov(sender_sp, rsp);               // Pass sender_sp to next frame
  __ addptr(rsi, wordSize);             // Bump array pointer (sizes)
  __ addptr(rcx, wordSize);             // Bump array pointer (pcs)
  __ decrementl(rdx);                   // Decrement counter
  __ jcc(Assembler::notZero, loop);
  __ pushptr(Address(rcx, 0));          // Save final return address

  // Re-push self-frame
  __ enter();                           // Save old & set new ebp

  // Allocate a full sized register save area.
  // Return address and rbp are in place, so we allocate two less words.
  __ subptr(rsp, (frame_size_in_words - 2) * wordSize);

  // Restore frame locals after moving the frame
  __ movdbl(Address(rsp, RegisterSaver::xmm0_offset_in_bytes()), xmm0);
  __ movptr(Address(rsp, RegisterSaver::rax_offset_in_bytes()), rax);

  // Call C code.  Need thread but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.  Call should
  // restore return values to their stack-slots with the new SP.
  //
  // void Deoptimization::unpack_frames(JavaThread* thread, int exec_mode)

  // Use rbp because the frames look interpreted now
  // Save "the_pc" since it cannot easily be retrieved using the last_java_SP after we aligned SP.
  // Don't need the precise return PC here, just precise enough to point into this code blob.
  address the_pc = __ pc();
  __ set_last_Java_frame(noreg, rbp, the_pc);

  __ andptr(rsp, -(StackAlignmentInBytes));  // Fix stack alignment as required by ABI
  __ mov(c_rarg0, r15_thread);
  __ movl(c_rarg1, r14); // second arg: exec_mode
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, Deoptimization::unpack_frames)));
  // Revert SP alignment after call since we're going to do some SP relative addressing below
  __ movptr(rsp, Address(r15_thread, JavaThread::last_Java_sp_offset()));

  // Set an oopmap for the call site
  // Use the same PC we used for the last java frame
  oop_maps->add_gc_map(the_pc - start,
                       new OopMap( frame_size_in_words, 0 ));

  // Clear fp AND pc
  __ reset_last_Java_frame(true);

  // Collect return values
  __ movdbl(xmm0, Address(rsp, RegisterSaver::xmm0_offset_in_bytes()));
  __ movptr(rax, Address(rsp, RegisterSaver::rax_offset_in_bytes()));
  // I think this is useless (throwing pc?)
  __ movptr(rdx, Address(rsp, RegisterSaver::rdx_offset_in_bytes()));

  // Pop self-frame.
  __ leave();                           // Epilog

  // Jump to interpreter
  __ ret(0);

  // Make sure all code is generated
  masm->flush();

  _deopt_blob = DeoptimizationBlob::create(&buffer, oop_maps, 0, exception_offset, reexecute_offset, frame_size_in_words);
  _deopt_blob->set_unpack_with_exception_in_tls_offset(exception_in_tls_offset);
}

#ifdef COMPILER2
//------------------------------generate_uncommon_trap_blob--------------------
void SharedRuntime::generate_uncommon_trap_blob() {
  // Allocate space for the code
  ResourceMark rm;
  // Setup code generation tools
  CodeBuffer buffer("uncommon_trap_blob", 2048, 1024);
  MacroAssembler* masm = new MacroAssembler(&buffer);

  assert(SimpleRuntimeFrame::framesize % 4 == 0, "sp not 16-byte aligned");

  address start = __ pc();

  if (UseRTMLocking) {
    // Abort RTM transaction before possible nmethod deoptimization.
    __ xabort(0);
  }

  // Push self-frame.  We get here with a return address on the
  // stack, so rsp is 8-byte aligned until we allocate our frame.
  __ subptr(rsp, SimpleRuntimeFrame::return_off << LogBytesPerInt); // Epilog!

  // No callee saved registers. rbp is assumed implicitly saved
  __ movptr(Address(rsp, SimpleRuntimeFrame::rbp_off << LogBytesPerInt), rbp);

  // compiler left unloaded_class_index in j_rarg0 move to where the
  // runtime expects it.
  __ movl(c_rarg1, j_rarg0);

  __ set_last_Java_frame(noreg, noreg, NULL);

  // Call C code.  Need thread but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.  Call should
  // capture callee-saved registers as well as return values.
  // Thread is in rdi already.
  //
  // UnrollBlock* uncommon_trap(JavaThread* thread, jint unloaded_class_index);

  __ mov(c_rarg0, r15_thread);
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, Deoptimization::uncommon_trap)));

  // Set an oopmap for the call site
  OopMapSet* oop_maps = new OopMapSet();
  OopMap* map = new OopMap(SimpleRuntimeFrame::framesize, 0);

  // location of rbp is known implicitly by the frame sender code

  oop_maps->add_gc_map(__ pc() - start, map);

  __ reset_last_Java_frame(false);

  // Load UnrollBlock* into rdi
  __ mov(rdi, rax);

  // Pop all the frames we must move/replace.
  //
  // Frame picture (youngest to oldest)
  // 1: self-frame (no frame link)
  // 2: deopting frame  (no frame link)
  // 3: caller of deopting frame (could be compiled/interpreted).

  // Pop self-frame.  We have no frame, and must rely only on rax and rsp.
  __ addptr(rsp, (SimpleRuntimeFrame::framesize - 2) << LogBytesPerInt); // Epilog!

  // Pop deoptimized frame (int)
  __ movl(rcx, Address(rdi,
                       Deoptimization::UnrollBlock::
                       size_of_deoptimized_frame_offset_in_bytes()));
  __ addptr(rsp, rcx);

  // rsp should be pointing at the return address to the caller (3)

  // Pick up the initial fp we should save
  // restore rbp before stack bang because if stack overflow is thrown it needs to be pushed (and preserved)
  __ movptr(rbp, Address(rdi, Deoptimization::UnrollBlock::initial_info_offset_in_bytes()));

#ifdef ASSERT
  // Compilers generate code that bang the stack by as much as the
  // interpreter would need. So this stack banging should never
  // trigger a fault. Verify that it does not on non product builds.
  if (UseStackBanging) {
    __ movl(rbx, Address(rdi ,Deoptimization::UnrollBlock::total_frame_sizes_offset_in_bytes()));
    __ bang_stack_size(rbx, rcx);
  }
#endif

  // Load address of array of frame pcs into rcx (address*)
  __ movptr(rcx, Address(rdi, Deoptimization::UnrollBlock::frame_pcs_offset_in_bytes()));

  // Trash the return pc
  __ addptr(rsp, wordSize);

  // Load address of array of frame sizes into rsi (intptr_t*)
  __ movptr(rsi, Address(rdi, Deoptimization::UnrollBlock:: frame_sizes_offset_in_bytes()));

  // Counter
  __ movl(rdx, Address(rdi, Deoptimization::UnrollBlock:: number_of_frames_offset_in_bytes())); // (int)

  // Now adjust the caller's stack to make up for the extra locals but
  // record the original sp so that we can save it in the skeletal
  // interpreter frame and the stack walking of interpreter_sender
  // will get the unextended sp value and not the "real" sp value.

  const Register sender_sp = r8;

  __ mov(sender_sp, rsp);
  __ movl(rbx, Address(rdi, Deoptimization::UnrollBlock:: caller_adjustment_offset_in_bytes())); // (int)
  __ subptr(rsp, rbx);

  // Push interpreter frames in a loop
  Label loop;
  __ bind(loop);
  __ movptr(rbx, Address(rsi, 0)); // Load frame size
  __ subptr(rbx, 2 * wordSize);    // We'll push pc and rbp by hand
  __ pushptr(Address(rcx, 0));     // Save return address
  __ enter();                      // Save old & set new rbp
  __ subptr(rsp, rbx);             // Prolog
#ifdef CC_INTERP
  __ movptr(Address(rbp,
                  -(sizeof(BytecodeInterpreter)) + in_bytes(byte_offset_of(BytecodeInterpreter, _sender_sp))),
            sender_sp); // Make it walkable
#else // CC_INTERP
  __ movptr(Address(rbp, frame::interpreter_frame_sender_sp_offset * wordSize),
            sender_sp);            // Make it walkable
  // This value is corrected by layout_activation_impl
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD );
#endif // CC_INTERP
  __ mov(sender_sp, rsp);          // Pass sender_sp to next frame
  __ addptr(rsi, wordSize);        // Bump array pointer (sizes)
  __ addptr(rcx, wordSize);        // Bump array pointer (pcs)
  __ decrementl(rdx);              // Decrement counter
  __ jcc(Assembler::notZero, loop);
  __ pushptr(Address(rcx, 0));     // Save final return address

  // Re-push self-frame
  __ enter();                 // Save old & set new rbp
  __ subptr(rsp, (SimpleRuntimeFrame::framesize - 4) << LogBytesPerInt);
                              // Prolog

  // Use rbp because the frames look interpreted now
  // Save "the_pc" since it cannot easily be retrieved using the last_java_SP after we aligned SP.
  // Don't need the precise return PC here, just precise enough to point into this code blob.
  address the_pc = __ pc();
  __ set_last_Java_frame(noreg, rbp, the_pc);

  // Call C code.  Need thread but NOT official VM entry
  // crud.  We cannot block on this call, no GC can happen.  Call should
  // restore return values to their stack-slots with the new SP.
  // Thread is in rdi already.
  //
  // BasicType unpack_frames(JavaThread* thread, int exec_mode);

  __ andptr(rsp, -(StackAlignmentInBytes)); // Align SP as required by ABI
  __ mov(c_rarg0, r15_thread);
  __ movl(c_rarg1, Deoptimization::Unpack_uncommon_trap);
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, Deoptimization::unpack_frames)));

  // Set an oopmap for the call site
  // Use the same PC we used for the last java frame
  oop_maps->add_gc_map(the_pc - start, new OopMap(SimpleRuntimeFrame::framesize, 0));

  // Clear fp AND pc
  __ reset_last_Java_frame(true);

  // Pop self-frame.
  __ leave();                 // Epilog

  // Jump to interpreter
  __ ret(0);

  // Make sure all code is generated
  masm->flush();

  _uncommon_trap_blob =  UncommonTrapBlob::create(&buffer, oop_maps,
                                                 SimpleRuntimeFrame::framesize >> 1);
}
#endif // COMPILER2


//------------------------------generate_handler_blob------
//
// Generate a special Compile2Runtime blob that saves all registers,
// and setup oopmap.
//
SafepointBlob* SharedRuntime::generate_handler_blob(address call_ptr, int poll_type) {
  assert(StubRoutines::forward_exception_entry() != NULL,
         "must be generated before");

  ResourceMark rm;
  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map;

  // Allocate space for the code.  Setup code generation tools.
  CodeBuffer buffer("handler_blob", 2048, 1024);
  MacroAssembler* masm = new MacroAssembler(&buffer);

  address start   = __ pc();
  address call_pc = NULL;
  int frame_size_in_words;
  bool cause_return = (poll_type == POLL_AT_RETURN);
  bool save_vectors = (poll_type == POLL_AT_VECTOR_LOOP);

  if (UseRTMLocking) {
    // Abort RTM transaction before calling runtime
    // because critical section will be large and will be
    // aborted anyway. Also nmethod could be deoptimized.
    __ xabort(0);
  }

  // Make room for return address (or push it again)
  if (!cause_return) {
    __ push(rbx);
  }

  // Save registers, fpu state, and flags
  map = RegisterSaver::save_live_registers(masm, 0, &frame_size_in_words, save_vectors);

  // The following is basically a call_VM.  However, we need the precise
  // address of the call in order to generate an oopmap. Hence, we do all the
  // work outselves.

  __ set_last_Java_frame(noreg, noreg, NULL);

  // The return address must always be correct so that frame constructor never
  // sees an invalid pc.

  if (!cause_return) {
    // overwrite the dummy value we pushed on entry
    __ movptr(c_rarg0, Address(r15_thread, JavaThread::saved_exception_pc_offset()));
    __ movptr(Address(rbp, wordSize), c_rarg0);
  }

  // Do the call
  __ mov(c_rarg0, r15_thread);
  __ call(RuntimeAddress(call_ptr));

  // Set an oopmap for the call site.  This oopmap will map all
  // oop-registers and debug-info registers as callee-saved.  This
  // will allow deoptimization at this safepoint to find all possible
  // debug-info recordings, as well as let GC find all oops.

  oop_maps->add_gc_map( __ pc() - start, map);

  Label noException;

  __ reset_last_Java_frame(false);

  __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t)NULL_WORD);
  __ jcc(Assembler::equal, noException);

  // Exception pending

  RegisterSaver::restore_live_registers(masm, save_vectors);

  __ jump(RuntimeAddress(StubRoutines::forward_exception_entry()));

  // No exception case
  __ bind(noException);

  // Normal exit, restore registers and exit.
  RegisterSaver::restore_live_registers(masm, save_vectors);

  __ ret(0);

  // Make sure all code is generated
  masm->flush();

  // Fill-out other meta info
  return SafepointBlob::create(&buffer, oop_maps, frame_size_in_words);
}

//
// generate_resolve_blob - call resolution (static/virtual/opt-virtual/ic-miss
//
// Generate a stub that calls into vm to find out the proper destination
// of a java call. All the argument registers are live at this point
// but since this is generic code we don't know what they are and the caller
// must do any gc of the args.
//
RuntimeStub* SharedRuntime::generate_resolve_blob(address destination, const char* name) {
  assert (StubRoutines::forward_exception_entry() != NULL, "must be generated before");

  // allocate space for the code
  ResourceMark rm;

  CodeBuffer buffer(name, 1000, 512);
  MacroAssembler* masm                = new MacroAssembler(&buffer);

  int frame_size_in_words;

  OopMapSet *oop_maps = new OopMapSet();
  OopMap* map = NULL;

  int start = __ offset();

  map = RegisterSaver::save_live_registers(masm, 0, &frame_size_in_words);

  int frame_complete = __ offset();

  __ set_last_Java_frame(noreg, noreg, NULL);

  __ mov(c_rarg0, r15_thread);

  __ call(RuntimeAddress(destination));


  // Set an oopmap for the call site.
  // We need this not only for callee-saved registers, but also for volatile
  // registers that the compiler might be keeping live across a safepoint.

  oop_maps->add_gc_map( __ offset() - start, map);

  // rax contains the address we are going to jump to assuming no exception got installed

  // clear last_Java_sp
  __ reset_last_Java_frame(false);
  // check for pending exceptions
  Label pending;
  __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t)NULL_WORD);
  __ jcc(Assembler::notEqual, pending);

  // get the returned Method*
  __ get_vm_result_2(rbx, r15_thread);
  __ movptr(Address(rsp, RegisterSaver::rbx_offset_in_bytes()), rbx);

  __ movptr(Address(rsp, RegisterSaver::rax_offset_in_bytes()), rax);

  RegisterSaver::restore_live_registers(masm);

  // We are back the the original state on entry and ready to go.

  __ jmp(rax);

  // Pending exception after the safepoint

  __ bind(pending);

  RegisterSaver::restore_live_registers(masm);

  // exception pending => remove activation and forward to exception handler

  __ movptr(Address(r15_thread, JavaThread::vm_result_offset()), (int)NULL_WORD);

  __ movptr(rax, Address(r15_thread, Thread::pending_exception_offset()));
  __ jump(RuntimeAddress(StubRoutines::forward_exception_entry()));

  // -------------
  // make sure all code is generated
  masm->flush();

  // return the  blob
  // frame_size_words or bytes??
  return RuntimeStub::new_runtime_stub(name, &buffer, frame_complete, frame_size_in_words, oop_maps, true);
}


//------------------------------Montgomery multiplication------------------------
//

#ifndef _WINDOWS

#define ASM_SUBTRACT

#ifdef ASM_SUBTRACT
// Subtract 0:b from carry:a.  Return carry.
static unsigned long
sub(unsigned long a[], unsigned long b[], unsigned long carry, long len) {
  long i = 0, cnt = len;
  unsigned long tmp;
  asm volatile("clc; "
               "0: ; "
               "mov (%[b], %[i], 8), %[tmp]; "
               "sbb %[tmp], (%[a], %[i], 8); "
               "inc %[i]; dec %[cnt]; "
               "jne 0b; "
               "mov %[carry], %[tmp]; sbb $0, %[tmp]; "
               : [i]"+r"(i), [cnt]"+r"(cnt), [tmp]"=&r"(tmp)
               : [a]"r"(a), [b]"r"(b), [carry]"r"(carry)
               : "memory");
  return tmp;
}
#else // ASM_SUBTRACT
typedef int __attribute__((mode(TI))) int128;

// Subtract 0:b from carry:a.  Return carry.
static unsigned long
sub(unsigned long a[], unsigned long b[], unsigned long carry, int len) {
  int128 tmp = 0;
  int i;
  for (i = 0; i < len; i++) {
    tmp += a[i];
    tmp -= b[i];
    a[i] = tmp;
    tmp >>= 64;
    assert(-1 <= tmp && tmp <= 0, "invariant");
  }
  return tmp + carry;
}
#endif // ! ASM_SUBTRACT

// Multiply (unsigned) Long A by Long B, accumulating the double-
// length result into the accumulator formed of T0, T1, and T2.
#define MACC(A, B, T0, T1, T2)                                      \
do {                                                                \
  unsigned long hi, lo;                                             \
  asm volatile("mul %5; add %%rax, %2; adc %%rdx, %3; adc $0, %4"   \
           : "=&d"(hi), "=a"(lo), "+r"(T0), "+r"(T1), "+g"(T2)      \
           : "r"(A), "a"(B) : "cc");                                \
 } while(0)

// As above, but add twice the double-length result into the
// accumulator.
#define MACC2(A, B, T0, T1, T2)                                     \
do {                                                                \
  unsigned long hi, lo;                                             \
  asm volatile("mul %5; add %%rax, %2; adc %%rdx, %3; adc $0, %4;"  \
           "add %%rax, %2; adc %%rdx, %3; adc $0, %4"               \
           : "=&d"(hi), "=a"(lo), "+r"(T0), "+r"(T1), "+g"(T2)      \
           : "r"(A), "a"(B) : "cc");                                \
 } while(0)

// Fast Montgomery multiplication.  The derivation of the algorithm is
// in  A Cryptographic Library for the Motorola DSP56000,
// Dusse and Kaliski, Proc. EUROCRYPT 90, pp. 230-237.

static void __attribute__((noinline))
montgomery_multiply(unsigned long a[], unsigned long b[], unsigned long n[],
                    unsigned long m[], unsigned long inv, int len) {
  unsigned long t0 = 0, t1 = 0, t2 = 0; // Triple-precision accumulator
  int i;

  assert(inv * n[0] == -1UL, "broken inverse in Montgomery multiply");

  for (i = 0; i < len; i++) {
    int j;
    for (j = 0; j < i; j++) {
      MACC(a[j], b[i-j], t0, t1, t2);
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    MACC(a[i], b[0], t0, t1, t2);
    m[i] = t0 * inv;
    MACC(m[i], n[0], t0, t1, t2);

    assert(t0 == 0, "broken Montgomery multiply");

    t0 = t1; t1 = t2; t2 = 0;
  }

  for (i = len; i < 2*len; i++) {
    int j;
    for (j = i-len+1; j < len; j++) {
      MACC(a[j], b[i-j], t0, t1, t2);
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    m[i-len] = t0;
    t0 = t1; t1 = t2; t2 = 0;
  }

  while (t0)
    t0 = sub(m, n, t0, len);
}

// Fast Montgomery squaring.  This uses asymptotically 25% fewer
// multiplies so it should be up to 25% faster than Montgomery
// multiplication.  However, its loop control is more complex and it
// may actually run slower on some machines.

static void __attribute__((noinline))
montgomery_square(unsigned long a[], unsigned long n[],
                  unsigned long m[], unsigned long inv, int len) {
  unsigned long t0 = 0, t1 = 0, t2 = 0; // Triple-precision accumulator
  int i;

  assert(inv * n[0] == -1UL, "broken inverse in Montgomery multiply");

  for (i = 0; i < len; i++) {
    int j;
    int end = (i+1)/2;
    for (j = 0; j < end; j++) {
      MACC2(a[j], a[i-j], t0, t1, t2);
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    if ((i & 1) == 0) {
      MACC(a[j], a[j], t0, t1, t2);
    }
    for (; j < i; j++) {
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    m[i] = t0 * inv;
    MACC(m[i], n[0], t0, t1, t2);

    assert(t0 == 0, "broken Montgomery square");

    t0 = t1; t1 = t2; t2 = 0;
  }

  for (i = len; i < 2*len; i++) {
    int start = i-len+1;
    int end = start + (len - start)/2;
    int j;
    for (j = start; j < end; j++) {
      MACC2(a[j], a[i-j], t0, t1, t2);
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    if ((i & 1) == 0) {
      MACC(a[j], a[j], t0, t1, t2);
    }
    for (; j < len; j++) {
      MACC(m[j], n[i-j], t0, t1, t2);
    }
    m[i-len] = t0;
    t0 = t1; t1 = t2; t2 = 0;
  }

  while (t0)
    t0 = sub(m, n, t0, len);
}

// Swap words in a longword.
static unsigned long swap(unsigned long x) {
  return (x << 32) | (x >> 32);
}

// Copy len longwords from s to d, word-swapping as we go.  The
// destination array is reversed.
static void reverse_words(unsigned long *s, unsigned long *d, int len) {
  d += len;
  while(len-- > 0) {
    d--;
    *d = swap(*s);
    s++;
  }
}

// The threshold at which squaring is advantageous was determined
// experimentally on an i7-3930K (Ivy Bridge) CPU @ 3.5GHz.
#define MONTGOMERY_SQUARING_THRESHOLD 64

void SharedRuntime::montgomery_multiply(jint *a_ints, jint *b_ints, jint *n_ints,
                                        jint len, jlong inv,
                                        jint *m_ints) {
  assert(len % 2 == 0, "array length in montgomery_multiply must be even");
  int longwords = len/2;

  // Make very sure we don't use so much space that the stack might
  // overflow.  512 jints corresponds to an 16384-bit integer and
  // will use here a total of 8k bytes of stack space.
  int total_allocation = longwords * sizeof (unsigned long) * 4;
  guarantee(total_allocation <= 8192, "must be");
  unsigned long *scratch = (unsigned long *)alloca(total_allocation);

  // Local scratch arrays
  unsigned long
    *a = scratch + 0 * longwords,
    *b = scratch + 1 * longwords,
    *n = scratch + 2 * longwords,
    *m = scratch + 3 * longwords;

  reverse_words((unsigned long *)a_ints, a, longwords);
  reverse_words((unsigned long *)b_ints, b, longwords);
  reverse_words((unsigned long *)n_ints, n, longwords);

  ::montgomery_multiply(a, b, n, m, (unsigned long)inv, longwords);

  reverse_words(m, (unsigned long *)m_ints, longwords);
}

void SharedRuntime::montgomery_square(jint *a_ints, jint *n_ints,
                                      jint len, jlong inv,
                                      jint *m_ints) {
  assert(len % 2 == 0, "array length in montgomery_square must be even");
  int longwords = len/2;

  // Make very sure we don't use so much space that the stack might
  // overflow.  512 jints corresponds to an 16384-bit integer and
  // will use here a total of 6k bytes of stack space.
  int total_allocation = longwords * sizeof (unsigned long) * 3;
  guarantee(total_allocation <= 8192, "must be");
  unsigned long *scratch = (unsigned long *)alloca(total_allocation);

  // Local scratch arrays
  unsigned long
    *a = scratch + 0 * longwords,
    *n = scratch + 1 * longwords,
    *m = scratch + 2 * longwords;

  reverse_words((unsigned long *)a_ints, a, longwords);
  reverse_words((unsigned long *)n_ints, n, longwords);

  //montgomery_square fails to pass BigIntegerTest on solaris amd64
  //on jdk7 and jdk8.
#ifndef SOLARIS
  if (len >= MONTGOMERY_SQUARING_THRESHOLD) {
#else
  if (0) {
#endif
    ::montgomery_square(a, n, m, (unsigned long)inv, longwords);
  } else {
    ::montgomery_multiply(a, a, n, m, (unsigned long)inv, longwords);
  }

  reverse_words(m, (unsigned long *)m_ints, longwords);
}

#endif // WINDOWS

#ifdef COMPILER2
// This is here instead of runtime_x86_64.cpp because it uses SimpleRuntimeFrame
//
//------------------------------generate_exception_blob---------------------------
// creates exception blob at the end
// Using exception blob, this code is jumped from a compiled method.
// (see emit_exception_handler in x86_64.ad file)
//
// Given an exception pc at a call we call into the runtime for the
// handler in this method. This handler might merely restore state
// (i.e. callee save registers) unwind the frame and jump to the
// exception handler for the nmethod if there is no Java level handler
// for the nmethod.
//
// This code is entered with a jmp.
//
// Arguments:
//   rax: exception oop
//   rdx: exception pc
//
// Results:
//   rax: exception oop
//   rdx: exception pc in caller or ???
//   destination: exception handler of caller
//
// Note: the exception pc MUST be at a call (precise debug information)
//       Registers rax, rdx, rcx, rsi, rdi, r8-r11 are not callee saved.
//

void OptoRuntime::generate_exception_blob() {
  assert(!OptoRuntime::is_callee_saved_register(RDX_num), "");
  assert(!OptoRuntime::is_callee_saved_register(RAX_num), "");
  assert(!OptoRuntime::is_callee_saved_register(RCX_num), "");

  assert(SimpleRuntimeFrame::framesize % 4 == 0, "sp not 16-byte aligned");

  // Allocate space for the code
  ResourceMark rm;
  // Setup code generation tools
  CodeBuffer buffer("exception_blob", 2048, 1024);
  MacroAssembler* masm = new MacroAssembler(&buffer);


  address start = __ pc();

  // Exception pc is 'return address' for stack walker
  __ push(rdx);
  __ subptr(rsp, SimpleRuntimeFrame::return_off << LogBytesPerInt); // Prolog

  // Save callee-saved registers.  See x86_64.ad.

  // rbp is an implicitly saved callee saved register (i.e., the calling
  // convention will save/restore it in the prolog/epilog). Other than that
  // there are no callee save registers now that adapter frames are gone.

  __ movptr(Address(rsp, SimpleRuntimeFrame::rbp_off << LogBytesPerInt), rbp);

  // Store exception in Thread object. We cannot pass any arguments to the
  // handle_exception call, since we do not want to make any assumption
  // about the size of the frame where the exception happened in.
  // c_rarg0 is either rdi (Linux) or rcx (Windows).
  __ movptr(Address(r15_thread, JavaThread::exception_oop_offset()),rax);
  __ movptr(Address(r15_thread, JavaThread::exception_pc_offset()), rdx);

  // This call does all the hard work.  It checks if an exception handler
  // exists in the method.
  // If so, it returns the handler address.
  // If not, it prepares for stack-unwinding, restoring the callee-save
  // registers of the frame being removed.
  //
  // address OptoRuntime::handle_exception_C(JavaThread* thread)

  // At a method handle call, the stack may not be properly aligned
  // when returning with an exception.
  address the_pc = __ pc();
  __ set_last_Java_frame(noreg, noreg, the_pc);
  __ mov(c_rarg0, r15_thread);
  __ andptr(rsp, -(StackAlignmentInBytes));    // Align stack
  __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, OptoRuntime::handle_exception_C)));

  // Set an oopmap for the call site.  This oopmap will only be used if we
  // are unwinding the stack.  Hence, all locations will be dead.
  // Callee-saved registers will be the same as the frame above (i.e.,
  // handle_exception_stub), since they were restored when we got the
  // exception.

  OopMapSet* oop_maps = new OopMapSet();

  oop_maps->add_gc_map(the_pc - start, new OopMap(SimpleRuntimeFrame::framesize, 0));

  __ reset_last_Java_frame(false);

  // Restore callee-saved registers

  // rbp is an implicitly saved callee-saved register (i.e., the calling
  // convention will save restore it in prolog/epilog) Other than that
  // there are no callee save registers now that adapter frames are gone.

  __ movptr(rbp, Address(rsp, SimpleRuntimeFrame::rbp_off << LogBytesPerInt));

  __ addptr(rsp, SimpleRuntimeFrame::return_off << LogBytesPerInt); // Epilog
  __ pop(rdx);                  // No need for exception pc anymore

  // rax: exception handler

  // We have a handler in rax (could be deopt blob).
  __ mov(r8, rax);

  // Get the exception oop
  __ movptr(rax, Address(r15_thread, JavaThread::exception_oop_offset()));
  // Get the exception pc in case we are deoptimized
  __ movptr(rdx, Address(r15_thread, JavaThread::exception_pc_offset()));
#ifdef ASSERT
  __ movptr(Address(r15_thread, JavaThread::exception_handler_pc_offset()), (int)NULL_WORD);
  __ movptr(Address(r15_thread, JavaThread::exception_pc_offset()), (int)NULL_WORD);
#endif
  // Clear the exception oop so GC no longer processes it as a root.
  __ movptr(Address(r15_thread, JavaThread::exception_oop_offset()), (int)NULL_WORD);

  // rax: exception oop
  // r8:  exception handler
  // rdx: exception pc
  // Jump to handler

  __ jmp(r8);

  // Make sure all code is generated
  masm->flush();

  // Set exception blob
  _exception_blob =  ExceptionBlob::create(&buffer, oop_maps, SimpleRuntimeFrame::framesize >> 1);
}
#endif // COMPILER2


void stop_if(MacroAssembler *masm, Assembler::Condition condition, const char* message) {
  Label skip;
  __ jcc(masm->negate_condition(condition), skip);

  __ stop(message);
  __ int3();

  __ bind(skip);
}

void stop_if_null(MacroAssembler *masm, Register reg, const char* message) {
  __ testptr(reg, reg);
  stop_if(masm, Assembler::zero, message);
}

void stop_if_null(MacroAssembler *masm, Address adr, const char* message) {
  __ cmpptr(adr, 0);
  stop_if(masm, Assembler::zero, message);
}

MacroAssembler* debug_line(MacroAssembler* masm, int l) {
  masm->movl(r13, l);
  return masm;
}

void create_switchTo_contents(MacroAssembler *masm, int start, OopMapSet* oop_maps, int &stack_slots, int total_in_args,
                              BasicType *in_sig_bt, VMRegPair *in_regs, BasicType ret_type, bool terminate) {
  assert(total_in_args == 2, "wrong number of arguments");

  if (j_rarg0 != rsi) {
    __ movptr(rsi, j_rarg0);
  }
  if (j_rarg1 != rdx) {
    __ movptr(rdx, j_rarg1);
  }

  // push the current IP and frame pointer onto the stack
  __ push(rbp);

  Register thread = r15;
  Register target_coroutine = rdx;
  // check that we're dealing with sane objects...
  DEBUG_ONLY(stop_if_null(masm, target_coroutine, "null new_coroutine"));
  __ movptr(target_coroutine, Address(target_coroutine, java_dyn_CoroutineBase::get_data_offset()));
  DEBUG_ONLY(stop_if_null(masm, target_coroutine, "new_coroutine without data"));

/*
#ifdef ASSERT
#undef __
#define __ debug_line(masm, __LINE__)->
#endif
*/
  {
    //////////////////////////////////////////////////////////////////////////
    // store information into the old coroutine's object
    //
    // valid registers: rsi = old Coroutine, rdx = target Coroutine

    Register old_coroutine_obj = rsi;
    Register old_coroutine = r9;
    Register old_stack = r10;
    Register temp = r8;

    // check that we're dealing with sane objects...
    DEBUG_ONLY(stop_if_null(masm, old_coroutine_obj, "null old_coroutine"));
    __ movptr(old_coroutine, Address(old_coroutine_obj, java_dyn_CoroutineBase::get_data_offset()));
    DEBUG_ONLY(stop_if_null(masm, old_coroutine, "old_coroutine without data"));
    __ movptr(old_stack, Address(old_coroutine, Coroutine::stack_offset()));

#if defined(_WINDOWS)
    // rescue the SEH pointer
    __ prefix(Assembler::GS_segment);
    __ movptr(temp, Address(noreg, 0x00));
    __ movptr(Address(old_coroutine, Coroutine::last_SEH_offset()), temp);
#endif

    __ movl(Address(old_coroutine, Coroutine::state_offset()) , Coroutine::_onstack);

    // rescue old handle and resource areas
    __ movptr(temp, Address(thread, Thread::handle_area_offset()));
    __ movptr(Address(old_coroutine, Coroutine::handle_area_offset()), temp);
    __ movptr(temp, Address(thread, Thread::resource_area_offset()));
    __ movptr(Address(old_coroutine, Coroutine::resource_area_offset()), temp);
    __ movptr(temp, Address(thread, Thread::last_handle_mark_offset()));
    __ movptr(Address(old_coroutine, Coroutine::last_handle_mark_offset()), temp);
    __ movptr(temp, Address(thread, Thread::active_handles_offset()));
    __ movptr(Address(old_coroutine, Coroutine::active_handles_offset()), temp);
    __ movptr(temp, Address(thread, Thread::metadata_handles_offset()));
    __ movptr(Address(old_coroutine, Coroutine::metadata_handles_offset()), temp);
    __ movptr(temp, Address(thread, JavaThread::last_Java_pc_offset()));
    __ movptr(Address(old_coroutine, Coroutine::last_Java_pc_offset()), temp);
    __ movptr(temp, Address(thread, JavaThread::last_Java_sp_offset()));
    __ movptr(Address(old_coroutine, Coroutine::last_Java_sp_offset()), temp);
    __ movptr(temp, Address(thread, JavaThread::privileged_stack_top_offset()));
    __ movptr(Address(old_coroutine, Coroutine::privileged_stack_top_offset()), temp);
    __ movptr(temp, Address(thread, JavaThread::threadObj_offset()));
    __ movl(temp, Address(temp, java_lang_Thread::thread_status_offset()));
    __ movl(Address(old_coroutine, Coroutine::thread_status_offset()), temp);
    __ movl(temp, Address(thread, JavaThread::java_call_counter_offset()));
    __ movl(Address(old_coroutine, Coroutine::java_call_counter_offset()), temp);

    if (EagerAppCDS && !UseSharedSpaces) {
      __ movptr(temp, Address(thread, JavaThread::initiating_loader_offset()));
      __ movptr(Address(old_coroutine, Coroutine::initiating_loader_offset()), temp);
    }
    __ movptr(Address(old_stack, CoroutineStack::last_sp_offset()), rsp);
  }
  Register target_stack = r12;
  __ movptr(target_stack, Address(target_coroutine, Coroutine::stack_offset()));

  {
    //////////////////////////////////////////////////////////////////////////
    // perform the switch to the new stack
    //
    // valid registers: rdx = target Coroutine

    __ movl(Address(target_coroutine, Coroutine::state_offset()), Coroutine::_current);

    Register temp = r8;
    Register temp2 = r9;
    {
      Register thread = r15;
      __ movptr(Address(thread, JavaThread::current_coroutine_offset()), target_coroutine);
      // set new handle and resource areas
      __ movptr(temp, Address(target_coroutine, Coroutine::handle_area_offset()));
      __ movptr(Address(thread, Thread::handle_area_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::resource_area_offset()));
      __ movptr(Address(thread, Thread::resource_area_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::last_handle_mark_offset()));
      __ movptr(Address(thread, Thread::last_handle_mark_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::active_handles_offset()));
      __ movptr(Address(thread, Thread::active_handles_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::metadata_handles_offset()));
      __ movptr(Address(thread, Thread::metadata_handles_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::last_Java_pc_offset()));
      __ movptr(Address(thread, JavaThread::last_Java_pc_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::last_Java_sp_offset()));
      __ movptr(Address(thread, JavaThread::last_Java_sp_offset()), temp);
      __ movptr(temp, Address(target_coroutine, Coroutine::privileged_stack_top_offset()));
      __ movptr(Address(thread, JavaThread::privileged_stack_top_offset()), temp);
      __ movl(temp2, Address(target_coroutine, Coroutine::thread_status_offset()));
      __ movptr(temp, Address(thread, JavaThread::threadObj_offset()));
      __ movl(Address(temp, java_lang_Thread::thread_status_offset()), temp2);
      __ movl(temp, Address(target_coroutine, Coroutine::java_call_counter_offset()));
      __ movl(Address(thread, JavaThread::java_call_counter_offset()), temp);
#ifdef ASSERT
      __ movptr(Address(target_coroutine, Coroutine::handle_area_offset()), (intptr_t)NULL_WORD);
      __ movptr(Address(target_coroutine, Coroutine::resource_area_offset()), (intptr_t)NULL_WORD);
      __ movptr(Address(target_coroutine, Coroutine::last_handle_mark_offset()), (intptr_t)NULL_WORD);
      __ movl(Address(target_coroutine, Coroutine::java_call_counter_offset()), 0);
#endif

      if (EagerAppCDS && !UseSharedSpaces) {
        __ movptr(temp, Address(target_coroutine, Coroutine::initiating_loader_offset()));
        __ movptr(Address(thread, JavaThread::initiating_loader_offset()), temp);
      }

      // update the thread's stack base and size
      __ movptr(temp, Address(target_stack, CoroutineStack::stack_base_offset()));
      __ movptr(Address(thread, JavaThread::stack_base_offset()), temp);
      __ movl(temp2, Address(target_stack, CoroutineStack::stack_size_offset()));
      __ movl(Address(thread, JavaThread::stack_size_offset()), temp2);
    }
#if defined(_WINDOWS)
    {
      Register tib = rax;
      // get the linear address of the TIB (thread info block)
      __ prefix(Assembler::GS_segment);
      __ movptr(tib, Address(noreg, 0x30));

      // update the TIB stack base and top
      __ movptr(Address(tib, 0x8), temp);
      __ subptr(temp, temp2);
      __ movptr(Address(tib, 0x10), temp);

      // exchange the TIB structured exception handler pointer
      __ movptr(temp, Address(target_coroutine, Coroutine::last_SEH_offset()));
      __ movptr(Address(tib, 0), temp);
    }
#endif
    // restore the stack pointer
    __ movptr(temp, Address(target_stack, CoroutineStack::last_sp_offset()));
    __ movptr(rsp, temp);

    __ pop(rbp);

    __ int3();

    if (!terminate) {
      //////////////////////////////////////////////////////////////////////////
      // normal case (resume immediately)

      // this will reset r12
      __ reinit_heapbase();

      Label normal;
      __ lea(rcx, RuntimeAddress((unsigned char*)coroutine_start));
      __ cmpq(Address(rsp, 0), rcx);
      __ jcc(Assembler::notEqual, normal);

      __ movq(c_rarg0, Address(rsp, HeapWordSize * 2));
      __ movq(c_rarg1, Address(rsp, HeapWordSize * 3));

      __ bind(normal);

      __ ret(0);        // <-- this will jump to the stored IP of the target coroutine

    } else {
      //////////////////////////////////////////////////////////////////////////
      // slow case (terminate old coroutine)

      // this will reset r12
      __ reinit_heapbase();

      if (j_rarg0 != rsi) {
        __ movptr(j_rarg0, rsi);
      }
      __ movptr(j_rarg1, 0);
    }
  }
}
