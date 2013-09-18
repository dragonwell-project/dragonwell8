/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates.
 * All rights reserved.
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

#ifndef CPU_AARCH64_VM_MACROASSEMBLER_AARCH64_HPP
#define CPU_AARCH64_VM_MACROASSEMBLER_AARCH64_HPP

#include "asm/assembler.hpp"

// MacroAssembler extends Assembler by frequently used macros.
//
// Instructions for which a 'better' code sequence exists depending
// on arguments should also go in here.

class MacroAssembler: public Assembler {
  friend class LIR_Assembler;

 protected:

  // Support for VM calls
  //
  // This is the base routine called by the different versions of call_VM_leaf. The interpreter
  // may customize this version by overriding it for its purposes (e.g., to save/restore
  // additional registers when doing a VM call).
#ifdef CC_INTERP
  // c++ interpreter never wants to use interp_masm version of call_VM
  #define VIRTUAL
#else
  #define VIRTUAL virtual
#endif

  VIRTUAL void call_VM_leaf_base(
    address entry_point,               // the entry point
    int     number_of_arguments,        // the number of arguments to pop after the call
    Label *retaddr = NULL
  );

  VIRTUAL void call_VM_leaf_base(
    address entry_point,               // the entry point
    int     number_of_arguments,        // the number of arguments to pop after the call
    Label &retaddr) {
    call_VM_leaf_base(entry_point, number_of_arguments, &retaddr);
  }

  // This is the base routine called by the different versions of call_VM. The interpreter
  // may customize this version by overriding it for its purposes (e.g., to save/restore
  // additional registers when doing a VM call).
  //
  // If no java_thread register is specified (noreg) than rthread will be used instead. call_VM_base
  // returns the register which contains the thread upon return. If a thread register has been
  // specified, the return value will correspond to that register. If no last_java_sp is specified
  // (noreg) than rsp will be used instead.
  VIRTUAL void call_VM_base(           // returns the register containing the thread upon return
    Register oop_result,               // where an oop-result ends up if any; use noreg otherwise
    Register java_thread,              // the thread if computed before     ; use noreg otherwise
    Register last_java_sp,             // to set up last_Java_frame in stubs; use noreg otherwise
    address  entry_point,              // the entry point
    int      number_of_arguments,      // the number of arguments (w/o thread) to pop after the call
    bool     check_exceptions          // whether to check for pending exceptions after return
  );

  // These routines should emit JVMTI PopFrame and ForceEarlyReturn handling code.
  // The implementation is only non-empty for the InterpreterMacroAssembler,
  // as only the interpreter handles PopFrame and ForceEarlyReturn requests.
  virtual void check_and_handle_popframe(Register java_thread);
  virtual void check_and_handle_earlyret(Register java_thread);

  void call_VM_helper(Register oop_result, address entry_point, int number_of_arguments, bool check_exceptions = true);

 public:
  MacroAssembler(CodeBuffer* code) : Assembler(code) {}

  // Biased locking support
  // lock_reg and obj_reg must be loaded up with the appropriate values.
  // swap_reg must be rax, and is killed.
  // tmp_reg is optional. If it is supplied (i.e., != noreg) it will
  // be killed; if not supplied, push/pop will be used internally to
  // allocate a temporary (inefficient, avoid if possible).
  // Optional slow case is for implementations (interpreter and C1) which branch to
  // slow case directly. Leaves condition codes set for C2's Fast_Lock node.
  // Returns offset of first potentially-faulting instruction for null
  // check info (currently consumed only by C1). If
  // swap_reg_contains_mark is true then returns -1 as it is assumed
  // the calling code has already passed any potential faults.
  int biased_locking_enter(Register lock_reg, Register obj_reg,
                           Register swap_reg, Register tmp_reg,
                           bool swap_reg_contains_mark,
                           Label& done, Label* slow_case = NULL,
                           BiasedLockingCounters* counters = NULL);
  void biased_locking_exit (Register obj_reg, Register temp_reg, Label& done);

  // Load Effective Address
  void lea(Register r, const Address &a) {
    InstructionMark im(this);
    code_section()->relocate(inst_mark(), a.rspec());
    a.lea(this, r);
  }

  void addmw(Address a, Register incr, Register scratch) {
    ldrw(scratch, a);
    addw(scratch, scratch, incr);
    strw(scratch, a);
  }

  // Add constant to memory word
  void addmw(Address a, int imm, Register scratch) {
    ldrw(scratch, a);
    if (imm > 0)
      addw(scratch, scratch, (unsigned)imm);
    else
      subw(scratch, scratch, (unsigned)-imm);
    strw(scratch, a);
  }

  virtual void _call_Unimplemented(address call_site) {
    mov(rscratch2, call_site);
    haltsim();
  }

#define call_Unimplemented() _call_Unimplemented((address)__PRETTY_FUNCTION__)

  virtual void notify(int type);

  // aliases defined in AARCH64 spec


  template<class T>
  inline void  cmpw(Register Rd, T imm)  { subsw(zr, Rd, imm); }
  inline void cmp(Register Rd, unsigned imm)  { subs(zr, Rd, imm); }

  inline void cmnw(Register Rd, unsigned imm) { addsw(zr, Rd, imm); }
  inline void cmn(Register Rd, unsigned imm) { adds(zr, Rd, imm); }

  void cset(Register Rd, Assembler::Condition cond) {
    csinc(Rd, zr, zr, ~cond);
  }

  void csetw(Register Rd, Assembler::Condition cond) {
    csincw(Rd, zr, zr, ~cond);
  }

  inline void movw(Register Rd, Register Rn) {
    if (Rd == sp || Rn == sp) {
      addw(Rd, Rn, 0U);
    } else {
      orrw(Rd, zr, Rn);
    }
  }
  inline void mov(Register Rd, Register Rn) {
    assert(Rd != r31_sp && Rn != r31_sp, "should be");
    if (Rd == Rn) {
    } else if (Rd == sp || Rn == sp) {
      add(Rd, Rn, 0U);
    } else {
      orr(Rd, zr, Rn);
    }
  }

