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
    n_float_register_parameters_c = 8,  // v0, v1, ... v7 (c_farg0, c_farg1, ... )

    n_int_register_parameters_j   = 8, // r1, ... r7, r0 (rj_rarg0, j_rarg1, ...
    n_float_register_parameters_j = 8  // v0, v1, ... v7 (j_farg0, j_farg1, ...
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

REGISTER_DECLARATION(FloatRegister, c_farg0, v0);
REGISTER_DECLARATION(FloatRegister, c_farg1, v1);
REGISTER_DECLARATION(FloatRegister, c_farg2, v2);
REGISTER_DECLARATION(FloatRegister, c_farg3, v3);
REGISTER_DECLARATION(FloatRegister, c_farg4, v4);
REGISTER_DECLARATION(FloatRegister, c_farg5, v5);
REGISTER_DECLARATION(FloatRegister, c_farg6, v6);
REGISTER_DECLARATION(FloatRegister, c_farg7, v7);

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

REGISTER_DECLARATION(FloatRegister, j_farg0, v0);
REGISTER_DECLARATION(FloatRegister, j_farg1, v1);
REGISTER_DECLARATION(FloatRegister, j_farg2, v2);
REGISTER_DECLARATION(FloatRegister, j_farg3, v3);
REGISTER_DECLARATION(FloatRegister, j_farg4, v4);
REGISTER_DECLARATION(FloatRegister, j_farg5, v5);
REGISTER_DECLARATION(FloatRegister, j_farg6, v6);
REGISTER_DECLARATION(FloatRegister, j_farg7, v7);

// registers used to hold VM data either temporarily within a method
// or across method calls

// volatile (caller-save) registers

// r8 is used for indirect result location return
// we use it and r9 as scratch registers
REGISTER_DECLARATION(Register, rscratch1, r8);
REGISTER_DECLARATION(Register, rscratch2, r9);

// other volatile registers

// constant pool cache
REGISTER_DECLARATION(Register, rcpool,    r10);
// monitors allocated on stack
REGISTER_DECLARATION(Register, rmonitors, r11);
// locals on stack
REGISTER_DECLARATION(Register, rlocals,   r12);
// current method
REGISTER_DECLARATION(Register, rmethod,   r13);
// bytecode pointer
REGISTER_DECLARATION(Register, rbcp,      r14);
// Java expression stackpointer
REGISTER_DECLARATION(Register, resp,      r15);

// non-volatile (callee-save) registers are r16-29
// of which the following are dedicated gloabl state

// link register
REGISTER_DECLARATION(Register, lr,       r30);
// frame pointer
REGISTER_DECLARATION(Register, rfp,       r29);
// current thread
REGISTER_DECLARATION(Register, rthread,   r28);
// base of heap
REGISTER_DECLARATION(Register, rheapbase, r27);
// constant pool cache
REGISTER_DECLARATION(Register, rcpool,    r26);
// monitors allocated on stack
REGISTER_DECLARATION(Register, rmonitors, r25);
// locals on stack
REGISTER_DECLARATION(Register, rlocals,   r24);
// current method
REGISTER_DECLARATION(Register, rmethod,   r23);
// bytecode pointer
REGISTER_DECLARATION(Register, rbcp,      r22);
// Java expression stackpointer
// REGISTER_DECLARATION(Register, resp,      r21);

// TODO : x86 uses rbp to save SP in method handle code
// we may need to do the same with fp
// JSR 292 fixed register usages:
//REGISTER_DECLARATION(Register, r_mh_SP_save, r29);

#define assert_cond(ARG1) assert(ARG1, #ARG1)

namespace asm_util {
  uint32_t encode_immediate_v2(int is32, uint64_t imm);
};

using namespace asm_util;


class Assembler;

class Instruction_aarch64 {
  unsigned insn;
#ifdef ASSERT
  unsigned bits;
#endif
  Assembler *assem;

public:

  Instruction_aarch64(class Assembler *as) {
#ifdef ASSERT
    bits = 0;
#endif
    insn = 0;
    assem = as;
  }

  inline ~Instruction_aarch64();

  unsigned &get_insn() { return insn; }
#ifdef ASSERT
  unsigned &get_bits() { return bits; }
#endif

  static inline int32_t extend(unsigned val, int hi = 31, int lo = 0) {
    union {
      unsigned u;
      int n;
    };

    u = val << (31 - hi);
    n = n >> (31 - hi + lo);
    return n;
  }

  static inline uint32_t extract(uint32_t val, int msb, int lsb) {
    int nbits = msb - lsb + 1;
    assert_cond(msb >= lsb);
    uint32_t mask = (1U << nbits) - 1;
    uint32_t result = val >> lsb;
    result &= mask;
    return result;
  }

