/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_AARCH64_VM_ASSEMBLER_AARCH64_HPP
#define CPU_AARCH64_VM_ASSEMBLER_AARCH64_HPP

// definitions of various symbolic names for machine registers

// First intercalls between C and Java which use 8 general registers
// and 8 floating registers

// we also have to copy between x86 and ARM registers but that's a
// secondary complication -- not all code employing C call convention
// executes as x86 code though -- we generate some of it

class Argument VALUE_OBJ_CLASS_SPEC {
 public:
  enum {
    n_int_register_parameters_c   = 8,  // r0, r1, ... r7 (c_rarg0, c_rarg1, ...)
    n_float_register_parameters_c = 8,  // F0, F1, ... F7 (c_farg0, c_farg1, ... )

    n_int_register_parameters_j   = 8, // r1, ... r7, r0 (rj_rarg0, j_rarg1, ...
    n_float_register_parameters_j = 8  // F0, F1, ... F7 (j_farg0, j_farg1, ...
  };
};

REGISTER_DECLARATION(Register, c_rarg0, r0);
REGISTER_DECLARATION(Register, c_rarg1, r1);
REGISTER_DECLARATION(Register, c_rarg2, r2);
REGISTER_DECLARATION(Register, c_rarg3, r3);
REGISTER_DECLARATION(Register, c_rarg4, r4);
REGISTER_DECLARATION(Register, c_rarg5, r5);
REGISTER_DECLARATION(Register, c_rarg6, r6);
REGISTER_DECLARATION(Register, c_rarg7, r7);

REGISTER_DECLARATION(FloatRegister, c_farg0, F0);
REGISTER_DECLARATION(FloatRegister, c_farg1, F1);
REGISTER_DECLARATION(FloatRegister, c_farg2, F2);
REGISTER_DECLARATION(FloatRegister, c_farg3, F3);
REGISTER_DECLARATION(FloatRegister, c_farg4, F4);
REGISTER_DECLARATION(FloatRegister, c_farg5, F5);
REGISTER_DECLARATION(FloatRegister, c_farg6, F6);
REGISTER_DECLARATION(FloatRegister, c_farg7, F7);

// Symbolically name the register arguments used by the Java calling convention.
// We have control over the convention for java so we can do what we please.
// What pleases us is to offset the java calling convention so that when
// we call a suitable jni method the arguments are lined up and we don't
// have to do much shuffling. A suitable jni method is non-static and a
// small number of arguments
//
//  |--------------------------------------------------------------------|
//  | c_rarg0  c_rarg1  c_rarg2 c_rarg3 c_rarg4 c_rarg5 c_rarg6 c_rarg7  |
//  |--------------------------------------------------------------------|
//  | r0       r1       r2      r3      r4      r5      r6      r7       |
//  |--------------------------------------------------------------------|
//  | j_rarg7  j_rarg0  j_rarg1 j_rarg2 j_rarg3 j_rarg4 j_rarg5 j_rarg6  |
//  |--------------------------------------------------------------------|


REGISTER_DECLARATION(Register, j_rarg0, c_rarg1);
REGISTER_DECLARATION(Register, j_rarg1, c_rarg2);
REGISTER_DECLARATION(Register, j_rarg2, c_rarg3);
REGISTER_DECLARATION(Register, j_rarg3, c_rarg4);
REGISTER_DECLARATION(Register, j_rarg4, c_rarg5);
REGISTER_DECLARATION(Register, j_rarg5, c_rarg6);
REGISTER_DECLARATION(Register, j_rarg6, c_rarg7);
REGISTER_DECLARATION(Register, j_rarg7, c_rarg0);

// Java floating args are passed as per C

REGISTER_DECLARATION(FloatRegister, j_farg0, F0);
REGISTER_DECLARATION(FloatRegister, j_farg1, F1);
REGISTER_DECLARATION(FloatRegister, j_farg2, F2);
REGISTER_DECLARATION(FloatRegister, j_farg3, F3);
REGISTER_DECLARATION(FloatRegister, j_farg4, F4);
REGISTER_DECLARATION(FloatRegister, j_farg5, F5);
REGISTER_DECLARATION(FloatRegister, j_farg6, F6);
REGISTER_DECLARATION(FloatRegister, j_farg7, F7);

// registers used to hold VM data either temporarily within a method
// or across method calls

// volatile (caller-save) registers

// r8 is used for indirect result location return
// we use it and r9 as scratch registers
REGISTER_DECLARATION(Register, rscratch1, r8);
REGISTER_DECLARATION(Register, rscratch2, r9);

// other volatile registers

// constant pool cache
REGISTER_DECLARATION(Register, r10_cpool,    r10);
// monitors allocated on stack
REGISTER_DECLARATION(Register, r11_monitors, r11);
// locals on stack
REGISTER_DECLARATION(Register, r12_locals,   r12);
// current method
REGISTER_DECLARATION(Register, r13_method,   r13);
// bytecode pointer
REGISTER_DECLARATION(Register, r14_bcp,      r14);
// Java expression stackpointer
REGISTER_DECLARATION(Register, r15_esp,      r15);

// non-volatile (callee-save) registers are r16-29
// of which the following are dedicated gloabl state

// link register
REGISTER_DECLARATION(Register, r30_lr,       r30);
// frame pointer
REGISTER_DECLARATION(Register, r29_fp,       r29);
// current thread
REGISTER_DECLARATION(Register, r28_thread,   r28);
// base of heap
REGISTER_DECLARATION(Register, r27_heapbase, r27);

// TODO : x86 uses rbp to save SP in method handle code
// we may need to do the same with fp
// JSR 292 fixed register usages:
//REGISTER_DECLARATION(Register, r29_mh_SP_save, r29);

#define assert_cond(ARG1) assert(ARG1, #ARG1)

namespace asm_util {
  uint32_t encode_immediate_v2(int is32, uint64_t imm);
};

using namespace asm_util;


class Assembler;

class Instruction_aarch64 {
  unsigned insn;
  unsigned bits;
  Assembler *assem;

public:

  Instruction_aarch64(class Assembler *as) {
    bits = 0;
    insn = 0;
    assem = as;
  }

  ~Instruction_aarch64();

  unsigned &get_insn() { return insn; }
  unsigned &get_bits() { return bits; }

  static inline int32_t extend(unsigned val, int hi = 31, int lo = 0) {
    union {
      unsigned u;
      int n;
    };

    u = val << (31 - hi);
    n = n >> (31 - hi + lo);
    return n;
  }

  void f(unsigned val, int msb, int lsb) {
    int nbits = msb - lsb + 1;
    assert_cond(val < (1U << nbits));
    assert_cond(msb >= lsb);
    unsigned mask = (1U << nbits) - 1;
    val <<= lsb;
    mask <<= lsb;
    insn |= val;
    assert_cond((bits & mask) == 0);
    bits |= mask;
  }

  void f(unsigned val, int bit) {
    f(val, bit, bit);
  }

  void sf(long val, int msb, int lsb) {
    int nbits = msb - lsb + 1;
    long chk = val >> (nbits - 1);
    assert_cond (chk == -1 || chk == 0);
    unsigned uval = val;
    unsigned mask = (1U << nbits) - 1;
    uval &= mask;
    f(uval, lsb + nbits - 1, lsb);
  }

  void rf(Register r, int lsb) {
    f(r->encoding_nocheck(), lsb + 4, lsb);
  }

  unsigned getf (int msb = 31, int lsb = 0) {
    int nbits = msb - lsb + 1;
    unsigned mask = ((1U << nbits) - 1) << lsb;
    assert_cond(bits & mask == mask);
    return (insn & mask) >> lsb;
  }

  void fixed(unsigned value, unsigned mask) {
    assert_cond ((mask & bits) == 0);
    bits |= mask;
    insn |= value;
  }
};

#define starti Instruction_aarch64 do_not_use(this); set_current(&do_not_use)

class Pre {
  int _offset;
  Register _r;
public:
  Pre(Register reg, int o) : _r(reg), _offset(o) { }
  int offset() { return _offset; }
  Register reg() { return _r; }
};