  inline void moviw(Register Rd, unsigned imm) { orrw(Rd, zr, imm); }
  inline void movi(Register Rd, unsigned imm) { orr(Rd, zr, imm); }

  inline void tstw(Register Rd, unsigned imm) { andsw(zr, Rd, imm); }
  inline void tst(Register Rd, unsigned imm) { ands(zr, Rd, imm); }

  inline void bfiw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    bfmw(Rd, Rn, ((32 - lsb) & 31), (width - 1));
  }
  inline void bfi(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    bfm(Rd, Rn, ((64 - lsb) & 63), (width - 1));
  }

  inline void bfxilw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    bfmw(Rd, Rn, lsb, (lsb + width - 1));
  }
  inline void bfxil(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    bfm(Rd, Rn, lsb , (lsb + width - 1));
  }

  inline void sbfizw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    sbfmw(Rd, Rn, ((32 - lsb) & 31), (width - 1));
  }
  inline void sbfiz(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    sbfm(Rd, Rn, ((64 - lsb) & 63), (width - 1));
  }

  inline void sbfxw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    sbfmw(Rd, Rn, lsb, (lsb + width - 1));
  }
  inline void sbfx(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    sbfm(Rd, Rn, lsb , (lsb + width - 1));
  }

  inline void ubfizw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    ubfmw(Rd, Rn, ((32 - lsb) & 31), (width - 1));
  }
  inline void ubfiz(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    ubfm(Rd, Rn, ((64 - lsb) & 63), (width - 1));
  }

  inline void ubfxw(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    ubfmw(Rd, Rn, lsb, (lsb + width - 1));
  }
  inline void ubfx(Register Rd, Register Rn, unsigned lsb, unsigned width) {
    ubfm(Rd, Rn, lsb , (lsb + width - 1));
  }

  inline void asrw(Register Rd, Register Rn, unsigned imm) {
    sbfmw(Rd, Rn, imm, 31);
  }

  inline void asr(Register Rd, Register Rn, unsigned imm) {
    sbfm(Rd, Rn, imm, 63);
  }

  inline void lslw(Register Rd, Register Rn, unsigned imm) {
    ubfmw(Rd, Rn, ((32 - imm) & 31), (31 - imm));
  }

  inline void lsl(Register Rd, Register Rn, unsigned imm) {
    ubfm(Rd, Rn, ((64 - imm) & 63), (63 - imm));
  }

  inline void lsrw(Register Rd, Register Rn, unsigned imm) {
    ubfmw(Rd, Rn, imm, 31);
  }

  inline void lsr(Register Rd, Register Rn, unsigned imm) {
    ubfm(Rd, Rn, imm, 63);
  }

  inline void rorw(Register Rd, Register Rn, unsigned imm) {
    extrw(Rd, Rn, Rn, imm);
  }

  inline void ror(Register Rd, Register Rn, unsigned imm) {
    extr(Rd, Rn, Rn, imm);
  }

  inline void sxtbw(Register Rd, Register Rn) {
    sbfmw(Rd, Rn, 0, 7);
  }
  inline void sxthw(Register Rd, Register Rn) {
    sbfmw(Rd, Rn, 0, 15);
  }
  inline void sxtb(Register Rd, Register Rn) {
    sbfm(Rd, Rn, 0, 7);
  }
  inline void sxth(Register Rd, Register Rn) {
    sbfm(Rd, Rn, 0, 15);
  }
  inline void sxtw(Register Rd, Register Rn) {
    sbfm(Rd, Rn, 0, 31);
  }

  inline void uxtbw(Register Rd, Register Rn) {
    ubfmw(Rd, Rn, 0, 7);
  }
  inline void uxthw(Register Rd, Register Rn) {
    ubfmw(Rd, Rn, 0, 15);
  }
  inline void uxtb(Register Rd, Register Rn) {
    ubfm(Rd, Rn, 0, 7);
  }
  inline void uxth(Register Rd, Register Rn) {
    ubfm(Rd, Rn, 0, 15);
  }
  inline void uxtw(Register Rd, Register Rn) {
    ubfm(Rd, Rn, 0, 31);
  }

  inline void cmnw(Register Rn, Register Rm) {
    addsw(zr, Rn, Rm);
  }
  inline void cmn(Register Rn, Register Rm) {
    adds(zr, Rn, Rm);
  }

  inline void cmpw(Register Rn, Register Rm) {
    subsw(zr, Rn, Rm);
  }
  inline void cmp(Register Rn, Register Rm) {
    subs(zr, Rn, Rm);
  }

  inline void negw(Register Rd, Register Rn) {
    subw(Rd, zr, Rn);
  }

  inline void neg(Register Rd, Register Rn) {
    sub(Rd, zr, Rn);
  }

  inline void negsw(Register Rd, Register Rn) {
    subsw(Rd, zr, Rn);
  }

  inline void negs(Register Rd, Register Rn) {
    subs(Rd, zr, Rn);
  }

  inline void cmnw(Register Rn, Register Rm, enum shift_kind kind, unsigned shift = 0) {
    addsw(zr, Rn, Rm, kind, shift);
  }
  inline void cmn(Register Rn, Register Rm, enum shift_kind kind, unsigned shift = 0) {
    adds(zr, Rn, Rm, kind, shift);
  }

  inline void cmpw(Register Rn, Register Rm, enum shift_kind kind, unsigned shift = 0) {
    subsw(zr, Rn, Rm, kind, shift);
  }
  inline void cmp(Register Rn, Register Rm, enum shift_kind kind, unsigned shift = 0) {
    subs(zr, Rn, Rm, kind, shift);
  }

  inline void negw(Register Rd, Register Rn, enum shift_kind kind, unsigned shift = 0) {
    subw(Rd, zr, Rn, kind, shift);
  }

  inline void neg(Register Rd, Register Rn, enum shift_kind kind, unsigned shift = 0) {
    sub(Rd, zr, Rn, kind, shift);
  }

  inline void negsw(Register Rd, Register Rn, enum shift_kind kind, unsigned shift = 0) {
    subsw(Rd, zr, Rn, kind, shift);
  }

  inline void negs(Register Rd, Register Rn, enum shift_kind kind, unsigned shift = 0) {
    subs(Rd, zr, Rn, kind, shift);
  }

  inline void mnegw(Register Rd, Register Rn, Register Rm) {
    msubw(Rd, Rn, Rm, zr);
  }
  inline void mneg(Register Rd, Register Rn, Register Rm) {
    msub(Rd, Rn, Rm, zr);
  }

  inline void mulw(Register Rd, Register Rn, Register Rm) {
    maddw(Rd, Rn, Rm, zr);
  }
  inline void mul(Register Rd, Register Rn, Register Rm) {
    madd(Rd, Rn, Rm, zr);
  }

  inline void smnegl(Register Rd, Register Rn, Register Rm) {
    smsubl(Rd, Rn, Rm, zr);
  }
  inline void smull(Register Rd, Register Rn, Register Rm) {
    smaddl(Rd, Rn, Rm, zr);
  }

  inline void umnegl(Register Rd, Register Rn, Register Rm) {
    umsubl(Rd, Rn, Rm, zr);
  }
  inline void umull(Register Rd, Register Rn, Register Rm) {
    umaddl(Rd, Rn, Rm, zr);
  }

  // macro assembly operations needed for aarch64

  // first two private routines for loading 32 bit or 64 bit constants