  static void patch(address a, int msb, int lsb, unsigned long val) {
    int nbits = msb - lsb + 1;
    guarantee(val < (1U << nbits), "Field too big for insn");
    assert_cond(msb >= lsb);
    unsigned mask = (1U << nbits) - 1;
    val <<= lsb;
    mask <<= lsb;
    unsigned target = *(unsigned *)a;
    target &= ~mask;
    target |= val;
    *(unsigned *)a = target;
  }

  static void spatch(address a, int msb, int lsb, long val) {
    int nbits = msb - lsb + 1;
    long chk = val >> (nbits - 1);
    guarantee (chk == -1 || chk == 0, "Field too big for insn");
    unsigned uval = val;
    unsigned mask = (1U << nbits) - 1;
    uval &= mask;
    uval <<= lsb;
    mask <<= lsb;
    unsigned target = *(unsigned *)a;
    target &= ~mask;
    target |= uval;
    *(unsigned *)a = target;
  }

  void f(unsigned val, int msb, int lsb) {
    int nbits = msb - lsb + 1;
    guarantee(val < (1U << nbits), "Field too big for insn");
    assert_cond(msb >= lsb);
    unsigned mask = (1U << nbits) - 1;
    val <<= lsb;
    mask <<= lsb;
    insn |= val;
    assert_cond((bits & mask) == 0);
#ifdef ASSERT
    bits |= mask;
#endif
  }

  void f(unsigned val, int bit) {
    f(val, bit, bit);
  }

  void sf(long val, int msb, int lsb) {
    int nbits = msb - lsb + 1;
    long chk = val >> (nbits - 1);
    guarantee (chk == -1 || chk == 0, "Field too big for insn");
    unsigned uval = val;
    unsigned mask = (1U << nbits) - 1;
    uval &= mask;
    f(uval, lsb + nbits - 1, lsb);
  }

  void rf(Register r, int lsb) {
    f(r->encoding_nocheck(), lsb + 4, lsb);
  }

  // reg|ZR
  void zrf(Register r, int lsb) {
    f(r->encoding_nocheck() - (r == zr), lsb + 4, lsb);
  }

  // reg|SP
  void srf(Register r, int lsb) {
    f(r == sp ? 31 : r->encoding_nocheck(), lsb + 4, lsb);
  }

  void rf(FloatRegister r, int lsb) {
    f(r->encoding_nocheck(), lsb + 4, lsb);
  }

  unsigned get(int msb = 31, int lsb = 0) {
    int nbits = msb - lsb + 1;
    unsigned mask = ((1U << nbits) - 1) << lsb;
    assert_cond(bits & mask == mask);
    return (insn & mask) >> lsb;
  }

  void fixed(unsigned value, unsigned mask) {
    assert_cond ((mask & bits) == 0);
#ifdef ASSERT
    bits |= mask;
#endif
    insn |= value;
  }
};

#define starti Instruction_aarch64 do_not_use(this); set_current(&do_not_use)

class PrePost {
  int _offset;
  Register _r;
public:
  PrePost(Register reg, int o) : _r(reg), _offset(o) { }
  PrePost(Register reg, ByteSize disp) : _r(reg), _offset(in_bytes(disp)) { }
  int offset() { return _offset; }
  Register reg() { return _r; }
};

class Pre : public PrePost {
public:
  Pre(Register reg, int o) : PrePost(reg, o) { }
  Pre(Register reg, ByteSize disp) : PrePost(reg, disp) { }
};
class Post : public PrePost {
public:
  Post(Register reg, int o) : PrePost(reg, o) { }
  Post(Register reg, ByteSize disp) : PrePost(reg, disp) { }
};

namespace ext
{
  enum operation { uxtb, uxth, uxtw, uxtx, sxtb, sxth, sxtw, sxtx };
};

// Addressing modes
class Address VALUE_OBJ_CLASS_SPEC {
 public:

  enum mode { base_plus_offset, pre, post, pcrel,
	      base_plus_offset_reg, literal };

  enum ScaleFactor { times_4, times_8 };