class Post {
  int _offset;
  Register _r;
public:
  Post(Register reg, int o) : _r(reg), _offset(o) { }
  int offset() { return _offset; }
  Register reg() { return _r; }
};

// Addressing modes
class Address VALUE_OBJ_CLASS_SPEC {
 public:
  enum mode { base_plus_offset, pre, post, pcrel,
	      base_plus_offset_reg };
  enum ScaleFactor { times_4, times_8 };
 private:
  Register _base;
  Register _index;
  int _offset;
  enum mode _mode;
  int _scale;

 public:
  Address(Register r)
    : _mode(base_plus_offset), _base(r), _offset(0) { }
  Address(Register r, int o)
    : _mode(base_plus_offset), _base(r), _offset(o) { }
  Address(Register r, Register r1, int scale = 0)
    : _mode(base_plus_offset_reg), _base(r), _index(r1), _scale(scale) { }
  Address(Pre p)
    : _mode(pre), _base(p.reg()), _offset(p.offset()) { }
  Address(Post p)
    : _mode(post), _base(p.reg()), _offset(p.offset()) { }
  Address(address a) : _mode(pcrel), _adr(a) { }

  void encode(Instruction_aarch64 *i) {
    switch(_mode) {
    case base_plus_offset:
      {
	i->f(0b111, 29, 27), i->f(0b01, 25, 24);
	unsigned shift = i->getf(31, 30);
	assert_cond((_offset >> shift) << shift == _offset);
	_offset >>= shift;
	i->sf(_offset, 21, 10);
	i->rf(_base, 5);
      }
      break;

    case base_plus_offset_reg:
      assert_cond(_scale == 0);
      i->f(0b111, 29, 27), i->f(0b00, 25, 24);
      i->f(1, 21);
      i->rf(_index, 16);
      i->rf(_base, 5);
      i->f(0b011, 15, 13); // Offset is always X register
      i->f(0, 12); // Shift is 0
      i->f(0b10, 11, 10);
      break;

    case pre:
      i->f(0b111, 29, 27), i->f(0b00, 25, 24);
      i->f(0, 21), i->f(0b11, 11, 10);
      i->sf(_offset, 20, 12);
      break;

    case post:
      i->f(0b111, 29, 27), i->f(0b00, 25, 24);
      i->f(0, 21), i->f(0b01, 11, 10);
      i->sf(_offset, 20, 12);
      break;

    default:
      assert_cond(false);
    }
  }
};

class Assembler : public AbstractAssembler {
public:
  Address pre(Register base, int offset) {
    return Address(Pre(base, offset));
  }

  Address post (Register base, int offset) {
    return Address(Post(base, offset));
  }

  Instruction_aarch64* current;
public:
  void set_current(Instruction_aarch64* i) { current = i; }

  void f(unsigned val, int msb, int lsb) {
    current->f(val, msb, lsb);
  }
  void f(unsigned val, int msb) {
    current->f(val, msb, msb);
  }
  void sf(long val, int msb, int lsb) {
    current->sf(val, msb, lsb);
  }
  void rf(Register reg, int lsb) {
    current->rf(reg, lsb);
  }
  void fixed(unsigned value, unsigned mask) {
    current->fixed(value, mask);
  }

  void emit() {
    emit_long(current->get_insn());
    assert_cond(current->get_bits() == 0xffffffff);
    current = NULL;
  }

  // PC-rel. addressing
#define INSN(NAME, op, shift)						\
  void NAME(Register Rd, address adr) {					\
    long offset = adr - pc();						\
    offset >>= shift;							\
    int offset_lo = offset & 3;						\
    offset >>= 2;							\
    starti;								\
    f(op, 31), f(offset_lo, 30, 29), f(0b10000, 28, 24), sf(offset, 23, 5); \
    rf(Rd, 0);								\
  }

  INSN(adr, 0, 0);
  INSN(adrp, 1, 12);

#undef INSN
  // Add/subtract (immediate)
#define INSN(NAME, decode)						\
  void NAME(Register Rd, Register Rn, unsigned imm, unsigned shift = 0) { \
    starti;								\
    f(decode, 31, 29), f(0b10001, 28, 24), f(shift, 23, 22), f(imm, 21, 10); \
    rf(Rd, 0), rf(Rn, 5);						\
  }

  INSN(addw,  0b000);
  INSN(addsw, 0b001);
  INSN(subw,  0b010);
  INSN(subsw, 0b011);
  INSN(add,   0b100);
  INSN(adds,  0b101);
  INSN(sub,   0b110);
  INSN(subs,  0b111);

#undef INSN

 // Logical (immediate)
#define INSN(NAME, decode, is32)				\
  void NAME(Register Rd, Register Rn, uint64_t imm) {		\
    starti;							\
    uint32_t val = encode_immediate_v2(is32, imm);		\
    f(decode, 31, 29), f(0b100100, 28, 23), f(val, 22, 10);	\
    rf(Rd, 0), rf(Rn, 5);					\
  }

  INSN(andw, 0b000, true);
  INSN(orrw, 0b001, true);
  INSN(eorw, 0b010, true);
  INSN(andsw, 0b011, true);
  INSN(andr,  0b100, false);
  INSN(orr,  0b101, false);
  INSN(eor,  0b110, false);
  INSN(ands, 0b111, false);

#undef INSN

  // Move wide (immediate)
#define INSN(NAME, opcode)						\
  void NAME(Register Rd, unsigned imm, unsigned shift = 0) {		\
    assert_cond((shift/16)*16 == shift);				\
    starti;								\
    f(opcode, 31, 29), f(0b100101, 28, 23), f(shift/16, 22, 21),	\
      f(imm, 20, 5);							\
    rf(Rd, 0);								\
  }

  INSN(movnw, 0b000);
  INSN(movzw, 0b010);
  INSN(movkw, 0b011);
  INSN(movn, 0b100);
  INSN(movz, 0b110);
  INSN(movk, 0b111);

#undef INSN

  // Bitfield
#define INSN(NAME, opcode)						\
  void NAME(Register Rd, Register Rn, unsigned immr, unsigned imms) {	\
    starti;								\
    f(opcode, 31, 22), f(immr, 21, 16), f(imms, 15, 10);		\
    rf(Rn, 5), rf(Rd, 0);						\
  }

  INSN(sbfmw, 0b0001001100);
  INSN(bfmw,  0b0011001100);
  INSN(ubfmw, 0b0101001100);
  INSN(sbfm,  0b1001001101);
  INSN(bfm,   0b1011001101);
  INSN(ubfm,  0b1101001101);

#undef INSN

  // Extract
#define INSN(NAME, opcode)						\
  void NAME(Register Rd, Register Rn, Register Rm, unsigned imms) {	\
    starti;								\
    f(opcode, 31, 21), f(imms, 15, 10);					\
    rf(Rm, 16), rf(Rn, 5), rf(Rd, 0);					\
  }

  INSN(extrw, 0b00010011100);
  INSN(extr,  0b10010011110);

#undef INSN

  // Unconditional branch (immediate)
#define INSN(NAME, opcode)					\
  void NAME(address dest) {					\
    starti;							\
    long offset = (dest - pc()) >> 2;				\
    f(opcode, 31), f(0b00101, 30, 26), sf(offset, 25, 0);	\
  }

  INSN(b, 0);
  INSN(bl, 1);

#undef INSN

  // Compare & branch (immediate)
#define INSN(NAME, opcode)				\
  void NAME(Register Rt, address dest) {		\
    long offset = (dest - pc()) >> 2;			\
    starti;						\
    f(opcode, 31, 24), sf(offset, 23, 5), rf(Rt, 0);	\
  }

  INSN(cbzw,  0b00110100);
  INSN(cbnzw, 0b00110101);
  INSN(cbz,   0b10110100);
  INSN(cbnz,  0b10110101);

#undef INSN

  // Test & branch (immediate)
#define INSN(NAME, opcode)						\
  void NAME(Register Rt, int bitpos, address dest) {			\
    long offset = (dest - pc()) >> 2;					\
    int b5 = bitpos >> 5;						\
    bitpos &= 0x1f;							\
    starti;								\
    f(b5, 31), f(opcode, 30, 24), f(bitpos, 23, 19), sf(offset, 18, 5);	\
    rf(Rt, 0);								\
  }