private:

  void mov_immediate64(Register dst, u_int64_t imm64);
  void mov_immediate32(Register dst, u_int32_t imm32);

  // now mov instructions for loading absolute addresses and 32 or
  // 64 bit integers
public:

  inline void mov(Register dst, address addr)
  {
    mov_immediate64(dst, (u_int64_t)addr);
  }

  inline void mov(Register dst, u_int64_t imm64)
  {
    mov_immediate64(dst, imm64);
  }

  inline void movw(Register dst, u_int32_t imm32)
  {
    mov_immediate32(dst, imm32);
  }

  inline void mov(Register dst, long l)
  {
    mov(dst, (u_int64_t)l);
  }

  inline void mov(Register dst, int i)
  {
    mov(dst, (long)i);
  }

  void mov(Register dst, Address a);
  void mov64(Register r, uintptr_t imm64);

  // macro instructions for accessing and updating floating point
  // status register
  //
  // FPSR : op1 == 011
  //        CRn == 0100
  //        CRm == 0100
  //        op2 == 001

  inline void get_fpsr(Register reg)
  {
    mrs(0b11, 0b0100, 0b0100, 0b001, reg);
  }

  inline void set_fpsr(Register reg)
  {
    msr(0b011, 0b0100, 0b0100, 0b001, reg);
  }

  inline void clear_fpsr()
  {
    msr(0b011, 0b0100, 0b0100, 0b001, zr);
  }

  // idiv variant which deals with MINLONG as dividend and -1 as divisor
  int corrected_idivl(Register result, Register ra, Register rb,
		      bool want_remainder, Register tmp = rscratch1);
  int corrected_idivq(Register result, Register ra, Register rb,
		      bool want_remainder, Register tmp = rscratch1);

  // Support for NULL-checks
  //
  // Generates code that causes a NULL OS exception if the content of reg is NULL.
  // If the accessed location is M[reg + offset] and the offset is known, provide the
  // offset. No explicit code generation is needed if the offset is within a certain
  // range (0 <= offset <= page_size).

  virtual void null_check(Register reg, int offset = -1);
  static bool needs_explicit_null_check(intptr_t offset);

  static address target_addr_for_insn(address insn_addr, unsigned insn);

  // Required platform-specific helpers for Label::patch_instructions.
  // They _shadow_ the declarations in AbstractAssembler, which are undefined.
  static void pd_patch_instruction(address branch, address target);
  static address pd_call_destination(address branch) {
    unsigned insn = *(unsigned*)branch;
    return target_addr_for_insn(branch, insn);
  }
#ifndef PRODUCT
  static void pd_print_patched_instruction(address branch);
#endif

  // The following 4 methods return the offset of the appropriate move instruction

  // Support for fast byte/short loading with zero extension (depending on particular CPU)
  int load_unsigned_byte(Register dst, Address src);
  int load_unsigned_short(Register dst, Address src);

  // Support for fast byte/short loading with sign extension (depending on particular CPU)
  int load_signed_byte(Register dst, Address src);
  int load_signed_short(Register dst, Address src);

  int load_signed_byte32(Register dst, Address src);
  int load_signed_short32(Register dst, Address src);

  // Support for sign-extension (hi:lo = extend_sign(lo))
  void extend_sign(Register hi, Register lo);

  // Load and store values by size and signed-ness
  void load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2 = noreg);
  void store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2 = noreg);

  // Support for inc/dec with optimal instruction selection depending on value

  // x86_64 aliases an unqualified register/address increment and
  // decrement to call incrementq and decrementq but also supports
  // explicitly sized calls to incrementq/decrementq or
  // incrementl/decrementl

  // for aarch64 the proper convention would be to use
  // increment/decrement for 64 bit operatons and
  // incrementw/decrementw for 32 bit operations. so when porting
  // x86_64 code we can leave calls to increment/decrement as is,
  // replace incrementq/decrementq with increment/decrement and
  // replace incrementl/decrementl with incrementw/decrementw.

  // n.b. increment/decrement calls with an Address destination will
  // need to use a scratch register to load the value to be
  // incremented. increment/decrement calls which add or subtract a
  // constant value greater than 2^12 will need to use a 2nd scratch
  // register to hold the constant. so, a register increment/decrement
  // may trash rscratch2 and an address increment/decrement trash
  // rscratch and rscratch2

  void decrementw(Address dst, int value = 1);
  void decrementw(Register reg, int value = 1);

  void decrement(Register reg, int value = 1);
  void decrement(Address dst, int value = 1);

  void incrementw(Address dst, int value = 1);
  void incrementw(Register reg, int value = 1);

  void increment(Register reg, int value = 1);
  void increment(Address dst, int value = 1);


  // Alignment
  void align(int modulus);

  // A 5 byte nop that is safe for patching (see patch_verified_entry)
  void fat_nop();

  // Stack frame creation/removal
  void enter();
  void leave();

  // debug only support for spilling and restoring/checking callee
  // save registers around a Java method call