  // Shift and extend for base reg + reg offset addressing
  class extend {
    int _option, _shift;
    ext::operation _op;
  public:
    extend() { }
    extend(int s, int o, ext::operation op) : _shift(s), _option(o), _op(op) { }
    int option() const{ return _option; }
    int shift() const { return _shift; }
    ext::operation op() const { return _op; }
  };
  class uxtw : public extend {
  public:
    uxtw(int shift = -1): extend(shift, 0b010, ext::uxtw) { }
  };
  class lsl : public extend {
  public:
    lsl(int shift = -1): extend(shift, 0b011, ext::uxtx) { }
  };
  class sxtw : public extend {
  public:
    sxtw(int shift = -1): extend(shift, 0b110, ext::sxtw) { }
  };
  class sxtx : public extend {
  public:
    sxtx(int shift = -1): extend(shift, 0b111, ext::sxtx) { }
  };

 private:
  Register _base;
  Register _index;
  long _offset;
  enum mode _mode;
  extend _ext;

  RelocationHolder _rspec;

  // Typically we use AddressLiterals we want to use their rval
  // However in some situations we want the lval (effect address) of
  // the item.  We provide a special factory for making those lvals.
  bool _is_lval;

  // If the target is far we'll need to load the ea of this to a
  // register to reach it. Otherwise if near we can do PC-relative
  // addressing.
  address          _target;

  address target() const { return _target; }
  const RelocationHolder& rspec() const { return _rspec; }

 public:
  Address(Register r)
    : _mode(base_plus_offset), _base(r), _offset(0), _index(noreg) { }
  Address(Register r, int o)
    : _mode(base_plus_offset), _base(r), _offset(o), _index(noreg) { }
  Address(Register r, long o)
    : _mode(base_plus_offset), _base(r), _offset(o), _index(noreg) { }
  Address(Register r, unsigned long o)
    : _mode(base_plus_offset), _base(r), _offset(o), _index(noreg) { }
  Address(Register r, ByteSize disp)
    : _mode(base_plus_offset), _base(r), _offset(in_bytes(disp)),
      _index(noreg) { }
  Address(Register r, Register r1, extend ext = lsl())
    : _mode(base_plus_offset_reg), _base(r), _index(r1),
    _ext(ext), _offset(0) { }
  Address(Pre p)
    : _mode(pre), _base(p.reg()), _offset(p.offset()) { }
  Address(Post p)
    : _mode(post), _base(p.reg()), _offset(p.offset()) { }
  Address(address target, RelocationHolder const& rspec)
    : _mode(literal),
      _rspec(rspec),
      _is_lval(false),
      _target(target)  { }
  Address(address target, relocInfo::relocType rtype = relocInfo::external_word_type);
  Address(Register base, RegisterOrConstant index, extend ext = lsl(), int o = 0)
    : _base (base),
      _ext(ext), _offset(o) {
    if (index.is_register()) {
      _mode = base_plus_offset_reg;
      _index = index.as_register();
      assert(o == 0, "inconsistent address");
    } else {
      _mode = base_plus_offset;
      _offset = o;
    }
  }

  Register base() {
    guarantee((_mode == base_plus_offset | _mode == base_plus_offset_reg),
	      "wrong mode");
    return _base;
  }
  long offset() {
    return _offset;
  }
  Register index() {
    return _index;
  }
  bool uses(Register reg) const { return _base == reg || _index == reg; }

  void encode(Instruction_aarch64 *i) const {
    i->f(0b111, 29, 27);
    i->srf(_base, 5);

    switch(_mode) {
    case base_plus_offset:
      {
	unsigned size = i->get(31, 30);
	unsigned mask = (1 << size) - 1;
	if (_offset < 0 || _offset & mask)
	  {
	    i->f(0b00, 25, 24);
	    i->f(0, 21), i->f(0b00, 11, 10);
	    i->sf(_offset, 20, 12);
	  } else {
	    i->f(0b01, 25, 24);
	    i->f(_offset >> size, 21, 10);
	  }
      }
      break;

    case base_plus_offset_reg:
      {
	i->f(0b00, 25, 24);
	i->f(1, 21);
	i->rf(_index, 16);
	i->f(_ext.option(), 15, 13);
	unsigned size = i->get(31, 30);
	if (size == 0) // It's a byte
	  i->f(_ext.shift() >= 0, 12);
	else {
	  if (_ext.shift() > 0)
	    assert(_ext.shift() == (int)size, "bad shift");
	  i->f(_ext.shift() > 0, 12);
	}
	i->f(0b10, 11, 10);
      }
      break;

    case pre:
      i->f(0b00, 25, 24);
      i->f(0, 21), i->f(0b11, 11, 10);
      i->sf(_offset, 20, 12);
      break;

    case post:
      i->f(0b00, 25, 24);
      i->f(0, 21), i->f(0b01, 11, 10);
      i->sf(_offset, 20, 12);
      break;

    case literal:
      ShouldNotReachHere();
      break;

    default:
      ShouldNotReachHere();
    }
  }
  void lea(MacroAssembler *, Register) const;
};