  INSN(tbz,  0b0110110);
  INSN(tbnz, 0b0110111);

#undef INSN

  // Conditional branch (immediate)
  void cond_branch(int cond, address dest) {
    long offset = (dest - pc()) >> 2;
    starti;
    f(0b0101010, 31, 25), f(0, 24), sf(offset, 23, 5), f(0, 4), f(cond, 3, 0);
  }

  enum {EQ, NE, HS, CS=HS, LO, CC=LO, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV};

#define INSN(NAME, cond)			\
  void NAME(address dest) {			\
    cond_branch(cond, dest);			\
  }

  INSN(beq, EQ);
  INSN(bne, NE);
  INSN(bhs, HS);
  INSN(bcs, CS);
  INSN(blo, LO);
  INSN(bcc, CC);
  INSN(bmi, MI);
  INSN(bpl, PL);
  INSN(bvs, VS);
  INSN(bvc, VC);
  INSN(bhi, HI);
  INSN(bls, LS);
  INSN(bge, GE);
  INSN(blt, LT);
  INSN(bgt, GT);
  INSN(ble, LE);
  INSN(bal, AL);
  INSN(bnv, NV);

#undef INSN

  // Exception generation
  void generate_exception(int opc, int op2, int LL, unsigned imm) {
    starti;
    f(0b11010100, 31, 24);
    f(opc, 23, 21), f(imm, 20, 5), f(op2, 4, 2), f(LL, 1, 0);
  }

#define INSN(NAME, opc, op2, LL)		\
  void NAME(unsigned imm) {			\
    generate_exception(opc, op2, LL, imm);	\
  }

  INSN(svc, 0b000, 0, 0b01);
  INSN(hvc, 0b000, 0, 0b10);
  INSN(smc, 0b000, 0, 0b11);
  INSN(brk, 0b001, 0, 0b00);
  INSN(hlt, 0b010, 0, 0b00);
  INSN(dpcs1, 0b101, 0, 0b01);
  INSN(dpcs2, 0b101, 0, 0b10);
  INSN(dpcs3, 0b101, 0, 0b11);

#undef INSN

  // System
  void system(int op0, int op1, int CRn, int CRm_op2, Register rt)
  {
    starti;
    f(0b11010101000, 31, 21);
    f(op0, 20, 19);
    f(op1, 18, 16);
    f(CRn, 15, 12);
    f(CRm_op2, 11, 5);
    rf(rt, 0);
  }

  void hint(int imm) {
    system(0b00, 0b011, 0b0010, imm, (Register)0b11111);
  }

  void nop() {
    hint(0);
  }

  // Unconditional branch (register)
  void branch_reg(Register R, int opc) {
    starti;
    f(0b1101011, 31, 25);
    f(opc, 24, 21);
    f(0b11111000000, 20, 10);
    rf(R, 5);
    f(0b00000, 4, 0);
  }

#define INSN(NAME, opc)				\
  void NAME(Register R) {			\
    branch_reg(R, opc);				\
  }

  INSN(br, 0b0000);
  INSN(blr, 0b0001);
  INSN(ret, 0b0010);

#undef INSN

#define INSN(NAME, opc)				\
  void NAME() {			\
    branch_reg((Register)0b11111, opc);		\
  }

  INSN(eret, 0b0100);
  INSN(drps, 0b0101);

#undef INSN

  // Load/store exclusive
  enum operand_size { byte, halfword, word, xword };

  void load_store_exclusive(Register Rs, Register Rt1, Register Rt2,
    Register Rn, enum operand_size sz, int op, int o0) {
    starti;
    f(sz, 31, 30), f(0b001000, 29, 24), f(op, 23, 21);
    rf(Rs, 16), f(o0, 15), rf(Rt2, 10), rf(Rn, 5), rf(Rt1, 0);
  }

#define INSN4(NAME, sz, op, o0) /* Four registers */			\
  void NAME(Register Rs, Register Rt1, Register Rt2, Register Rn) {	\
    load_store_exclusive(Rs, Rt1, Rt2, Rn, sz, op, o0);			\
  }

#define INSN3(NAME, sz, op, o0) /* Three registers */			\
  void NAME(Register Rs, Register Rt, Register Rn) {			\
    load_store_exclusive(Rs, Rt, (Register)0b11111, Rn, sz, op, o0);	\
  }

#define INSN2(NAME, sz, op, o0) /* Two registers */			\
  void NAME(Register Rt, Register Rn) {					\
    load_store_exclusive((Register)0b11111, Rt, (Register)0b11111,	\
			 Rn, sz, op, o0);				\
  }

#define INSN_FOO(NAME, sz, op, o0) /* Three registers, encoded differently */ \
  void NAME(Register Rt1, Register Rt2, Register Rn) {			\
    load_store_exclusive((Register)0b11111, Rt1, Rt2, Rn, sz, op, o0);	\
  }

  // bytes
  INSN3(stxrb, byte, 0b000, 0);
  INSN3(stlxrb, byte, 0b000, 1);
  INSN2(ldxrb, byte, 0b010, 0);
  INSN2(ldaxrb, byte, 0b010, 1);
  INSN2(stlrb, byte, 0b100, 1);
  INSN2(ldarb, byte, 0b110, 1);

  // halfwords
  INSN3(stxrh, halfword, 0b000, 0);
  INSN3(stlxrh, halfword, 0b000, 1);
  INSN2(ldxrh, halfword, 0b010, 0);
  INSN2(ldaxrh, halfword, 0b010, 1);
  INSN2(stlrh, halfword, 0b100, 1);
  INSN2(ldarh, halfword, 0b110, 1);

  // words
  INSN3(stxrw, word, 0b000, 0);
  INSN3(stlxrw, word, 0b000, 1);
  INSN4(stxpw, word, 0b001, 0);
  INSN4(stlxpw, word, 0b001, 1);
  INSN2(ldxrw, word, 0b010, 0);
  INSN2(ldaxrw, word, 0b010, 1);
  INSN_FOO(ldxpw, word, 0b011, 0);
  INSN_FOO(ldaxpw, word, 0b011, 1);
  INSN2(stlrw, word, 0b100, 1);
  INSN2(ldarw, word, 0b110, 1);

  // xwords
  INSN3(stxr, xword, 0b000, 0);
  INSN3(stlxr, xword, 0b000, 1);
  INSN4(stxp, xword, 0b001, 0);
  INSN4(stlxp, xword, 0b001, 1);
  INSN2(ldxr, xword, 0b010, 0);
  INSN2(ldaxr, xword, 0b010, 1);
  INSN_FOO(ldxp, xword, 0b011, 0);
  INSN_FOO(ldaxp, xword, 0b011, 1);
  INSN2(stlr, xword, 0b100, 1);
  INSN2(ldar, xword, 0b110, 1);

#undef INSN2
#undef INSN3
#undef INSN4
#undef INSN_FOO

  // Load register (literal)
#define INSN(NAME, opc, V)						\
  void NAME(Register Rt, address dest) {				\
    long offset = (dest - pc()) >> 2;					\
    starti;								\
    f(opc, 31, 30), f(0b011, 29, 27), f(V, 26), f(0b00, 25, 24),	\
      sf(offset, 23, 5);						\
    rf(Rt, 0);								\
  }

  INSN(ldrw, 0b00, 0);
  INSN(ldr, 0b01, 0);
  INSN(ldrsw, 0b10, 0);

#undef INSN

#define INSN(NAME, opc, V)						\
  void NAME(FloatRegister Rt, address dest) {				\
    long offset = (dest - pc()) >> 2;					\
    starti;								\
    f(opc, 31, 30), f(0b011, 29, 27), f(V, 26), f(0b00, 25, 24),	\
      sf(offset, 23, 5);						\
    rf((Register)Rt, 0);						\
  }