#ifdef ASSERT
  void spill(Register rscratcha, Register rscratchb);
  void spillcheck(Register rscratcha, Register rscratchb);
#endif // ASSERT

  // Support for getting the JavaThread pointer (i.e.; a reference to thread-local information)
  // The pointer will be loaded into the thread register.
  void get_thread(Register thread);


  // Support for VM calls
  //
  // It is imperative that all calls into the VM are handled via the call_VM macros.
  // They make sure that the stack linkage is setup correctly. call_VM's correspond
  // to ENTRY/ENTRY_X entry points while call_VM_leaf's correspond to LEAF entry points.


  void call_VM(Register oop_result,
               address entry_point,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1, Register arg_2,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               address entry_point,
               Register arg_1, Register arg_2, Register arg_3,
               bool check_exceptions = true);

  // Overloadings with last_Java_sp
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               int number_of_arguments = 0,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, bool
               check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, Register arg_2,
               bool check_exceptions = true);
  void call_VM(Register oop_result,
               Register last_java_sp,
               address entry_point,
               Register arg_1, Register arg_2, Register arg_3,
               bool check_exceptions = true);

  void get_vm_result  (Register oop_result, Register thread);
  void get_vm_result_2(Register metadata_result, Register thread);

  // These always tightly bind to MacroAssembler::call_VM_base
  // bypassing the virtual implementation
  void super_call_VM(Register oop_result, Register last_java_sp, address entry_point, int number_of_arguments = 0, bool check_exceptions = true);
  void super_call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, bool check_exceptions = true);
  void super_call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, Register arg_2, bool check_exceptions = true);
  void super_call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, Register arg_2, Register arg_3, bool check_exceptions = true);
  void super_call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, Register arg_2, Register arg_3, Register arg_4, bool check_exceptions = true);

  void call_VM_leaf(address entry_point,
                    int number_of_arguments = 0);
  void call_VM_leaf(address entry_point,
                    Register arg_1);
  void call_VM_leaf(address entry_point,
                    Register arg_1, Register arg_2);
  void call_VM_leaf(address entry_point,
                    Register arg_1, Register arg_2, Register arg_3);

  // These always tightly bind to MacroAssembler::call_VM_leaf_base
  // bypassing the virtual implementation
  void super_call_VM_leaf(address entry_point);
  void super_call_VM_leaf(address entry_point, Register arg_1);
  void super_call_VM_leaf(address entry_point, Register arg_1, Register arg_2);
  void super_call_VM_leaf(address entry_point, Register arg_1, Register arg_2, Register arg_3);
  void super_call_VM_leaf(address entry_point, Register arg_1, Register arg_2, Register arg_3, Register arg_4);

  // last Java Frame (fills frame anchor)
  void set_last_Java_frame(Register last_java_sp,
                           Register last_java_fp,
                           address last_java_pc,
			   Register scratch);

  void set_last_Java_frame(Register last_java_sp,
                           Register last_java_fp,
                           Label &last_java_pc,
			   Register scratch);

  void set_last_Java_frame(Register last_java_sp,
                           Register last_java_fp,
                           Register last_java_pc,
			   Register scratch);

  void reset_last_Java_frame(Register thread, bool clearfp, bool clear_pc);

  // thread in the default location (r15_thread on 64bit)
  void reset_last_Java_frame(bool clear_fp, bool clear_pc);

  // Stores
  void store_check(Register obj);                // store check for obj - register is destroyed afterwards
  void store_check(Register obj, Address dst);   // same as above, dst is exact store location (reg. is destroyed)

#if INCLUDE_ALL_GCS

  void g1_write_barrier_pre(Register obj,
                            Register pre_val,
                            Register thread,
                            Register tmp,
                            bool tosca_live,
                            bool expand_call);

  void g1_write_barrier_post(Register store_addr,
                             Register new_val,
                             Register thread,
                             Register tmp,
                             Register tmp2);

#endif // INCLUDE_ALL_GCS

  // split store_check(Register obj) to enhance instruction interleaving
  void store_check_part_1(Register obj);
  void store_check_part_2(Register obj);

  // currently unimplemented
#if 0
  // C 'boolean' to Java boolean: x == 0 ? 0 : 1
  void c2bool(Register x);

  // C++ bool manipulation

  void movbool(Register dst, Address src);
  void movbool(Address dst, bool boolconst);
  void movbool(Address dst, Register src);
  void testbool(Register dst);
#endif

  // oop manipulations
  void load_klass(Register dst, Register src);
  void store_klass(Register dst, Register src);

  void load_heap_oop(Register dst, Address src);

  void load_heap_oop_not_null(Register dst, Address src);
  void store_heap_oop(Address dst, Register src);

  // currently unimplemented
  // Used for storing NULL. All other oop constants should be
  // stored using routines that take a jobject.
  void store_heap_oop_null(Address dst);

  // currently unimplemented
#if 0
  void load_prototype_header(Register dst, Register src);
#endif

  void store_klass_gap(Register dst, Register src);

  // This dummy is to prevent a call to store_heap_oop from
  // converting a zero (like NULL) into a Register by giving
  // the compiler two choices it can't resolve

  void store_heap_oop(Address dst, void* dummy);

  void encode_heap_oop(Register d, Register s);
  void encode_heap_oop(Register r) { encode_heap_oop(r, r); }
  void decode_heap_oop(Register r);
  void encode_heap_oop_not_null(Register r);
  void decode_heap_oop_not_null(Register r);
  void encode_heap_oop_not_null(Register dst, Register src);
  void decode_heap_oop_not_null(Register dst, Register src);

  void set_narrow_oop(Register dst, jobject obj);
  // currently unimplemented