// Convience classes
class RuntimeAddress: public Address {

  public:

  RuntimeAddress(address target) : Address(target, relocInfo::runtime_call_type) {}

};

class OopAddress: public Address {

  public:

  OopAddress(address target) : Address(target, relocInfo::oop_type){}

};

class ExternalAddress: public Address {
 private:
  static relocInfo::relocType reloc_for_target(address target) {
    // Sometimes ExternalAddress is used for values which aren't
    // exactly addresses, like the card table base.
    // external_word_type can't be used for values in the first page
    // so just skip the reloc in that case.
    return external_word_Relocation::can_be_relocated(target) ? relocInfo::external_word_type : relocInfo::none;
  }

 public:

  ExternalAddress(address target) : Address(target, reloc_for_target(target)) {}

};

class InternalAddress: public Address {

  public:

  InternalAddress(address target) : Address(target, relocInfo::internal_word_type) {}
};

const int FPUStateSizeInWords = 27; // FIXME   :-)

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
  void srf(Register reg, int lsb) {
    current->srf(reg, lsb);
  }
  void zrf(Register reg, int lsb) {
    current->zrf(reg, lsb);
  }
  void rf(FloatRegister reg, int lsb) {
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

  typedef void (Assembler::* uncond_branch_insn)(address dest);
  typedef void (Assembler::* compare_and_branch_insn)(Register Rt, address dest);
  typedef void (Assembler::* test_and_branch_insn)(Register Rt, int bitpos, address dest);
  typedef void (Assembler::* prefetch_insn)(address target, int prfop);

  void wrap_label(Label &L, uncond_branch_insn insn);
  void wrap_label(Register r, Label &L, compare_and_branch_insn insn);
  void wrap_label(Register r, int bitpos, Label &L, test_and_branch_insn insn);
  void wrap_label(Label &L, int prfop, prefetch_insn insn);

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
  }									\
  void NAME(Register Rd, Label &L) {					\
    wrap_label(Rd, L, &Assembler::NAME);				\
  }

  INSN(adr, 0, 0);
  INSN(adrp, 1, 12);

#undef INSN

  // Add/subtract (immediate)
#define INSN(NAME, decode)						\
  void NAME(Register Rd, Register Rn, unsigned imm, unsigned shift = 0) { \
    starti;								\
    f(decode, 31, 29), f(0b10001, 28, 24), f(shift, 23, 22), f(imm, 21, 10); \
    zrf(Rd, 0), srf(Rn, 5);						\
  }

  INSN(addsw, 0b001);
  INSN(subsw, 0b011);
  INSN(adds,  0b101);
  INSN(subs,  0b111);

#undef INSN

#define INSN(NAME, decode)						\
  void NAME(Register Rd, Register Rn, unsigned imm, unsigned shift = 0) { \
    starti;								\
    f(decode, 31, 29), f(0b10001, 28, 24), f(shift, 23, 22), f(imm, 21, 10); \
    srf(Rd, 0), srf(Rn, 5);						\
  }

  INSN(addw,  0b000);
  INSN(subw,  0b010);
  INSN(add,   0b100);
  INSN(sub,   0b110);

#undef INSN

 // Logical (immediate)
#define INSN(NAME, decode, is32)				\
  void NAME(Register Rd, Register Rn, uint64_t imm) {		\
    starti;							\
    uint32_t val = encode_immediate_v2(is32, imm);		\
    f(decode, 31, 29), f(0b100100, 28, 23), f(val, 22, 10);	\
    srf(Rd, 0), zrf(Rn, 5);					\
  }

  INSN(andw, 0b000, true);
  INSN(orrw, 0b001, true);
  INSN(eorw, 0b010, true);
  INSN(andr,  0b100, false);
  INSN(orr,  0b101, false);
  INSN(eor,  0b110, false);

#undef INSN

#define INSN(NAME, decode, is32)				\
  void NAME(Register Rd, Register Rn, uint64_t imm) {		\
    starti;							\
    uint32_t val = encode_immediate_v2(is32, imm);		\
    f(decode, 31, 29), f(0b100100, 28, 23), f(val, 22, 10);	\
    zrf(Rd, 0), zrf(Rn, 5);					\
  }

  INSN(ands, 0b111, false);
  INSN(andsw, 0b011, true);

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
  }								\
  void NAME(Label &L) {						\
    wrap_label(L, &Assembler::NAME);				\
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
  }							\
  void NAME(Register Rt, Label &L) {			\
    wrap_label(Rt, L, &Assembler::NAME);		\
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
  }									\
  void NAME(Register Rt, int bitpos, Label &L) {			\
    wrap_label(Rt, bitpos, L, &Assembler::NAME);			\
  }

  INSN(tbz,  0b0110110);
  INSN(tbnz, 0b0110111);