  INSN(ldrs, 0b00, 1);
  INSN(ldrd, 0b01, 1);

#undef INSN

#define INSN(NAME, opc, V)						\
  void NAME(address dest, int prfop = 0) {				\
    long offset = (dest - pc()) >> 2;					\
    starti;								\
    f(opc, 31, 30), f(0b011, 29, 27), f(V, 26), f(0b00, 25, 24),	\
      sf(offset, 23, 5);						\
    f(prfop, 4, 0);							\
  }

  INSN(prfm, 0b11, 0);

#undef INSN

  // Load/store
  void ld_st1(int opc, int p1, int V, int p2, int L,
	      Register Rt1, Register Rt2, Register Rn, int imm) {
    starti;
    f(opc, 31, 30), f(p1, 29, 27), f(V, 26), f(p2, 25, 23), f(L, 22);
    sf(imm, 21, 15);
    rf(Rt2, 10), rf(Rn, 5), rf(Rt1, 0);
  }

  int scale_ld_st(int size, int offset) {
    int imm;
    switch(size) {
      case 0b01:
      case 0b00:
	imm = offset >> 2;
	assert_cond(imm << 2 == offset);
	break;
      case 0b10:
	imm = offset >> 3;
	assert_cond(imm << 3 == offset);
	break;
      default:
	assert_cond(false);
      }
    return imm;
  }

  // Load/store register pair (offset)
#define INSN(NAME, size, p1, V, p2, L)					\
  void NAME(Register Rt1, Register Rt2, Register Rn, int offset) {	\
    ld_st1(size, p1, V, p2, L, Rt1, Rt2, Rn, scale_ld_st(size, offset)); \
  }

  INSN(stpw, 0b00, 0b101, 0, 0b0010, 0);
  INSN(ldpw, 0b00, 0b101, 0, 0b0010, 1);
  INSN(ldpsw, 0b01, 0b101, 0, 0b0010, 1);
  INSN(stp, 0b10, 0b101, 0, 0b0010, 0);
  INSN(ldp, 0b10, 0b101, 0, 0b0010, 1);

  // Load/store no-allocate pair (offset)
  INSN(stnpw, 0b00, 0b101, 0, 0b000, 0);
  INSN(ldnpw, 0b00, 0b101, 0, 0b000, 1);
  INSN(stnp, 0b10, 0b101, 0, 0b000, 0);
  INSN(ldnp, 0b10, 0b101, 0, 0b000, 1);

#undef INSN

  void ld_st2(Register Rt, Address adr, int size, int op) {
    starti;
    f(size, 31, 30);
    f(op, 23, 22); // str
    f(0, 26); // general reg
    rf(Rt, 0);
    adr.encode(current);
  }

#define INSN(NAME, size, op)			\
  void NAME(Register Rt, Address adr) {	\
    ld_st2(Rt, adr, size, op);			\
  }

  INSN(str, 0b11, 0b00);
  INSN(strw, 0b10, 0b00);
  INSN(strb, 0b00, 0b00);
  INSN(strh, 0b01, 0b00);

  INSN(ldr, 0b11, 0b01);
  INSN(ldrw, 0b10, 0b01);
  INSN(ldrb, 0b00, 0b01);
  INSN(ldrh, 0b01, 0b01);

  INSN(ldrsb, 0b00, 0b10);
  INSN(ldrsh, 0b01, 0b10);
  INSN(ldrshw, 0b01, 0b11);
  INSN(ldrsw, 0b10, 0b10);

#undef INSN

#define INSN(NAME, size, op)			\
  void NAME(Address adr) {			\
    ld_st2((Register)0, adr, size, op);		\
  }

  INSN(prfm, 0b11, 0b10); // FIXME: PRFM should not be used with
			  // writeback modes, but the assembler
			  // doesn't enfore that.

#undef INSN

#define INSN(NAME, size, op)				\
  void NAME(FloatRegister Rt, Address adr) {	\
    ld_st2((Register)Rt, adr, size, op, 1);		\
  }

  INSN(strd, 0b11, 0b00);
  INSN(strs, 0b10, 0b00);
  INSN(ldrd, 0b11, 0b01);
  INSN(ldrs, 0b10, 0b01);

#undef INSN

  enum shift_kind { lsl, lsr, asr, ror };

  void op_shifted_reg(unsigned decode,
		      Register Rd, Register Rn, Register Rm,
		      enum shift_kind kind, unsigned shift,
		      unsigned size, unsigned op) {
    f(size, 31);
    f(op, 30, 29);
    f(decode, 28, 24);
    rf(Rm, 16), rf(Rn, 5), rf(Rd, 0);
    f(shift, 15, 10);
    f(kind, 23, 22);
  }

  // Logical (shifted regsiter)
#define INSN(NAME, size, op, N)					\
  void NAME(Register Rd, Register Rn, Register Rm,		\
	    enum shift_kind kind = lsl, unsigned shift = 0) {	\
    starti;							\
    f(N, 21);							\
    op_shifted_reg(0b01010, Rd, Rn, Rm, kind, shift, size, op);	\
  }

  INSN(andr, 1, 0b00, 0);
  INSN(orr, 1, 0b01, 0);
  INSN(eor, 1, 0b10, 0);
  INSN(ands, 1, 0b11, 0);
  INSN(andw, 0, 0b00, 0);
  INSN(orrw, 0, 0b01, 0);
  INSN(eorw, 0, 0b10, 0);
  INSN(andsw, 0, 0b11, 0);

  INSN(bic, 1, 0b00, 1);
  INSN(orn, 1, 0b01, 1);
  INSN(eon, 1, 0b10, 1);
  INSN(bics, 1, 0b11, 1);
  INSN(bicw, 0, 0b00, 1);
  INSN(ornw, 0, 0b01, 1);
  INSN(eonw, 0, 0b10, 1);
  INSN(bicsw, 0, 0b11, 1);

#undef INSN

  // Add/subtract (shifted regsiter)
#define INSN(NAME, size, op)					\
  void NAME(Register Rd, Register Rn, Register Rm,		\
	    enum shift_kind kind = lsl, unsigned shift = 0) {	\
    starti;							\
    f(0, 21);							\
    assert_cond(kind != ror);					\
    op_shifted_reg(0b01011, Rd, Rn, Rm, kind, shift, size, op);	\
  }

  INSN(add, 1, 0b000);
  INSN(adds, 1, 0b001);
  INSN(sub, 1, 0b10);
  INSN(subs, 1, 0b11);
  INSN(addw, 0, 0b000);
  INSN(addsw, 0, 0b001);
  INSN(subw, 0, 0b10);
  INSN(subsw, 0, 0b11);

#undef INSN

  // Add/subtract (extended register)
#define INSN(NAME, op)							\
  void NAME(Register Rd, Register Rn, Register Rm,			\
           ext::operation option, int amount) {				\
    add_sub_extended_reg(op, 0b01011, Rd, Rn, Rm, 0b00, option, amount); \
  }

  void add_sub_extended_reg(unsigned op, unsigned decode,
    Register Rd, Register Rn, Register Rm,
    unsigned opt, ext::operation option, unsigned imm) {
    starti;
    f(op, 31, 29), f(decode, 28, 24), f(opt, 23, 22), f(1, 21);
    f(option, 15, 13), f(imm, 12, 10);
    rf(Rm, 16), rf(Rn, 5), rf(Rd, 0);
  }

  INSN(addw, 0b000);
  INSN(addsw, 0b001);
  INSN(subw, 0b010);
  INSN(subsw, 0b011);
  INSN(add, 0b100);
  INSN(adds, 0b101);
  INSN(sub, 0b110);
  INSN(subs, 0b111);

#undef INSN

  // Add/subtract (with carry)
  void add_sub_carry(unsigned op, Register Rd, Register Rn, Register Rm) {
    starti;
    f(op, 31, 29);
    f(0b11010000, 28, 21);
    f(0b000000, 15, 10);
    rf(Rm, 16), rf(Rn, 5), rf(Rd, 0);
  }

  #define INSN(NAME, op)				\
    void NAME(Register Rd, Register Rn, Register Rm) {	\
      add_sub_carry(op, Rd, Rn, Rm);			\
    }