#if 0
  void set_narrow_oop(Address dst, jobject obj);
  void cmp_narrow_oop(Register dst, jobject obj);
  void cmp_narrow_oop(Address dst, jobject obj);
#endif

  void encode_klass_not_null(Register r);
  void decode_klass_not_null(Register r);
  void encode_klass_not_null(Register dst, Register src);
  void decode_klass_not_null(Register dst, Register src);

  void set_narrow_klass(Register dst, Klass* k);
  // currently unimplemented
#if 0
  void set_narrow_klass(Address dst, Klass* k);
  void cmp_narrow_klass(Register dst, Klass* k);
  void cmp_narrow_klass(Address dst, Klass* k);
#endif

  // if heap base register is used - reinit it with the correct value
  void reinit_heapbase();

  DEBUG_ONLY(void verify_heapbase(const char* msg);)

  // currently unimplemented
#if 0
  void int3();
#endif

  // currently unimplemented
#if 0
  // Long operation macros for a 32bit cpu
  // Long negation for Java
  void lneg(Register hi, Register lo);

  // Long multiplication for Java
  // (destroys contents of eax, ebx, ecx and edx)
  void lmul(int x_rsp_offset, int y_rsp_offset); // rdx:rax = x * y

  // Long shifts for Java
  // (semantics as described in JVM spec.)
  void lshl(Register hi, Register lo);                               // hi:lo << (rcx & 0x3f)
  void lshr(Register hi, Register lo, bool sign_extension = false);  // hi:lo >> (rcx & 0x3f)

  // Long compare for Java
  // (semantics as described in JVM spec.)
  void lcmp2int(Register x_hi, Register x_lo, Register y_hi, Register y_lo); // x_hi = lcmp(x, y)


  // misc

  // Sign extension
  void sign_extend_short(Register reg);
  void sign_extend_byte(Register reg);

  // Division by power of 2, rounding towards 0
  void division_with_shift(Register reg, int shift_value);
#endif

  // unimpelements
#if 0
  // Compares the top-most stack entries on the FPU stack and sets the eflags as follows:
  //
  // CF (corresponds to C0) if x < y
  // PF (corresponds to C2) if unordered
  // ZF (corresponds to C3) if x = y
  //
  // The arguments are in reversed order on the stack (i.e., top of stack is first argument).
  // tmp is a temporary register, if none is available use noreg (only matters for non-P6 code)
  void fcmp(Register tmp);
  // Variant of the above which allows y to be further down the stack
  // and which only pops x and y if specified. If pop_right is
  // specified then pop_left must also be specified.
  void fcmp(Register tmp, int index, bool pop_left, bool pop_right);

  // Floating-point comparison for Java
  // Compares the top-most stack entries on the FPU stack and stores the result in dst.
  // The arguments are in reversed order on the stack (i.e., top of stack is first argument).
  // (semantics as described in JVM spec.)
  void fcmp2int(Register dst, bool unordered_is_less);
  // Variant of the above which allows y to be further down the stack
  // and which only pops x and y if specified. If pop_right is
  // specified then pop_left must also be specified.
  void fcmp2int(Register dst, bool unordered_is_less, int index, bool pop_left, bool pop_right);

  // Floating-point remainder for Java (ST0 = ST0 fremr ST1, ST1 is empty afterwards)
  // tmp is a temporary register, if none is available use noreg
  void fremr(Register tmp);


  // Inlined sin/cos generator for Java; must not use CPU instruction
  // directly on Intel as it does not have high enough precision
  // outside of the range [-pi/4, pi/4]. Extra argument indicate the
  // number of FPU stack slots in use; all but the topmost will
  // require saving if a slow case is necessary. Assumes argument is
  // on FP TOS; result is on FP TOS.  No cpu registers are changed by
  // this code.
  void trigfunc(char trig, int num_fpu_regs_in_use = 1);

  // branch to L if FPU flag C2 is set/not set
  // tmp is a temporary register, if none is available use noreg
  void jC2 (Register tmp, Label& L);
  void jnC2(Register tmp, Label& L);

  void push_IU_state();
  void pop_IU_state();

  void push_FPU_state();
  void pop_FPU_state();
#endif

  void push_CPU_state();
  void pop_CPU_state() ;

  // Round up to a power of two
  void round_to(Register reg, int modulus);

  // unimplemented
#if 0
  // Callee saved registers handling
  void push_callee_saved_registers();
  void pop_callee_saved_registers();