#undef INSN

  // Conditional branch (immediate)
  enum Condition
    {EQ, NE, HS, CS=HS, LO, CC=LO, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV};

  void br(Condition  cond, address dest) {
    long offset = (dest - pc()) >> 2;
    starti;
    f(0b0101010, 31, 25), f(0, 24), sf(offset, 23, 5), f(0, 4), f(cond, 3, 0);
  }

#define INSN(NAME, cond)			\
  void NAME(address dest) {			\
    br(cond, dest);				\
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

  void br(Condition cc, Label &L);

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
  void system(int op0, int op1, int CRn, int CRm, int op2,
	      Register rt = (Register)0b11111)
  {
    starti;
    f(0b11010101000, 31, 21);
    f(op0, 20, 19);
    f(op1, 18, 16);
    f(CRn, 15, 12);
    f(CRm, 11, 8);
    f(op2, 7, 5);
    rf(rt, 0);
  }

  void hint(int imm) {
    system(0b00, 0b011, 0b0010, imm, 0b000);
  }

  void nop() {
    hint(0);
  }

  enum barrier {OSHLD = 0b0001, OSHST, OSH, NSHLD=0b0101, NSHST, NSH,
		ISHLD = 0b1001, ISHST, ISH, LD=0b1101, ST, SY};

  void dsb(barrier imm) {
    system(0b00, 0b011, 0b00011, imm, 0b100);
  }

  void dmb(barrier imm) {
    system(0b00, 0b011, 0b00011, imm, 0b101);
  }

  void isb() {
    system(0b00, 0b011, 0b00011, SY, 0b110);
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
  }									\
  void NAME(Register Rt, Label &L) {					\
    wrap_label(Rt, L, &Assembler::NAME);				\
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
  }									\
  void NAME(Label &L, int prfop = 0) {					\
    wrap_label(L, prfop, &Assembler::NAME);				\
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

  // Load/store register (all modes)
  void ld_st2(Register Rt, const Address &adr, int size, int op, int V = 0) {
    starti;
    f(size, 31, 30);
    f(op, 23, 22); // str
    f(V, 26); // general reg?
    zrf(Rt, 0);
    adr.encode(current);
  }

#define INSN(NAME, size, op)				\
  void NAME(Register Rt, const Address &adr) {		\
    ld_st2(Rt, adr, size, op);				\
  }							\

  INSN(str, 0b11, 0b00);
  INSN(strw, 0b10, 0b00);
  INSN(strb, 0b00, 0b00);
  INSN(strh, 0b01, 0b00);

  INSN(ldr, 0b11, 0b01);
  INSN(ldrw, 0b10, 0b01);
  INSN(ldrb, 0b00, 0b01);
  INSN(ldrh, 0b01, 0b01);

  INSN(ldrsb, 0b00, 0b10);
  INSN(ldrsbw, 0b00, 0b11);
  INSN(ldrsh, 0b01, 0b10);
  INSN(ldrshw, 0b01, 0b11);
  INSN(ldrsw, 0b10, 0b10);

#undef INSN

#define INSN(NAME, size, op)			\
  void NAME(const Address &adr) {			\
    ld_st2((Register)0, adr, size, op);		\
  }

  INSN(prfm, 0b11, 0b10); // FIXME: PRFM should not be used with
			  // writeback modes, but the assembler
			  // doesn't enfore that.

#undef INSN

#define INSN(NAME, size, op)				\
  void NAME(FloatRegister Rt, const Address &adr) {	\
    ld_st2((Register)Rt, adr, size, op, 1);		\
  }

  INSN(strd, 0b11, 0b00);
  INSN(strs, 0b10, 0b00);
  INSN(ldrd, 0b11, 0b01);
  INSN(ldrs, 0b10, 0b01);

#undef INSN

  enum shift_kind { LSL, LSR, ASR, ROR };

  void op_shifted_reg(unsigned decode,
		      enum shift_kind kind, unsigned shift,
		      unsigned size, unsigned op) {
    f(size, 31);
    f(op, 30, 29);
    f(decode, 28, 24);
    f(shift, 15, 10);
    f(kind, 23, 22);
  }

  // Logical (shifted regsiter)
#define INSN(NAME, size, op, N)					\
  void NAME(Register Rd, Register Rn, Register Rm,		\
	    enum shift_kind kind = LSL, unsigned shift = 0) {	\
    starti;							\
    f(N, 21);							\
    zrf(Rm, 16), zrf(Rn, 5), zrf(Rd, 0);			\
    op_shifted_reg(0b01010, kind, shift, size, op);		\
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
	    enum shift_kind kind = LSL, unsigned shift = 0) {	\
    starti;							\
    f(0, 21);							\
    assert_cond(kind != ROR);					\
    zrf(Rd, 0), srf(Rn, 5), rf(Rm, 16);				\
    op_shifted_reg(0b01011, kind, shift, size, op);		\
  }

#undef INSN

  // Add/subtract (shifted regsiter)
#define INSN(NAME, size, op)					\
  void NAME(Register Rd, Register Rn, Register Rm,		\
	    enum shift_kind kind = LSL, unsigned shift = 0) {	\
    starti;							\
    f(0, 21);							\
    assert_cond(kind != ROR);					\
    zrf(Rd, 0), zrf(Rn, 5), rf(Rm, 16);				\
    op_shifted_reg(0b01011, kind, shift, size, op);		\
  }

  INSN(add, 1, 0b000);
  INSN(sub, 1, 0b10);
  INSN(addw, 0, 0b000);
  INSN(subw, 0, 0b10);

  INSN(adds, 1, 0b001);
  INSN(subs, 1, 0b11);
  INSN(addsw, 0, 0b001);
  INSN(subsw, 0, 0b11);

#undef INSN

  // Add/subtract (extended register)
#define INSN(NAME, op)							\
  void NAME(Register Rd, Register Rn, Register Rm,			\
           ext::operation option, int amount) {				\
    starti;								\
    zrf(Rm, 16), srf(Rn, 5), srf(Rd, 0);				\
    add_sub_extended_reg(op, 0b01011, Rd, Rn, Rm, 0b00, option, amount); \
  }

  void add_sub_extended_reg(unsigned op, unsigned decode,
    Register Rd, Register Rn, Register Rm,
    unsigned opt, ext::operation option, unsigned imm) {
    f(op, 31, 29), f(decode, 28, 24), f(opt, 23, 22), f(1, 21);
    f(option, 15, 13), f(imm, 12, 10);
  }

  INSN(addw, 0b000);
  INSN(subw, 0b010);
  INSN(add, 0b100);
  INSN(sub, 0b110);

#undef INSN

#define INSN(NAME, op)							\
  void NAME(Register Rd, Register Rn, Register Rm,			\
           ext::operation option, int amount) {				\
    starti;								\
    zrf(Rm, 16), srf(Rn, 5), zrf(Rd, 0);					\
    add_sub_extended_reg(op, 0b01011, Rd, Rn, Rm, 0b00, option, amount); \
  }

  INSN(addsw, 0b001);
  INSN(subsw, 0b011);
  INSN(adds, 0b101);
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
  void NAME(Register Rn, Register Rm, int imm, Condition cond) {	\
    starti;								\
    f(0, 11);								\
    conditional_compare(op, 0, 0, Rn, (uintptr_t)Rm, imm, cond);	\
  }									\
									\
  void NAME(Register Rn, int imm5, int imm, Condition cond) {	\
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
    zrf(Rm, 16), zrf(Rn, 5), rf(Rd, 0);
  }

#define INSN(NAME, op, op2)						\
  void NAME(Register Rd, Register Rn, Register Rm, Condition cond) { \
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
    zrf(Rm, 16), zrf(Ra, 10), zrf(Rn, 5), zrf(Rd, 0);
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
    zrf(Rn, 5), zrf(Rd, 0);
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

#undef INSN
#undef INSN1

  // Floating-point Move (immediate)
private:
  unsigned pack(double value);

  void fmov_imm(FloatRegister Vn, double value, unsigned size) {
    starti;
    f(0b00011110, 31, 24), f(size, 23, 22), f(1, 21);
    f(pack(value), 20, 13), f(0b10000000, 12, 5);
    rf(Vn, 0);
  }

public:

  void fmovs(FloatRegister Vn, double value) {
    fmov_imm(Vn, value, 0b00);

  }
  void fmovd(FloatRegister Vn, double value) {
    fmov_imm(Vn, value, 0b01);
  }

/* Simulator extensions to the ISA

   haltsim

   takes no arguments, causes sim to run dumpState() and then return from
   run() with STATUS_HALT -- maybe it should enters debug first? The
   linking code which enters the sim via run() will call fatal() when it
   sees STATUS_HALT.

   brx86 Xn, Wm
   brx86 Xn, #gpargs, #fpargs, #type
   Xn holds the 64 bit x86 branch_address
   call format is encoded either as immediate data in the call
   or in register Wm. In the latter case
     Wm[9..6] = #gpargs,
     Wm[5..2] = #fpargs,
     Wm[1,0] = #type

   calls the x86 code address 'branch_address' supplied in Xn passing
   arguments taken from the general and floating point registers according
   to the supplied counts 'gpargs' and 'fpargs'. may return a result in r0
   or v0 according to the the return type #type' where

   address branch_address;
   uimm4 gpargs;
   uimm4 fpargs;
   enum ReturnType type;

   enum ReturnType
     {
       void_ret = 0,
       int_ret = 1,
       long_ret = 1,
       obj_ret = 1, // i.e. same as long
       float_ret = 2,
       double_ret = 3
     }

   Instruction encodings
   ---------------------

   These are encoded in the space with instr[28:25] = 00 which is
   unallocated. Encodings are

                     10987654321098765432109876543210
   PSEUDO_HALT   = 0x11100000000000000000000000000000
   PSEUDO_BRX86  = 0x11000000000000000_______________
   PSEUDO_BRX86R = 0x1100000000000000100000___________

   instr[31,29] = op1 : 111 ==> HALT, 110 ==> BRX86/BRX86R

   for BRX86
     instr[14,11] = #gpargs, instr[10,7] = #fpargs
     instr[6,5] = #type, instr[4,0] = Rn
   for BRX86R
     instr[9,5] = Rm, instr[4,0] = Rn
*/

  void brx86(Register Rn, int gpargs, int fpargs, int type) {
    starti;
    f(0b110, 31 ,29);
    f(0b00, 28, 25);
    //  4321098765
    f(0b0000000000, 24, 15);
    f(gpargs, 14, 11);
    f(fpargs, 10, 7);
    f(type, 6, 5);
    rf(Rn, 0);
  }

  void brx86(Register Rn, Register Rm) {
    starti;
    f(0b110, 31 ,29);
    f(0b00, 28, 25);
    //  4321098765
    f(0b0000000001, 24, 15);
    //  43210
    f(0b00000, 14, 10);
    rf(Rm, 5);
    rf(Rn, 0);
  }


  void haltsim() {
    starti;
    f(0b111, 31 ,29);
    f(0b00, 28, 27);
    //  654321098765432109876543210
    f(0b000000000000000000000000000, 26, 0);
  }

  Assembler(CodeBuffer* code) : AbstractAssembler(code) {
  }

  virtual RegisterOrConstant delayed_value_impl(intptr_t* delayed_value_addr,
                                                Register tmp,
                                                int offset) {
  }

  // Stack overflow checking
  virtual void bang_stack_with_offset(int offset);

  bool operand_valid_for_logical_immdiate(int is32, uint64_t imm);
};

Instruction_aarch64::~Instruction_aarch64() {
  assem->emit();
}

#undef starti

// extra stuff needed to compile
// not sure which of these methods are really necessary
class BiasedLockingCounters;

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
    int     number_of_arguments        // the number of arguments to pop after the call
  );

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

  // helpers for FPU flag access
  // tmp is a temporary register, if none is available use noreg
  void save_rax   (Register tmp);
  void restore_rax(Register tmp);

 public:
  MacroAssembler(CodeBuffer* code) : Assembler(code) {}

  // Load Effective Address
  void lea(Register r, const Address &a) { a.lea(this, r); }

  virtual void call_Unimplemented() {
    haltsim();
  }

  // aliases defined in AARCH64 spec

  inline void cmpw(Register Rd, unsigned imm)  { subsw(zr, Rd, imm); }
  inline void cmp(Register Rd, unsigned imm)  { subs(zr, Rd, imm); }

  inline void cmnw(Register Rd, unsigned imm) { addsw(zr, Rd, imm); }
  inline void cmn(Register Rd, unsigned imm) { adds(zr, Rd, imm); }

  inline void movw(Register Rd, Register Rn) {
    if (Rd == sp || Rn == sp) {
      addw(Rd, Rn, 0U);
    } else {
      orrw(Rd, zr, Rn);
    }
  }
  inline void mov(Register Rd, Register Rn) {
    if (Rd == sp || Rn == sp) {
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
    mov(dst, (u_int64_t)i);
  }

  // idiv variant which deals with MINLONG as dividend and -1 as divisor
  // not sure we need this given how aarch64 div instruction works
  int corrected_idivl(Register ra, Register rb);

  // Support for NULL-checks
  //
  // Generates code that causes a NULL OS exception if the content of reg is NULL.
  // If the accessed location is M[reg + offset] and the offset is known, provide the
  // offset. No explicit code generation is needed if the offset is within a certain
  // range (0 <= offset <= page_size).

  virtual void null_check(Register reg, int offset = -1);
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
                           address last_java_pc);

  void reset_last_Java_frame(Register thread, bool clearfp, bool clear_pc);

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

  void push_IU_state();
  void pop_IU_state();

  void push_FPU_state();
  void pop_FPU_state();

  void push_CPU_state();
  void pop_CPU_state();

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

  // Arithmetics


  void addptr(Address dst, int32_t src) { Unimplemented(); }
  void addptr(Address dst, Register src);

  void addptr(Register dst, Address src) { Unimplemented(); }
  void addptr(Register dst, int32_t src);
  void addptr(Register dst, Register src);
  void addptr(Register dst, RegisterOrConstant src) { Unimplemented(); }

  void andptr(Register dst, int32_t src);
  void andptr(Register src1, Register src2) { Unimplemented(); }

  // renamed to drag out the casting of address to int32_t/intptr_t
  void cmp32(Register src1, int32_t imm);

  void cmp32(Register src1, Address src2);

#ifndef _LP64
  void cmpoop(Address dst, jobject obj);
  void cmpoop(Register dst, jobject obj);
#endif // _LP64

  void cmpptr(Register src1, Register src2) { Unimplemented(); }
  void cmpptr(Register src1, Address src2);
  // void cmpptr(Address src1, Register src2) { Unimplemented(); }

  void cmpptr(Register src1, int32_t src2) { Unimplemented(); }
  void cmpptr(Address src1, int32_t src2) { Unimplemented(); }

  void cmpxchgptr(Register oldv, Register newv, Register addr, Register tmp);

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

  // Jumps

  // NOTE: these jumps tranfer to the effective address of dst NOT
  // the address contained by dst. This is because this is more natural
  // for jumps/calls.
  void jump(Address dst);
  void jump_cc(Condition cc, Address dst);

  // Floating

  void fadd_s(Address src)        { Unimplemented(); }

  void fldcw(Address src) { Unimplemented(); }

  void fld_s(int index)   { Unimplemented(); }
  void fld_s(Address src) { Unimplemented(); }

  void fld_d(Address src) { Unimplemented(); }

  void fld_x(Address src) { Unimplemented(); }

  void fmul_s(Address src)        { Unimplemented(); }

  void ldmxcsr(Address src) { Unimplemented(); }

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

#if 0
  void cmov32( Condition cc, Register dst, Address  src);
  void cmov32( Condition cc, Register dst, Register src);

  void cmov(   Condition cc, Register dst, Register src) { Unimplemented(); }

  void cmovptr(Condition cc, Register dst, Address  src) { Unimplemented(); }
  void cmovptr(Condition cc, Register dst, Register src) { Unimplemented(); }

  void movoop(Register dst, jobject obj);
  void movoop(Address dst, jobject obj);

  void movptr(Register dst, Address src);

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


  void pushptr(Address src) { Unimplemented(); }
  void popptr(Address src) { Unimplemented(); }
#endif

  void pushoop(jobject obj);

  // sign extend as need a l to ptr sized element
  void movl2ptr(Register dst, Address src) { Unimplemented(); }
  void movl2ptr(Register dst, Register src) { Unimplemented(); }

  // C2 compiled method's prolog code.
  void verified_entry(int framesize, bool stack_bang, bool fp_mode_24b);

#undef VIRTUAL

  // MacroAssembler routines found actually to be needed

  // Stack push and pop individual 64 bit registers
  void push(Register src);
  void pop(Register dst);

  // push all registers onto the stack
  void pusha();
  void popa();

  void push(unsigned int bitset);
  void pop(unsigned int bitset);

  void repne_scan(Register addr, Register value, Register count,
		  Register scratch);
  void repne_scanw(Register addr, Register value, Register count,
		   Register scratch);

  // Prolog generator routines to support switch between x86 code and
  // generated ARM code

  // routine to generate an x86 prolog for a stub function which
  // bootstraps into the generated ARM code which directly follows the
  // stub
  //
  enum { ret_type_void, ret_type_integral, ret_type_float, ret_type_double};

  void c_stub_prolog(int gp_arg_count, int fp_arg_count, int ret_type);
};

#ifdef ASSERT
inline bool AbstractAssembler::pd_check_instruction_mark() { Unimplemented(); return false; }
#endif

class BiasedLockingCounters;

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

#endif // CPU_AARCH64_VM_ASSEMBLER_AARCH64_HPP
