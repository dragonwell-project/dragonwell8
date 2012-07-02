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

#include "register_aarch64.hpp"

#define assert_cond(ARG1) assert(ARG1, #ARG1)

namespace asm_util {
  uint32_t encode_immediate_v2(int is32, uint64_t imm);
};

using namespace asm_util;


class Assembler_aarch64;

class Instruction {
  unsigned insn;
  unsigned bits;
  Assembler_aarch64 *assem;

public:

  Instruction(class Assembler_aarch64 *as) {
    bits = 0;
    insn = 0;
    assem = as;
  }

  ~Instruction();

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

#define starti Instruction do_not_use(this); set_current(&do_not_use)

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

// Address_aarch64ing modes
class Address_aarch64 VALUE_OBJ_CLASS_SPEC {
 public:
  enum mode { base_plus_offset, pre, post, pcrel,
	      base_plus_offset_reg, base_plus_offset_reg_extended};

 private:
  Register _base;
  Register _index;
  int _offset;
  enum mode _mode;
  address _adr;
  int _scale;

 public:
  Address_aarch64(Register r)
    : _mode(base_plus_offset), _base(r), _offset(0) { }
  Address_aarch64(Register r, int o)
    : _mode(base_plus_offset), _base(r), _offset(o) { }
  Address_aarch64(Register r, Register r1, int scale = 0)
    : _mode(base_plus_offset_reg), _base(r), _index(r1), _scale(scale) { }
  Address_aarch64(Pre p)
    : _mode(pre), _base(p.reg()), _offset(p.offset()) { }
  Address_aarch64(Post p)
    : _mode(post), _base(p.reg()), _offset(p.offset()) { }
  Address_aarch64(address a) : _mode(pcrel), _adr(a) { }

  void encode(Instruction *i) {
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
      i->f(_offset, 20, 12);
      i->rf(_base, 5);
      break;

    case post:
      i->f(0b111, 29, 27), i->f(0b00, 25, 24);
      i->f(0, 21), i->f(0b01, 11, 10);
      i->f(_offset, 20, 12);
      i->rf(_base, 5);
      break;

    default:
      assert_cond(false);
    }
  }
};

class Assembler_aarch64 : public AbstractAssembler {
public:
  Address_aarch64 pre(Register base, int offset) {
    return Address_aarch64(Pre(base, offset));
  }

  Address_aarch64 post (Register base, int offset) {
    return Address_aarch64(Post(base, offset));
  }

  Instruction* current;
public:
  void set_current(Instruction* i) { current = i; }

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
    f(0, 31), f(offset_lo, 30, 29), f(0b10000, 28, 24), sf(offset, 23, 5); \
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

  INSN(addwi,  0b000);
  INSN(addswi, 0b001);
  INSN(subwi,  0b010);
  INSN(subswi, 0b011);
  INSN(addi,   0b100);
  INSN(addsi,  0b101);
  INSN(subi,   0b110);
  INSN(subsi,  0b111);

#undef INSN

 // Logical (immediate)
#define INSN(NAME, decode, is32)				\
  void NAME(Register Rd, Register Rn, uint64_t imm) {		\
    starti;							\
    uint32_t val = encode_immediate_v2(is32, imm);		\
    f(decode, 31, 29), f(0b100100, 28, 23), f(val, 22, 10);	\
    rf(Rd, 0), rf(Rn, 5);					\
  }

  INSN(andwi, 0b000, true);
  INSN(orrwi, 0b001, true);
  INSN(eorwi, 0b000, true);
  INSN(andswi, 0b011, true);
  INSN(andi,  0b100, false);
  INSN(orri,  0b101, false);
  INSN(eori,  0b100, false);
  INSN(andsi, 0b111, false);

#undef INSN

  // Move wide (immediate)
#define INSN(NAME, opcode)						\
  void NAME(Register Rd, unsigned imm, unsigned shift = 0) {		\
    starti;								\
    f(opcode, 31, 29), f(0b100101, 28, 23), f(shift, 22, 21), f(imm, 20, 5); \
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

  INSN(sbfmw, 0b0000);
  INSN(bfmw,  0b0010);
  INSN(ubfmw, 0b0100);
  INSN(sbfm,  0b1000);
  INSN(bfm,   0b1010);
  INSN(ubfm,  0b1100);

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
  void NAME(int prfop, address dest) {					\
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

  void ld_st2(Register Rt, Address_aarch64 adr, int size, int op) {
    starti;
    f(size, 31, 30);
    f(op, 23, 22); // str
    f(0, 26); // general reg
    rf(Rt, 0);
    adr.encode(current);
  }

#define INSN(NAME, size, op)			\
  void NAME(Register Rt, Address_aarch64 adr) {	\
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

#undef INSN

  Assembler_aarch64(CodeBuffer* code) : AbstractAssembler(code) {
  }

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset) {
  }

  // Stack overflow checking
  virtual void bang_stack_with_offset(int offset) {
  }

  bool operand_valid_for_logical_immdiate(int is32, uint64_t imm);
};

#undef starti

Instruction::~Instruction() {
  assem->emit();
}


#endif // CPU_AARCH64_VM_ASSEMBLER_AARCH64_HPP