  INSN(adcw, 0b000);
  INSN(adcsw, 0b001);
  INSN(sbcw, 0b010);
  INSN(sbcsw, 0b011);
  INSN(adc, 0b100);
  INSN(adcs, 0b101);
  INSN(sbc,0b110);
  INSN(sbcs, 0b111);

#undef INSN

  // Conditional compare (both kinds)
  void conditional_compare(unsigned op, int o2, int o3,
                           Register Rn, unsigned imm5, unsigned nzcv,
                           unsigned cond) {
    f(op, 31, 29);
    f(0b11010010, 28, 21);
    f(cond, 15, 12);
    f(o2, 10);
    f(o3, 4);
    f(nzcv, 3, 0);
    f(imm5, 20, 16), rf(Rn, 5);
  }

#define INSN(NAME, op)							\
  void NAME(Register Rn, Register Rm, int imm, condition_code cond) {	\
    starti;								\
    f(0, 11);								\
    conditional_compare(op, 0, 0, Rn, (uintptr_t)Rm, imm, cond);	\
  }									\
									\
  void NAME(Register Rn, int imm5, int imm, condition_code cond) {	\
    starti;								\
    f(1, 11);								\
    conditional_compare(op, 0, 0, Rn, imm5, imm, cond);			\
  }

  INSN(ccmnw, 0b001);
  INSN(ccmpw, 0b011);
  INSN(ccmn, 0b101);
  INSN(ccmp, 0b111);

#undef INSN

  // Conditional select
  void conditional_select(unsigned op, unsigned op2,
			  Register Rd, Register Rn, Register Rm,
			  unsigned cond) {
    starti;
    f(op, 31, 29);
    f(0b11010100, 28, 21);
    f(cond, 15, 12);
    f(op2, 11, 10);
    rf(Rm, 16), rf(Rn, 5), rf(Rd, 0);
  }

#define INSN(NAME, op, op2)						\
  void NAME(Register Rd, Register Rn, Register Rm, condition_code cond) { \
    conditional_select(op, op2, Rd, Rn, Rm, cond);			\
  }

  INSN(cselw, 0b000, 0b00);
  INSN(csincw, 0b000, 0b01);
  INSN(csinvw, 0b010, 0b00);
  INSN(csnegw, 0b010, 0b01);
  INSN(csel, 0b100, 0b00);
  INSN(csinc, 0b100, 0b01);
  INSN(csinv, 0b110, 0b00);
  INSN(csneg, 0b110, 0b01);

#undef INSN

  // Data processing
  void data_processing(unsigned op29, unsigned opcode,
		       Register Rd, Register Rn) {
    f(op29, 31, 29), f(0b11010110, 28, 21);
    f(opcode, 15, 10);
    rf(Rn, 5), rf(Rd, 0);
  }

  // (1 source)
#define INSN(NAME, op29, opcode2, opcode)	\
  void NAME(Register Rd, Register Rn) {		\
    starti;					\
    f(opcode2, 20, 16);				\
    data_processing(op29, opcode, Rd, Rn);	\
  }

  INSN(rbitw,  0b010, 0b00000, 0b00000);
  INSN(rev16w, 0b010, 0b00000, 0b00001);
  INSN(revw,   0b010, 0b00000, 0b00010);
  INSN(clzw,   0b010, 0b00000, 0b00100);
  INSN(clsw,   0b010, 0b00000, 0b00101);
 
  INSN(rbit,   0b110, 0b00000, 0b00000);
  INSN(rev16,  0b110, 0b00000, 0b00001);
  INSN(rev32,  0b110, 0b00000, 0b00010);
  INSN(rev,    0b110, 0b00000, 0b00011);
  INSN(clz,    0b110, 0b00000, 0b00100);
  INSN(cls,    0b110, 0b00000, 0b00101);

#undef INSN

  // (2 sources)
#define INSN(NAME, op29, opcode)			\
  void NAME(Register Rd, Register Rn, Register Rm) {	\
    starti;						\
    rf(Rm, 16);						\
    data_processing(op29, opcode, Rd, Rn);		\
  }

  INSN(udivw, 0b000, 0b000010);
  INSN(sdivw, 0b000, 0b000011);
  INSN(lslvw, 0b000, 0b001000);
  INSN(lsrvw, 0b000, 0b001001);
  INSN(asrvw, 0b000, 0b001010);
  INSN(rorvw, 0b000, 0b001011);

  INSN(udiv, 0b100, 0b000010);
  INSN(sdiv, 0b100, 0b000011);
  INSN(lslv, 0b100, 0b001000);
  INSN(lsrv, 0b100, 0b001001);
  INSN(asrv, 0b100, 0b001010);
  INSN(rorv, 0b100, 0b001011);

#undef INSN
 
  // (3 sources)
  void data_processing(unsigned op54, unsigned op31, unsigned o0,
		       Register Rd, Register Rn, Register Rm,
		       Register Ra) {
    starti;
    f(op54, 31, 29), f(0b11011, 28, 24);
    f(op31, 23, 21), f(o0, 15);
    rf(Rm, 16), rf(Ra, 10), rf(Rn, 5), rf(Rd, 0);
  }

#define INSN(NAME, op54, op31, o0)					\
  void NAME(Register Rd, Register Rn, Register Rm, Register Ra) {	\
    data_processing(op54, op31, o0, Rd, Rn, Rm, Ra);			\
  }

  INSN(maddw, 0b000, 0b000, 0);
  INSN(msubw, 0b000, 0b000, 1);
  INSN(madd, 0b100, 0b000, 0);
  INSN(msub, 0b100, 0b000, 1);
  INSN(smaddl, 0b100, 0b001, 0);
  INSN(smsubl, 0b100, 0b001, 1);
  INSN(umaddl, 0b100, 0b101, 0);
  INSN(umsubl, 0b100, 0b101, 1);

#undef INSN

#define INSN(NAME, op54, op31, o0)			\
  void NAME(Register Rd, Register Rn, Register Rm) {	\
    data_processing(op54, op31, o0, Rd, Rn, Rm, (Register)31);	\
  }

  INSN(smulh, 0b100, 0b010, 0);
  INSN(umulh, 0b100, 0b110, 0);

#undef INSN

  // Floating-point data-processing (1 source)
  void data_processing(unsigned op31, unsigned type, unsigned opcode,
		       FloatRegister Vd, FloatRegister Vn) {
    starti;
    f(op31, 31, 29);
    f(0b11110, 28, 24);
    f(type, 23, 22), f(1, 21), f(opcode, 20, 15), f(0b10000, 14, 10);
    rf(Vn, 5), rf(Vd, 0);
  }

#define INSN(NAME, op31, type, opcode)			\
  void NAME(FloatRegister Vd, FloatRegister Vn) {	\
    data_processing(op31, type, opcode, Vd, Vn);	\
  }

  INSN(fmovs, 0b000, 0b00, 0b000000);
  INSN(fabss, 0b000, 0b00, 0b000001);
  INSN(fnegs, 0b000, 0b00, 0b000010);
  INSN(fsqrts, 0b000, 0b00, 0b000011);
  INSN(fcvts, 0b000, 0b00, 0b000101);

  INSN(fmovd, 0b000, 0b01, 0b000000);
  INSN(fabsd, 0b000, 0b01, 0b000001);
  INSN(fnegd, 0b000, 0b01, 0b000010);
  INSN(fsqrtd, 0b000, 0b01, 0b000011);
  INSN(fcvtd, 0b000, 0b01, 0b000100);

#undef INSN

  // Floating-point data-processing (2 source)
  void data_processing(unsigned op31, unsigned type, unsigned opcode,
		       FloatRegister Vd, FloatRegister Vn, FloatRegister Vm) {
    starti;
    f(op31, 31, 29);
    f(0b11110, 28, 24);
    f(type, 23, 22), f(1, 21), f(opcode, 15, 12), f(0b10, 11, 10);
    rf(Vm, 16), rf(Vn, 5), rf(Vd, 0);
  }

#define INSN(NAME, op31, type, opcode)			\
  void NAME(FloatRegister Vd, FloatRegister Vn, FloatRegister Vm) {	\
    data_processing(op31, type, opcode, Vd, Vn, Vm);	\
  }