#endif

  // unimplemented

  // allocation
  void eden_allocate(
    Register obj,                      // result: pointer to object after successful allocation
    Register var_size_in_bytes,        // object size in bytes if unknown at compile time; invalid otherwise
    int      con_size_in_bytes,        // object size in bytes if   known at compile time
    Register t1,                       // temp register
    Label&   slow_case                 // continuation point if fast allocation fails
  );
  void tlab_allocate(
    Register obj,                      // result: pointer to object after successful allocation
    Register var_size_in_bytes,        // object size in bytes if unknown at compile time; invalid otherwise
    int      con_size_in_bytes,        // object size in bytes if   known at compile time
    Register t1,                       // temp register
    Register t2,                       // temp register
    Label&   slow_case                 // continuation point if fast allocation fails
  );
  Register tlab_refill(Label& retry_tlab, Label& try_eden, Label& slow_case); // returns TLS address
  void verify_tlab();

  void incr_allocated_bytes(Register thread,
                            Register var_size_in_bytes, int con_size_in_bytes,
                            Register t1 = noreg);

  // interface method calling
  void lookup_interface_method(Register recv_klass,
                               Register intf_klass,
                               RegisterOrConstant itable_index,
                               Register method_result,
                               Register scan_temp,
                               Label& no_such_interface);

  // virtual method calling
  // n.b. x86 allows RegisterOrConstant for vtable_index
  void lookup_virtual_method(Register recv_klass,
                             RegisterOrConstant vtable_index,
                             Register method_result);

  // Test sub_klass against super_klass, with fast and slow paths.

  // The fast path produces a tri-state answer: yes / no / maybe-slow.
  // One of the three labels can be NULL, meaning take the fall-through.
  // If super_check_offset is -1, the value is loaded up from super_klass.
  // No registers are killed, except temp_reg.
  void check_klass_subtype_fast_path(Register sub_klass,
                                     Register super_klass,
                                     Register temp_reg,
                                     Label* L_success,
                                     Label* L_failure,
                                     Label* L_slow_path,
                RegisterOrConstant super_check_offset = RegisterOrConstant(-1));

  // The rest of the type check; must be wired to a corresponding fast path.
  // It does not repeat the fast path logic, so don't use it standalone.
  // The temp_reg and temp2_reg can be noreg, if no temps are available.
  // Updates the sub's secondary super cache as necessary.
  // If set_cond_codes, condition codes will be Z on success, NZ on failure.
  void check_klass_subtype_slow_path(Register sub_klass,
                                     Register super_klass,
                                     Register temp_reg,
                                     Register temp2_reg,
                                     Label* L_success,
                                     Label* L_failure,
                                     bool set_cond_codes = false);

  // Simplified, combined version, good for typical uses.
  // Falls through on failure.
  void check_klass_subtype(Register sub_klass,
                           Register super_klass,
                           Register temp_reg,
                           Label& L_success);

  // unimplemented
#if 0
  // method handles (JSR 292)
  void check_method_handle_type(Register mtype_reg, Register mh_reg,
                                Register temp_reg,
                                Label& wrong_method_type);
  void load_method_handle_vmslots(Register vmslots_reg, Register mh_reg,
                                  Register temp_reg);
  void jump_to_method_handle_entry(Register mh_reg, Register temp_reg);
#endif
  Address argument_address(RegisterOrConstant arg_slot, int extra_slot_offset = 0);


  //----
#if 0
  // method handles (JSR 292)
  void set_word_if_not_zero(Register reg); // sets reg to 1 if not zero, otherwise 0
#endif

  // Debugging

  // only if +VerifyOops
  void verify_oop(Register reg, const char* s = "broken oop");
  void verify_oop_addr(Address addr, const char * s = "broken oop addr");

// TODO: verify method and klass metadata (compare against vptr?)
  void _verify_method_ptr(Register reg, const char * msg, const char * file, int line) {}
  void _verify_klass_ptr(Register reg, const char * msg, const char * file, int line){}

#define verify_method_ptr(reg) _verify_method_ptr(reg, "broken method " #reg, __FILE__, __LINE__)
#define verify_klass_ptr(reg) _verify_klass_ptr(reg, "broken klass " #reg, __FILE__, __LINE__)

  // only if +VerifyFPU
  void verify_FPU(int stack_depth, const char* s = "illegal FPU state");

  // prints msg, dumps registers and stops execution
  void stop(const char* msg);

  // prints msg and continues
  void warn(const char* msg);

  static void debug64(char* msg, int64_t pc, int64_t regs[]);

  // unimplemented
#if 0
  void os_breakpoint();
#endif

  void untested()                                { stop("untested"); }

  void unimplemented(const char* what = "")      { char* b = new char[1024];  jio_snprintf(b, 1024, "unimplemented: %s", what);  stop(b); }

  void should_not_reach_here()                   { stop("should not reach here"); }

  // unimplemented
#if 0
  void print_CPU_state();
#endif

  // Stack overflow checking
  void bang_stack_with_offset(int offset) {
    // stack grows down, caller passes positive offset
    assert(offset > 0, "must bang with negative offset");
    mov(rscratch2, -offset);
    ldr(zr, Address(sp, rscratch2));
  }

  // Writes to stack successive pages until offset reached to check for
  // stack overflow + shadow pages.  Also, clobbers tmp
  void bang_stack_size(Register size, Register tmp);

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset);

  // Support for serializing memory accesses between threads
  void serialize_memory(Register thread, Register tmp);

  // unimplemented
#if 0
  void verify_tlab();

  // Biased locking support
  // lock_reg and obj_reg must be loaded up with the appropriate values.
  // swap_reg must be rax, and is killed.
  // tmp_reg is optional. If it is supplied (i.e., != noreg) it will
  // be killed; if not supplied, push/pop will be used internally to
  // allocate a temporary (inefficient, avoid if possible).
  // Optional slow case is for implementations (interpreter and C1) which branch to
  // slow case directly. Leaves condition codes set for C2's Fast_Lock node.
  // Returns offset of first potentially-faulting instruction for null
  // check info (currently consumed only by C1). If
  // swap_reg_contains_mark is true then returns -1 as it is assumed
  // the calling code has already passed any potential faults.
  int biased_locking_enter(Register lock_reg, Register obj_reg,
                           Register swap_reg, Register tmp_reg,
                           bool swap_reg_contains_mark,
                           Label& done, Label* slow_case = NULL,
                           BiasedLockingCounters* counters = NULL);
  void biased_locking_exit (Register obj_reg, Register temp_reg, Label& done);


  Condition negate_condition(Condition cond);
#endif

  // Arithmetics

  void addptr(Address dst, int32_t src) {
    ldr(rscratch1, dst);
    add(rscratch1, rscratch1, src);
    str(rscratch1, dst);
}
  // unimplemented
#if 0
  void addptr(Address dst, Register src);
#endif

  void addptr(Register dst, Address src) { Unimplemented(); }
  // unimplemented
#if 0
  void addptr(Register dst, int32_t src);
  void addptr(Register dst, Register src);
#endif
  void addptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

  // unimplemented
#if 0
  void andptr(Register dst, int32_t src);
#endif
  void andptr(Register src1, Register src2) { Unimplemented(); }

  // unimplemented