  INSN(fmuls, 0b000, 0b00, 0b0000);
  INSN(fdivs, 0b000, 0b00, 0b0001);
  INSN(fadds, 0b000, 0b00, 0b0010);
  INSN(fsubs, 0b000, 0b00, 0b0011);
  INSN(fnmuls, 0b000, 0b00, 0b1000);

  INSN(fmuld, 0b000, 0b01, 0b0000);
  INSN(fdivd, 0b000, 0b01, 0b0001);
  INSN(faddd, 0b000, 0b01, 0b0010);
  INSN(fsubd, 0b000, 0b01, 0b0011);
  INSN(fnmuld, 0b000, 0b01, 0b1000);

#undef INSN

   // Floating-point data-processing (3 source)
  void data_processing(unsigned op31, unsigned type, unsigned o1, unsigned o0,
		       FloatRegister Vd, FloatRegister Vn, FloatRegister Vm,
		       FloatRegister Va) {
    starti;
    f(op31, 31, 29);
    f(0b11111, 28, 24);
    f(type, 23, 22), f(o1, 21), f(o0, 15);
    rf(Vm, 16), rf(Va, 10), rf(Vn, 5), rf(Vd, 0);
  }

#define INSN(NAME, op31, type, o1, o0)					\
  void NAME(FloatRegister Vd, FloatRegister Vn, FloatRegister Vm,	\
	    FloatRegister Va) {						\
    data_processing(op31, type, o1, o0, Vd, Vn, Vm, Va);		\
  }

  INSN(fmadds, 0b000, 0b00, 0, 0);
  INSN(fmsubs, 0b000, 0b00, 0, 1);
  INSN(fnmadds, 0b000, 0b00, 1, 0);
  INSN(fnmsubs, 0b000, 0b00, 1, 1);

  INSN(fmaddd, 0b000, 0b01, 0, 0);
  INSN(fmsubd, 0b000, 0b01, 0, 1);
  INSN(fnmaddd, 0b000, 0b01, 1, 0);
  INSN(fnmsub, 0b000, 0b01, 1, 1);

#undef INSN

   // Floating-point<->integer conversions
  void float_int_convert(unsigned op31, unsigned type,
			 unsigned rmode, unsigned opcode,
			 Register Rd, Register Rn) {
    starti;
    f(op31, 31, 29);
    f(0b11110, 28, 24);
    f(type, 23, 22), f(1, 21), f(rmode, 20, 19);
    f(opcode, 18, 16), f(0b000000, 15, 10);
    rf(Rn, 5), rf(Rd, 0);
  }

#define INSN(NAME, op31, type, rmode, opcode)				\
  void NAME(Register Rd, FloatRegister Vn) {				\
    float_int_convert(op31, type, rmode, opcode, Rd, (Register)Vn);	\
  }

  INSN(fcvtzsw, 0b000, 0b00, 0b11, 0b000);
  INSN(fcvtzs,  0b100, 0b00, 0b11, 0b000);
  INSN(fcvtzdw, 0b000, 0b01, 0b11, 0b000);
  INSN(fcvtzd,  0b100, 0b01, 0b11, 0b000);

  INSN(fmovs, 0b000, 0b00, 0b00, 0b110);
  INSN(fmovd, 0b100, 0b01, 0b00, 0b110);

  // INSN(fmovhid, 0b100, 0b10, 0b01, 0b110);

#undef INSN

#define INSN(NAME, op31, type, rmode, opcode)				\
  void NAME(FloatRegister Vd, Register Rn) {				\
    float_int_convert(op31, type, rmode, opcode, (Register)Vd, Rn);	\
  }

  INSN(fmovs, 0b000, 0b00, 0b00, 0b111);
  INSN(fmovd, 0b100, 0b01, 0b00, 0b111);

  INSN(scvtfws, 0b000, 0b00, 0b00, 0b010);
  INSN(scvtfs,  0b100, 0b00, 0b00, 0b010);
  INSN(scvtfwd, 0b000, 0b01, 0b00, 0b010);
  INSN(scvtfd,  0b100, 0b01, 0b00, 0b010);

  // INSN(fmovhid, 0b100, 0b10, 0b01, 0b111);

#undef INSN

  // Floating-point compare
  void float_compare(unsigned op31, unsigned type,
		     unsigned op, unsigned op2,
		     FloatRegister Vn, FloatRegister Vm = (FloatRegister)0) {
    starti;
    f(op31, 31, 29);
    f(0b11110, 28, 24);
    f(type, 23, 22), f(1, 21);
    f(op, 15, 14), f(0b1000, 13, 10), f(op2, 4, 0);
    rf(Vn, 5), rf(Vm, 16);
  }


#define INSN(NAME, op31, type, op, op2)			\
  void NAME(FloatRegister Vn, FloatRegister Vm) {	\
    float_compare(op31, type, op, op2, Vn, Vm);		\
  }

#define INSN1(NAME, op31, type, op, op2)	\
  void NAME(FloatRegister Vn, double d) {	\
    assert_cond(d == 0.0);			\
    float_compare(op31, type, op, op2, Vn);	\
  }

  INSN(fcmps, 0b000, 0b00, 0b00, 0b00000);
  INSN1(fcmps, 0b000, 0b00, 0b00, 0b01000);
  // INSN(fcmpes, 0b000, 0b00, 0b00, 0b10000);
  // INSN1(fcmpes, 0b000, 0b00, 0b00, 0b11000);

  INSN(fcmpd, 0b000,   0b01, 0b00, 0b00000);
  INSN1(fcmpd, 0b000,  0b01, 0b00, 0b01000);
  // INSN(fcmped, 0b000,  0b01, 0b00, 0b10000);
  // INSN1(fcmped, 0b000, 0b01, 0b00, 0b11000);

>>>>>>> 1b077c7... Assembler fixes, assembler test cases.
#undef INSN

  Assembler(CodeBuffer* code) : AbstractAssembler(code) {
  }

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset) {
  }

  // Stack overflow checking
  virtual void bang_stack_with_offset(int offset) {
  }

  bool operand_valid_for_logical_immdiate(int is32, uint64_t imm);

  enum Condition {
    dummy
  };
};

#undef starti

inline Instruction_aarch64::~Instruction_aarch64() {
  assem->emit();
}

// extra stuff needed to compile
// not sure which of these methods are really necessary
class AddressLiteral VALUE_OBJ_CLASS_SPEC {
  friend class ArrayAddress;;
 protected:
  // creation
  AddressLiteral();
  public:
  AddressLiteral addr() { Unimplemented(); }
};

class ArrayAddress;
class BiasedLockingCounters;

class MacroAssembler: public Assembler {
  friend class LIR_Assembler;

 protected:

  Address as_Address(AddressLiteral adr);
  Address as_Address(ArrayAddress adr);

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
    int     number_of_arguments        // the number of arguments to pop after the call
  );

  // This is the base routine called by the different versions of call_VM. The interpreter
  // may customize this version by overriding it for its purposes (e.g., to save/restore
  // additional registers when doing a VM call).
  //
  // If no java_thread register is specified (noreg) than rdi will be used instead. call_VM_base
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

  // helpers for FPU flag access
  // tmp is a temporary register, if none is available use noreg
  void save_rax   (Register tmp);
  void restore_rax(Register tmp);

 public:
  MacroAssembler(CodeBuffer* code) : Assembler(code) {}

  // Support for NULL-checks
  //
  // Generates code that causes a NULL OS exception if the content of reg is NULL.
  // If the accessed location is M[reg + offset] and the offset is known, provide the
  // offset. No explicit code generation is needed if the offset is within a certain
  // range (0 <= offset <= page_size).

  void null_check(Register reg, int offset = -1);
  static bool needs_explicit_null_check(intptr_t offset);

  // Required platform-specific helpers for Label::patch_instructions.
  // They _shadow_ the declarations in AbstractAssembler, which are undefined.
  void pd_patch_instruction(address branch, address target);
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

  // Support for sign-extension (hi:lo = extend_sign(lo))
  void extend_sign(Register hi, Register lo);

  // Load and store values by size and signed-ness
  void load_sized_value(Register dst, Address src, size_t size_in_bytes, bool is_signed, Register dst2 = noreg);
  void store_sized_value(Address dst, Register src, size_t size_in_bytes, Register src2 = noreg);

  // Support for inc/dec with optimal instruction selection depending on value

  void increment(Register reg, int value = 1) { Unimplemented(); }
  void decrement(Register reg, int value = 1) { Unimplemented(); }

  void decrementl(Address dst, int value = 1);
  void decrementl(Register reg, int value = 1);

  void decrementq(Register reg, int value = 1);
  void decrementq(Address dst, int value = 1);

  void incrementl(Address dst, int value = 1);
  void incrementl(Register reg, int value = 1);

  void incrementq(Register reg, int value = 1);
  void incrementq(Address dst, int value = 1);


  // Support optimal SSE move instructions.

  void incrementl(AddressLiteral dst);
  void incrementl(ArrayAddress dst);

  // Alignment
  void align(int modulus);

  // A 5 byte nop that is safe for patching (see patch_verified_entry)
  void fat_nop();

  // Stack frame creation/removal
  void enter();
  void leave();

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
  void set_last_Java_frame(Register thread,
                           Register last_java_sp,
                           Register last_java_fp,
                           address last_java_pc);

  // thread in the default location (r15_thread on 64bit)
  void set_last_Java_frame(Register last_java_sp,
                           Register last_java_fp,
                           address last_java_pc);

  void reset_last_Java_frame(Register thread, bool clear_fp, bool clear_pc);

  // thread in the default location (r15_thread on 64bit)
  void reset_last_Java_frame(bool clear_fp, bool clear_pc);

  // Stores
  void store_check(Register obj);                // store check for obj - register is destroyed afterwards
  void store_check(Register obj, Address dst);   // same as above, dst is exact store location (reg. is destroyed)

#ifndef SERIALGC

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

#endif // SERIALGC

  // split store_check(Register obj) to enhance instruction interleaving
  void store_check_part_1(Register obj);
  void store_check_part_2(Register obj);

  // C 'boolean' to Java boolean: x == 0 ? 0 : 1
  void c2bool(Register x);

  // C++ bool manipulation

  void movbool(Register dst, Address src);
  void movbool(Address dst, bool boolconst);
  void movbool(Address dst, Register src);
  void testbool(Register dst);

  // oop manipulations
  void load_klass(Register dst, Register src);
  void store_klass(Register dst, Register src);

  void load_heap_oop(Register dst, Address src);
  void load_heap_oop_not_null(Register dst, Address src);
  void store_heap_oop(Address dst, Register src);

  // Used for storing NULL. All other oop constants should be
  // stored using routines that take a jobject.
  void store_heap_oop_null(Address dst);

  void load_prototype_header(Register dst, Register src);

  void store_klass_gap(Register dst, Register src);

  // This dummy is to prevent a call to store_heap_oop from
  // converting a zero (like NULL) into a Register by giving
  // the compiler two choices it can't resolve

  void store_heap_oop(Address dst, void* dummy);

  void encode_heap_oop(Register r);
  void decode_heap_oop(Register r);
  void encode_heap_oop_not_null(Register r);
  void decode_heap_oop_not_null(Register r);
  void encode_heap_oop_not_null(Register dst, Register src);
  void decode_heap_oop_not_null(Register dst, Register src);

  void set_narrow_oop(Register dst, jobject obj);
  void set_narrow_oop(Address dst, jobject obj);
  void cmp_narrow_oop(Register dst, jobject obj);
  void cmp_narrow_oop(Address dst, jobject obj);

  // if heap base register is used - reinit it with the correct value
  void reinit_heapbase();

  DEBUG_ONLY(void verify_heapbase(const char* msg);)

  // Int division/remainder for Java
  // (as idivl, but checks for special case as described in JVM spec.)
  // returns idivl instruction offset for implicit exception handling
  int corrected_idivl(Register reg);

  // Long division/remainder for Java
  // (as idivq, but checks for special case as described in JVM spec.)
  // returns idivq instruction offset for implicit exception handling
  int corrected_idivq(Register reg);

  void int3();

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

  // don't think we need these
#if 0
  void push_IU_state();
  void pop_IU_state();

  void push_FPU_state();
  void pop_FPU_state();

  void push_CPU_state();
  void pop_CPU_state();
#endif

  // Round up to a power of two
  void round_to(Register reg, int modulus);

  // Callee saved registers handling
  void push_callee_saved_registers();
  void pop_callee_saved_registers();

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

  // method handles (JSR 292)
  void check_method_handle_type(Register mtype_reg, Register mh_reg,
                                Register temp_reg,
                                Label& wrong_method_type);
  void load_method_handle_vmslots(Register vmslots_reg, Register mh_reg,
                                  Register temp_reg);
  void jump_to_method_handle_entry(Register mh_reg, Register temp_reg);
  Address argument_address(RegisterOrConstant arg_slot, int extra_slot_offset = 0);


  //----
  void set_word_if_not_zero(Register reg); // sets reg to 1 if not zero, otherwise 0

  // Debugging

  // only if +VerifyOops
  void verify_oop(Register reg, const char* s = "broken oop");
  void verify_oop_addr(Address addr, const char * s = "broken oop addr");

  // only if +VerifyFPU
  void verify_FPU(int stack_depth, const char* s = "illegal FPU state");

  // prints msg, dumps registers and stops execution
  void stop(const char* msg);

  // prints msg and continues
  void warn(const char* msg);

  static void debug32(int rdi, int rsi, int rbp, int rsp, int rbx, int rdx, int rcx, int rax, int eip, char* msg);
  static void debug64(char* msg, int64_t pc, int64_t regs[]);

  void os_breakpoint();

  void untested()                                { stop("untested"); }

  void unimplemented(const char* what = "")      { char* b = new char[1024];  jio_snprintf(b, 1024, "unimplemented: %s", what);  stop(b); }

  void should_not_reach_here()                   { stop("should not reach here"); }

  void print_CPU_state();

  // Stack overflow checking
  void bang_stack_with_offset(int offset) { Unimplemented(); }

  // Writes to stack successive pages until offset reached to check for
  // stack overflow + shadow pages.  Also, clobbers tmp
  void bang_stack_size(Register size, Register tmp);

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset);

  // Support for serializing memory accesses between threads
  void serialize_memory(Register thread, Register tmp);

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

  // Instructions that use AddressLiteral operands. These instruction can handle 32bit/64bit
  // operands. In general the names are modified to avoid hiding the instruction in Assembler
  // so that we don't need to implement all the varieties in the Assembler with trivial wrappers
  // here in MacroAssembler. The major exception to this rule is call

  // Arithmetics


  void addptr(Address dst, int32_t src) { Unimplemented(); }
  void addptr(Address dst, Register src);

  void addptr(Register dst, Address src) { Unimplemented(); }
  void addptr(Register dst, int32_t src);
  void addptr(Register dst, Register src);
  void addptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

  void andptr(Register dst, int32_t src);
  void andptr(Register src1, Register src2) { Unimplemented(); }

  void cmp8(AddressLiteral src1, int imm);

  // renamed to drag out the casting of address to int32_t/intptr_t
  void cmp32(Register src1, int32_t imm);

  void cmp32(AddressLiteral src1, int32_t imm);
  // compare reg - mem, or reg - &mem
  void cmp32(Register src1, AddressLiteral src2);

  void cmp32(Register src1, Address src2);

#ifndef _LP64
  void cmpoop(Address dst, jobject obj);
  void cmpoop(Register dst, jobject obj);