#if 0
  // renamed to drag out the casting of address to int32_t/intptr_t
  void cmp32(Register src1, int32_t imm);

  void cmp32(Register src1, Address src2);
#endif

  void cmpptr(Register src1, Register src2) { Unimplemented(); }
  void cmpptr(Register src1, Address src2);
  // void cmpptr(Address src1, Register src2) { Unimplemented(); }

  void cmpptr(Register src1, int32_t src2) { Unimplemented(); }
  void cmpptr(Address src1, int32_t src2) { Unimplemented(); }

  void cmpxchgptr(Register oldv, Register newv, Register addr, Register tmp,
		  Label &suceed, Label &fail);

  void cmpxchgw(Register oldv, Register newv, Register addr, Register tmp,
		  Label &suceed, Label &fail);

  void imulptr(Register dst, Register src) { Unimplemented(); }


  void negptr(Register dst) { Unimplemented(); }

  void notptr(Register dst) { Unimplemented(); }

  // unimplemented
#if 0
  void shlptr(Register dst, int32_t shift);
#endif
  void shlptr(Register dst) { Unimplemented(); }

  // unimplemented
#if 0
  void shrptr(Register dst, int32_t shift);
#endif
  void shrptr(Register dst) { Unimplemented(); }

  void sarptr(Register dst) { Unimplemented(); }
  void sarptr(Register dst, int32_t src) { Unimplemented(); }

  void subptr(Address dst, int32_t src) { Unimplemented(); }

  void subptr(Register dst, Address src) { Unimplemented(); }
  // unimplemented
#if 0
  void subptr(Register dst, int32_t src);
  // Force generation of a 4 byte immediate value even if it fits into 8bit
  void subptr_imm32(Register dst, int32_t src);
  void subptr(Register dst, Register src);
#endif
  void subptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

  void sbbptr(Address dst, int32_t src) { Unimplemented(); }
  void sbbptr(Register dst, int32_t src) { Unimplemented(); }

  void xchgptr(Register src1, Register src2) { Unimplemented(); }
  void xchgptr(Register src1, Address src2) { Unimplemented(); }

  void xaddptr(Address src1, Register src2) { Unimplemented(); }



  // unimplemented
#if 0

  // Perhaps we should implement this one
  void lea(Register dst, Address adr) { Unimplemented(); }

  void leal32(Register dst, Address src) { Unimplemented(); }

  void orptr(Register dst, Address src) { Unimplemented(); }
  void orptr(Register dst, Register src) { Unimplemented(); }
  void orptr(Register dst, int32_t src) { Unimplemented(); }

  void testptr(Register src, int32_t imm32) {  Unimplemented(); }
  void testptr(Register src1, Register src2);

  void xorptr(Register dst, Register src) { Unimplemented(); }
  void xorptr(Register dst, Address src) { Unimplemented(); }
#endif

  // Calls

  // void call(Label& L, relocInfo::relocType rtype);

  // NOTE: this call tranfers to the effective address of entry NOT
  // the address contained by entry. This is because this is more natural
  // for jumps/calls.
  void call(Address entry);

  // Emit the CompiledIC call idiom
  void ic_call(address entry);

  // Jumps

  // unimplemented
#if 0
  // NOTE: these jumps tranfer to the effective address of dst NOT
  // the address contained by dst. This is because this is more natural
  // for jumps/calls.
  void jump(Address dst);
  void jump_cc(Condition cc, Address dst);
#endif

  // Floating

  void fadd_s(Address src)        { Unimplemented(); }

  void fldcw(Address src) { Unimplemented(); }

  void fld_s(int index)   { Unimplemented(); }
  void fld_s(Address src) { Unimplemented(); }

  void fld_d(Address src) { Unimplemented(); }

  void fld_x(Address src) { Unimplemented(); }

  void fmul_s(Address src)        { Unimplemented(); }

  // unimplemented
#if 0
  // compute pow(x,y) and exp(x) with x86 instructions. Don't cover
  // all corner cases and may result in NaN and require fallback to a
  // runtime call.
  void fast_pow();
  void fast_exp();
#endif

  // computes exp(x). Fallback to runtime call included.
  void exp_with_fallback(int num_fpu_regs_in_use) { Unimplemented(); }
  // computes pow(x,y). Fallback to runtime call included.
  void pow_with_fallback(int num_fpu_regs_in_use) { Unimplemented(); }

public:

  // Data

  void mov_metadata(Register dst, Metadata* obj);
  Address allocate_metadata_address(Metadata* obj);
  Address constant_oop_address(jobject obj);
  // unimplemented
#if 0
  void pushoop(jobject obj);
#endif

  void movoop(Register dst, jobject obj);

  // sign extend as need a l to ptr sized element
  void movl2ptr(Register dst, Address src) { Unimplemented(); }
  void movl2ptr(Register dst, Register src) { Unimplemented(); }

  // unimplemented
#if 0
  // C2 compiled method's prolog code.
  void verified_entry(int framesize, bool stack_bang, bool fp_mode_24b);
#endif

#undef VIRTUAL

  // Stack push and pop individual 64 bit registers
  void push(Register src);
  void pop(Register dst);

  // push all registers onto the stack
  void pusha();
  void popa();

  int push(unsigned int bitset, Register stack);
  int pop(unsigned int bitset, Register stack);

  void repne_scan(Register addr, Register value, Register count,
		  Register scratch);
  void repne_scanw(Register addr, Register value, Register count,
		   Register scratch);

  typedef void (MacroAssembler::* add_sub_imm_insn)(Register Rd, Register Rn, unsigned imm);
  typedef void (MacroAssembler::* add_sub_reg_insn)(Register Rd, Register Rn, Register Rm, enum shift_kind kind, unsigned shift);

  // If a constant does not fit in an immediate field, generate some
  // number of MOV instructions and then perform the operation
  void wrap_add_sub_imm_insn(Register Rd, Register Rn, unsigned imm,
			     add_sub_imm_insn insn1,
			     add_sub_reg_insn insn2);
  // Seperate vsn which sets the flags
  void wrap_adds_subs_imm_insn(Register Rd, Register Rn, unsigned imm,
			     add_sub_imm_insn insn1,
			     add_sub_reg_insn insn2);