#endif // _LP64

  // NOTE src2 must be the lval. This is NOT an mem-mem compare
  void cmpptr(Address src1, AddressLiteral src2);

  void cmpptr(Register src1, AddressLiteral src2);

  void cmpptr(Register src1, Register src2) { Unimplemented(); }
  void cmpptr(Register src1, Address src2) { Unimplemented(); }
  // void cmpptr(Address src1, Register src2) { Unimplemented(); }

  void cmpptr(Register src1, int32_t src2) { Unimplemented(); }
  void cmpptr(Address src1, int32_t src2) { Unimplemented(); }

  // cmp64 to avoild hiding cmpq
  void cmp64(Register src1, AddressLiteral src);

  void cmpxchgptr(Register reg, Address adr);

  void locked_cmpxchgptr(Register reg, AddressLiteral adr);


  void imulptr(Register dst, Register src) { Unimplemented(); }


  void negptr(Register dst) { Unimplemented(); }

  void notptr(Register dst) { Unimplemented(); }

  void shlptr(Register dst, int32_t shift);
  void shlptr(Register dst) { Unimplemented(); }

  void shrptr(Register dst, int32_t shift);
  void shrptr(Register dst) { Unimplemented(); }

  void sarptr(Register dst) { Unimplemented(); }
  void sarptr(Register dst, int32_t src) { Unimplemented(); }

  void subptr(Address dst, int32_t src) { Unimplemented(); }

  void subptr(Register dst, Address src) { Unimplemented(); }
  void subptr(Register dst, int32_t src);
  // Force generation of a 4 byte immediate value even if it fits into 8bit
  void subptr_imm32(Register dst, int32_t src);
  void subptr(Register dst, Register src);
  void subptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

  void sbbptr(Address dst, int32_t src) { Unimplemented(); }
  void sbbptr(Register dst, int32_t src) { Unimplemented(); }

  void xchgptr(Register src1, Register src2) { Unimplemented(); }
  void xchgptr(Register src1, Address src2) { Unimplemented(); }

  void xaddptr(Address src1, Register src2) { Unimplemented(); }



  // Helper functions for statistics gathering.
  // Conditionally (atomically, on MPs) increments passed counter address, preserving condition codes.
  void cond_inc32(Condition cond, AddressLiteral counter_addr);
  // Unconditional atomic increment.
  void atomic_incl(AddressLiteral counter_addr);

  void lea(Register dst, AddressLiteral adr);
  void lea(Address dst, AddressLiteral adr);
  void lea(Register dst, Address adr) { Unimplemented(); }

  void leal32(Register dst, Address src) { Unimplemented(); }

  INSN(fmaddd, 0b000, 0b01, 0, 0);
  INSN(fmsubd, 0b000, 0b01, 0, 1);
  INSN(fnmadd, 0b000, 0b01, 0, 0);
  INSN(fnmsub, 0b000, 0b01, 0, 1);

  void orptr(Register dst, Address src) { Unimplemented(); }
  void orptr(Register dst, Register src) { Unimplemented(); }
  void orptr(Register dst, int32_t src) { Unimplemented(); }

  void testptr(Register src, int32_t imm32) {  Unimplemented(); }
  void testptr(Register src1, Register src2);

  void xorptr(Register dst, Register src) { Unimplemented(); }
  void xorptr(Register dst, Address src) { Unimplemented(); }

  INSN(fcvtzsw, 0b000, 0b00, 0b11, 0b000);
  INSN(fcvtzs, 0b000, 0b01, 0b11, 0b000);
  INSN(fcvtzdw, 0b100, 0b00, 0b11, 0b000);
  INSN(fcvtszd, 0b100, 0b01, 0b11, 0b000);

  void call(Label& L, relocInfo::relocType rtype);
  void call(Register entry);

  // Jumps

  // NOTE: these jumps tranfer to the effective address of dst NOT
  // the address contained by dst. This is because this is more natural
  // for jumps/calls.
  void jump(AddressLiteral dst);
  void jump_cc(Condition cc, AddressLiteral dst);

  // 32bit can do a case table jump in one instruction but we no longer allow the base
  // to be installed in the Address class. This jump will tranfers to the address
  // contained in the location described by entry (not the address of entry)
  void jump(ArrayAddress entry);

  // Floating

  void fadd_s(Address src)        { Unimplemented(); }
  void fadd_s(AddressLiteral src) { Unimplemented(); }

  void fldcw(Address src) { Unimplemented(); }
  void fldcw(AddressLiteral src);

  void fld_s(int index)   { Unimplemented(); }
  void fld_s(Address src) { Unimplemented(); }
  void fld_s(AddressLiteral src);

  void fld_d(Address src) { Unimplemented(); }
  void fld_d(AddressLiteral src);

  void fld_x(Address src) { Unimplemented(); }
  void fld_x(AddressLiteral src);

  INSN(fcmps, 0b000, 0b00, 0b00, 0b00000);
  INSN1(fcmps, 0b000, 0b00, 0b00, 0b01000);
  // INSN(fcmpes, 0b000, 0b00, 0b00, 0b10000);
  // INSN1(fcmpes, 0b000, 0b00, 0b00, 0b11000);

  INSN(fcmpd, 0b000,   0b01, 0b00, 0b00000);
  INSN1(fcmpd, 0b000,  0b01, 0b00, 0b01000);
  // INSN(fcmped, 0b000,  0b01, 0b00, 0b10000);
  // INSN1(fcmped, 0b000, 0b01, 0b00, 0b11000);

  // compute pow(x,y) and exp(x) with x86 instructions. Don't cover
  // all corner cases and may result in NaN and require fallback to a
  // runtime call.
  void fast_pow();
  void fast_exp();

  // computes exp(x). Fallback to runtime call included.
  void exp_with_fallback(int num_fpu_regs_in_use) { Unimplemented(); }
  // computes pow(x,y). Fallback to runtime call included.
  void pow_with_fallback(int num_fpu_regs_in_use) { Unimplemented(); }

public:

  // Data

  void cmov32( Condition cc, Register dst, Address  src);
  void cmov32( Condition cc, Register dst, Register src);

  void cmov(   Condition cc, Register dst, Register src) { Unimplemented(); }

  void cmovptr(Condition cc, Register dst, Address  src) { Unimplemented(); }
  void cmovptr(Condition cc, Register dst, Register src) { Unimplemented(); }

  void movoop(Register dst, jobject obj);
  void movoop(Address dst, jobject obj);

  void movptr(ArrayAddress dst, Register src);
  // can this do an lea?
  void movptr(Register dst, ArrayAddress src);

  void movptr(Register dst, Address src);

  void movptr(Register dst, AddressLiteral src);

  void movptr(Register dst, intptr_t src);
  void movptr(Register dst, Register src);
  void movptr(Address dst, intptr_t src);

  void movptr(Address dst, Register src);

  void movptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

#ifdef _LP64
  // Generally the next two are only used for moving NULL
  // Although there are situations in initializing the mark word where
  // they could be used. They are dangerous.

  // They only exist on LP64 so that int32_t and intptr_t are not the same
  // and we have ambiguous declarations.

  void movptr(Address dst, int32_t imm32);
  void movptr(Register dst, int32_t imm32);
#endif // _LP64

  // to avoid hiding movl
  void mov32(AddressLiteral dst, Register src);
  void mov32(Register dst, AddressLiteral src);

  // to avoid hiding movb
  void movbyte(ArrayAddress dst, int src);

  // Can push value or effective address
  void pushptr(AddressLiteral src);

  void pushptr(Address src) { Unimplemented(); }
  void popptr(Address src) { Unimplemented(); }

  void pushoop(jobject obj);

  // sign extend as need a l to ptr sized element
  void movl2ptr(Register dst, Address src) { Unimplemented(); }
  void movl2ptr(Register dst, Register src) { Unimplemented(); }

  // C2 compiled method's prolog code.
  void verified_entry(int framesize, bool stack_bang, bool fp_mode_24b);

#undef VIRTUAL
  // routine to generate an x86 prolog for a stub function which
  // bootstraps into the generated ARM code which directly follows the
  // stub
  //
  // the argument encodes the number of general and fp registers
  // passed by the caller and the calling convention plus also whether
  // a return value is expected.
  void c_stub_prolog(u_int64_t calltype);
};

inline bool AbstractAssembler::pd_check_instruction_mark() { Unimplemented(); return false; }

class BiasedLockingCounters;

#endif // CPU_AARCH64_VM_ASSEMBLER_AARCH64_HPP