#define WRAP(INSN)							\
  void INSN(Register Rd, Register Rn, unsigned imm) {			\
    wrap_add_sub_imm_insn(Rd, Rn, imm, &Assembler::INSN, &Assembler::INSN); \
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm,			\
	     enum shift_kind kind, unsigned shift = 0) {		\
    Assembler::INSN(Rd, Rn, Rm, kind, shift);				\
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm) {			\
    Assembler::INSN(Rd, Rn, Rm);					\
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm,			\
           ext::operation option, int amount = 0) {			\
    Assembler::INSN(Rd, Rn, Rm, option, amount);			\
  }

  WRAP(add) WRAP(addw) WRAP(sub) WRAP(subw)

#undef WRAP
#define WRAP(INSN)							\
  void INSN(Register Rd, Register Rn, unsigned imm) {			\
    wrap_adds_subs_imm_insn(Rd, Rn, imm, &Assembler::INSN, &Assembler::INSN); \
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm,			\
	     enum shift_kind kind, unsigned shift = 0) {		\
    Assembler::INSN(Rd, Rn, Rm, kind, shift);				\
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm) {			\
    Assembler::INSN(Rd, Rn, Rm);					\
  }									\
									\
  void INSN(Register Rd, Register Rn, Register Rm,			\
           ext::operation option, int amount = 0) {			\
    Assembler::INSN(Rd, Rn, Rm, option, amount);			\
  }

  WRAP(adds) WRAP(addsw) WRAP(subs) WRAP(subsw)

  void add(Register Rd, Register Rn, RegisterOrConstant increment);
  void addw(Register Rd, Register Rn, RegisterOrConstant increment);

  void adrp(Register reg1, const Address &dest, unsigned long &byte_offset);

  void tableswitch(Register index, jint lowbound, jint highbound,
		   Label &jumptable, Label &jumptable_end, int stride = 1) {
    adr(rscratch1, jumptable);
    subsw(rscratch2, index, lowbound);
    subsw(zr, rscratch2, highbound - lowbound);
    br(Assembler::HS, jumptable_end);
    add(rscratch1, rscratch1, rscratch2,
	ext::sxtw, exact_log2(stride * Assembler::instruction_size));
    br(rscratch1);
  }

  // Form an address from base + (offset << shift) in Rd.  Rd may or
  // may not actually be used: you must use the Address that is
  // returned.  It is up to you to ensure that the shift provided
  // matches the size of your data.
  Address form_address(Register Rd, Register base, long byte_offset, int shift);

  // Prolog generator routines to support switch between x86 code and
  // generated ARM code

  // routine to generate an x86 prolog for a stub function which
  // bootstraps into the generated ARM code which directly follows the
  // stub
  //

  public:
  // enum used for aarch64--x86 linkage to define return type of x86 function
  enum ret_type { ret_type_void, ret_type_integral, ret_type_float, ret_type_double};

#ifdef BUILTIN_SIM
  void c_stub_prolog(int gp_arg_count, int fp_arg_count, int ret_type);
#endif

  // special version of call_VM_leaf_base needed for aarch64 simulator
  // where we need to specify both the gp and fp arg counts and the
  // return type so that the linkage routine from aarch64 to x86 and
  // back knows which aarch64 registers to copy to x86 registers and
  // which x86 result register to copy back to an aarch64 register

  void call_VM_leaf_base1(
    address  entry_point,             // the entry point
    int      number_of_gp_arguments,  // the number of gp reg arguments to pass
    int      number_of_fp_arguments,  // the number of fp reg arguments to pass
    ret_type type,		      // the return type for the call
    Label*   retaddr = NULL
  );

  enum Membar_mask_bits {
    StoreStore = 1 << 3,
    LoadStore  = 1 << 2,
    StoreLoad  = 1 << 1,
    LoadLoad   = 1 << 0
  };

  void membar(Membar_mask_bits order_constraint) {
    // LD     Load-Load, Load-Store
    // ST     Store-Store
    // SY     Any-Any

    // Handle simple cases first
    if (order_constraint == StoreStore) {
      dsb(ST);
    } else if (order_constraint == LoadLoad
	|| order_constraint == LoadStore
	|| order_constraint == (LoadLoad | LoadStore)) {
      dsb(LD);
    } else {
      dsb(SY);
    }
  }

  void ldr_constant(Register dest, address const_addr) {
    guarantee(const_addr, "constant pool overflow");
    if (NearCpool) {
      ldr(dest, const_addr, relocInfo::internal_word_type);
    } else {
      unsigned long offset;
      adrp(dest, InternalAddress(const_addr), offset);
      ldr(dest, Address(dest, offset));
    }
  }

  address read_polling_page(Register r, address page, relocInfo::relocType rtype);
  address read_polling_page(Register r, relocInfo::relocType rtype);

  typedef void (Assembler::* flush_insn)(Register Rt);
  void generate_flush_loop(flush_insn flush, Register start, Register lines);

};

#ifdef ASSERT
inline bool AbstractAssembler::pd_check_instruction_mark() { return false; }
#endif

/**
 * class SkipIfEqual:
 *
 * Instantiating this class will result in assembly code being output that will
 * jump around any code emitted between the creation of the instance and it's
 * automatic destruction at the end of a scope block, depending on the value of
 * the flag passed to the constructor, which will be checked at run-time.
 */
class SkipIfEqual {
 private:
  MacroAssembler* _masm;
  Label _label;

 public:
   SkipIfEqual(MacroAssembler*, const bool* flag_addr, bool value);
   ~SkipIfEqual();
};

struct tableswitch {
  Register _reg;
  int _insn_index; jint _first_key; jint _last_key;
  Label _after;
  Label _branches;
};

#endif // CPU_AARCH64_VM_MACROASSEMBLER_AARCH64_HPP
