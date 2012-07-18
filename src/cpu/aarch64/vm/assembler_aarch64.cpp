/*
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates. All rights reserved.
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

#undef TARGET_ARCH_x86
#define TARGET_ARCH_AARCH64
#define CPU_X86_VM_REGISTER_X86_HPP

#include <stdio.h>
#include <sys/types.h>

#include "precompiled.hpp"
// #include "assembler_aarch64.inline.hpp"
#include "asm/assembler.hpp"
#include "assembler_aarch64.hpp"

#include "compiler/disassembler.hpp"

// #include "gc_interface/collectedHeap.inline.hpp"
// #include "interpreter/interpreter.hpp"
// #include "memory/cardTableModRefBS.hpp"
// #include "memory/resourceArea.hpp"
// #include "prims/methodHandles.hpp"
// #include "runtime/biasedLocking.hpp"
// #include "runtime/interfaceSupport.hpp"
// #include "runtime/objectMonitor.hpp"
// #include "runtime/os.hpp"
// #include "runtime/sharedRuntime.hpp"
// #include "runtime/stubRoutines.hpp"
// #ifndef SERIALGC
// #include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
// #include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
// #include "gc_implementation/g1/heapRegion.hpp"
// #endif

REGISTER_DEFINITION(Register, r0);
REGISTER_DEFINITION(Register, r1);
REGISTER_DEFINITION(Register, r2);
REGISTER_DEFINITION(Register, r3);
REGISTER_DEFINITION(Register, r4);
REGISTER_DEFINITION(Register, r5);
REGISTER_DEFINITION(Register, r6);
REGISTER_DEFINITION(Register, r7);
// #if 0 // x86 defines these.  What a kludge!
REGISTER_DEFINITION(Register, r8);
REGISTER_DEFINITION(Register, r9);
REGISTER_DEFINITION(Register, r10);
REGISTER_DEFINITION(Register, r11);
REGISTER_DEFINITION(Register, r12);
REGISTER_DEFINITION(Register, r13);
REGISTER_DEFINITION(Register, r14);
REGISTER_DEFINITION(Register, r15);
// #endif
REGISTER_DEFINITION(Register, r16);
REGISTER_DEFINITION(Register, r17);
REGISTER_DEFINITION(Register, r18);
REGISTER_DEFINITION(Register, r19);
REGISTER_DEFINITION(Register, r20);
REGISTER_DEFINITION(Register, r21);
REGISTER_DEFINITION(Register, r22);
REGISTER_DEFINITION(Register, r23);
REGISTER_DEFINITION(Register, r24);
REGISTER_DEFINITION(Register, r25);
REGISTER_DEFINITION(Register, r26);
REGISTER_DEFINITION(Register, r27);
REGISTER_DEFINITION(Register, r28);
REGISTER_DEFINITION(Register, r29);
REGISTER_DEFINITION(Register, r30);
REGISTER_DEFINITION(Register, sp);


extern "C" void entry(CodeBuffer *cb);

#define __ _masm.

void entry(CodeBuffer *cb) {
  Assembler _masm(cb);
  address entry = __ pc();

  // Every instruction
  __ add(r15, r20, r18, Assembler::asr, 17);
  __ sub(r5, r15, r30, Assembler::asr, 32);
  __ adds(r25, r3, r15, Assembler::asr, 21);
  __ subs(r4, r22, r8, Assembler::lsr, 7);
  __ addw(r2, r15, r11, Assembler::lsl, 17);
  __ subw(r1, r24, r11, Assembler::asr, 1);
  __ addsw(r15, r14, r24, Assembler::asr, 19);
  __ subsw(r22, r7, r23, Assembler::lsr, 5);
  __ andr(r6, r5, r8, Assembler::lsl, 52);
  __ orr(r27, r8, r9, Assembler::asr, 28);
  __ eor(r23, r24, r14, Assembler::lsl, 61);
  __ ands(r20, r5, r25, Assembler::lsr, 15);
  __ andw(r12, r28, r2, Assembler::lsr, 13);
  __ orrw(r18, r10, r25, Assembler::asr, 28);
  __ eorw(r4, r17, r25, Assembler::asr, 13);
  __ andsw(r11, r5, r18, Assembler::lsr, 30);
  __ bic(r5, r15, r17, Assembler::lsr, 60);
  __ orn(r12, r16, r5, Assembler::lsr, 35);
  __ eon(r8, r18, r9, Assembler::lsl, 1);
  __ bics(r14, r4, r16, Assembler::asr, 43);
  __ bicw(r23, r27, r25, Assembler::asr, 28);
  __ ornw(r23, r4, r10, Assembler::lsl, 6);
  __ eonw(r10, r2, r9, Assembler::lsr, 30);
  __ bicsw(r2, r14, r2, Assembler::lsr, 7);
  __ addwi(r22, r29, 933);
  __ addswi(r11, r6, 47);
  __ subwi(r21, r21, 8);
  __ subswi(r17, r4, 894);
  __ addi(r7, r24, 783);
  __ addsi(r19, r18, 400);
  __ subi(r11, r5, 818);
  __ subsi(r24, r17, 985);
  __ b(__ pc());
  __ bl(__ pc());
  __ cbzw(r27, __ pc());
  __ cbnzw(r22, __ pc());
  __ cbz(r5, __ pc());
  __ cbnz(r2, __ pc());
  __ adr(r6, __ pc());
  __ adrp(r24, __ pc());
  __ tbz(r3, 1, __ pc());
  __ tbnz(r20, 8, __ pc());
  __ movnw(r6, 3744, 0);
  __ movzw(r4, 2387, 16);
  __ movkw(r11, 9401, 0);
  __ movn(r29, 17777, 48);
  __ movz(r21, 3936, 0);
  __ movk(r28, 29297, 16);
  __ sbfm(r11, r24, 23, 27);
  __ bfmw(r4, r6, 2, 9);
  __ ubfmw(r8, r12, 31, 0);
  __ sbfm(r5, r7, 1, 3);
  __ bfm(r26, r0, 23, 24);
  __ ubfm(r18, r24, 3, 0);
  __ extrw(r1, r16, r10, 15);
  __ extr(r16, r18, r22, 38);
  __ beq(__ pc());
  __ bne(__ pc());
  __ bhs(__ pc());
  __ bcs(__ pc());
  __ blo(__ pc());
  __ bcc(__ pc());
  __ bmi(__ pc());
  __ bpl(__ pc());
  __ bvs(__ pc());
  __ bvc(__ pc());
  __ bhi(__ pc());
  __ bls(__ pc());
  __ bge(__ pc());
  __ blt(__ pc());
  __ bgt(__ pc());
  __ ble(__ pc());
  __ bal(__ pc());
  __ bnv(__ pc());
  __ svc(26802);
  __ hvc(21467);
  __ smc(18959);
  __ brk(3961);
  __ hlt(25489);
  __ nop();
  __ eret();
  __ drps();
  __ br(r6);
  __ blr(r21);
  __ stxr(r25, r20, r28);
  __ stlxr(r20, r13, r3);
  __ ldxr(r0, r25);
  __ ldaxr(r24, r13);
  __ stlr(r28, r25);
  __ ldar(r21, r19);
  __ stxrw(r14, r28, r30);
  __ stlxrw(r8, r14, r16);
  __ ldxrw(r29, r28);
  __ ldaxrw(r20, r8);
  __ stlrw(r18, r28);
  __ ldarw(r22, r1);
  __ stxrh(r10, r9, r8);
  __ stlxrh(r23, r5, r5);
  __ ldxrh(r29, r13);
  __ ldaxrh(r4, r16);
  __ stlrh(r14, r28);
  __ ldarh(r19, r30);
  __ stxrb(r9, r20, r4);
  __ stlxrb(r13, r16, r29);
  __ ldxrb(r9, r24);
  __ ldaxrb(r12, r20);
  __ stlrb(r21, r10);
  __ ldarb(r23, r0);
  __ ldxp(r27, r25, r5);
  __ ldaxp(r30, r20, r8);
  __ stxp(r23, r0, r7, r9);
  __ stlxp(r1, r3, r11, r5);
  __ ldxpw(r19, r24, r30);
  __ ldaxpw(r10, r6, r27);
  __ stxpw(r12, r3, r20, r19);
  __ stlxpw(r25, r30, r24, r4);
  __ str(r20, Address(r26, -122));
  __ strw(r9, Address(r14, -127));
  __ strb(r10, Address(r13, -8));
  __ strh(r22, Address(r14, 26));
  __ ldr(r25, Address(r5, -215));
  __ ldrw(r0, Address(r8, 20));
  __ ldrb(r9, Address(r9, -15));
  __ ldrh(r18, Address(r14, -16));
  __ ldrsb(r22, Address(r16, -3));
  __ ldrsh(r11, Address(r5, -57));
  __ ldrshw(r6, Address(r26, -37));
  __ ldrsw(r26, Address(r20, -126));
  __ ldrd(F28, Address(r25, -8));
  __ ldrs(F15, Address(r11, -82));
  __ strd(F24, Address(r21, -203));
  __ strs(F10, Address(r5, -57));
  __ str(r14, Address(__ pre(r26, -9)));
  __ strw(r5, Address(__ pre(r25, -75)));
  __ strb(r18, Address(__ pre(r13, -29)));
  __ strh(r13, Address(__ pre(r15, -5)));
  __ ldr(r21, Address(__ pre(r2, 22)));
  __ ldrw(r14, Address(__ pre(r0, -21)));
  __ ldrb(r25, Address(__ pre(r25, -16)));
  __ ldrh(r5, Address(__ pre(r11, 4)));
  __ ldrsb(r4, Address(__ pre(r25, 12)));
  __ ldrsh(r16, Address(__ pre(r8, 14)));
  __ ldrshw(r20, Address(__ pre(r14, 31)));
  __ ldrsw(r23, Address(__ pre(r26, -47)));
  __ ldrd(F29, Address(__ pre(r13, -23)));
  __ ldrs(F7, Address(__ pre(r29, 17)));
  __ strd(F22, Address(__ pre(r0, -202)));
  __ strs(F12, Address(__ pre(r11, -90)));
  __ str(r29, Address(__ post(r11, 33)));
  __ strw(r27, Address(__ post(r26, -38)));
  __ strb(r25, Address(__ post(r15, -15)));
  __ strh(r7, Address(__ post(r27, 29)));
  __ ldr(r30, Address(__ post(r30, -41)));
  __ ldrw(r24, Address(__ post(r25, -2)));
  __ ldrb(r16, Address(__ post(r7, -29)));
  __ ldrh(r25, Address(__ post(r28, -43)));
  __ ldrsb(r24, Address(__ post(r8, 11)));
  __ ldrsh(r21, Address(__ post(r5, 4)));
  __ ldrshw(r19, Address(__ post(r13, -62)));
  __ ldrsw(r27, Address(__ post(r1, -128)));
  __ ldrd(F21, Address(__ post(r6, -11)));
  __ ldrs(F27, Address(__ post(r14, -73)));
  __ strd(F26, Address(__ post(r1, -53)));
  __ strs(F0, Address(__ post(r2, 60)));
  __ str(r2, Address(r19, r2));
  __ strw(r13, Address(r14, r11));
  __ strb(r13, Address(r13, r18));
  __ strh(r23, Address(r0, r5));
  __ ldr(r20, Address(r24, r22));
  __ ldrw(r3, Address(r18, r5));
  __ ldrb(r30, Address(r5, r8));
  __ ldrh(r16, Address(r3, r6));
  __ ldrsb(r24, Address(r15, r22));
  __ ldrsh(r23, Address(r25, r18));
  __ ldrshw(r22, Address(r6, r26));
  __ ldrsw(r6, Address(r0, r0));
  __ ldrd(F4, Address(r19, r8));
  __ ldrs(F18, Address(r8, r15));
  __ strd(F23, Address(r5, r27));
  __ strs(F0, Address(r24, r26));
  __ str(r10, Address(r14, r21));
  __ strw(r8, Address(r21, r30));
  __ strb(r24, Address(r7, r15));
  __ strh(r4, Address(r23, r25));
  __ ldr(r3, Address(r7, r16));
  __ ldrw(r19, Address(r16, r9));
  __ ldrb(r22, Address(r10, r5));
  __ ldrh(r15, Address(r16, r12));
  __ ldrsb(r17, Address(r6, r24));
  __ ldrsh(r2, Address(r7, r9));
  __ ldrshw(r20, Address(r15, r5));
  __ ldrsw(r10, Address(r24, r19));
  __ ldrd(F19, Address(r25, r24));
  __ ldrs(F22, Address(r18, r18));
  __ strd(F19, Address(r21, r28));
  __ strs(F11, Address(r26, r12));
  __ ldr(r30, __ pc());
  __ ldrw(r27, __ pc());
  __ prfm(Address(r16, -96));
  __ prfm(__ pc());
  __ prfm(Address(r8, r6));
  __ prfm(Address(r11, r8));
  __ adcw(r12, r4, r3);
  __ adcsw(r20, r8, r5);
  __ sbcw(r22, r3, r21);
  __ sbcsw(r1, r20, r7);
  __ adc(r22, r1, r30);
  __ adcs(r6, r16, r12);
  __ sbc(r16, r15, r23);
  __ sbcs(r28, r10, r25);
  __ addw(r10, r7, r20, ext::uxtb, 1);
  __ addsw(r24, r14, r23, ext::sxtb, 3);
  __ sub(r16, r19, r0, ext::sxtb, 3);
  __ subsw(r30, r3, r14, ext::sxtw, 1);
  __ add(r7, r1, r14, ext::sxtw, 3);
  __ adds(r8, r2, r26, ext::uxtx, 1);
  __ sub(r2, r22, r29, ext::uxtx, 1);
  __ subs(r7, r5, r21, ext::sxtx, 4);
  __ ccmnw(r0, r19, 15, Assembler::LT);
  __ ccmpw(r8, r15, 11, Assembler::HI);
  __ ccmn(r10, r19, 10, Assembler::PL);
  __ ccmp(r23, r23, 0, Assembler::CC);
  __ ccmnw(r21, 16, 11, Assembler::HS);
  __ ccmpw(r15, 1, 12, Assembler::PL);
  __ ccmn(r17, 19, 4, Assembler::MI);
  __ ccmp(r20, 12, 1, Assembler::GE);
  __ cselw(r7, r13, r30, Assembler::GT);
  __ csincw(r17, r24, r2, Assembler::GE);
  __ csinvw(r30, r21, r10, Assembler::CC);
  __ csnegw(r20, r29, r26, Assembler::CC);
  __ csel(r10, r29, r28, Assembler::LT);
  __ csinc(r2, r13, r14, Assembler::VC);
  __ csinv(r29, r15, r4, Assembler::LT);
  __ csneg(r11, r10, r27, Assembler::CS);
  __ rbitw(r15, r26);
  __ rev16w(r12, r25);
  __ revw(r26, r11);
  __ clzw(r15, r5);
  __ clsw(r11, r1);
  __ rbit(r7, r18);
  __ rev16(r19, r24);
  __ rev32(r19, r13);
  __ rev(r0, r0);
  __ clz(r13, r30);
  __ cls(r24, r18);
  __ udivw(r9, r23, r20);
  __ sdivw(r12, r16, r0);
  __ lslvw(r8, r20, r22);
  __ lsrvw(r11, r24, r18);
  __ asrvw(r21, r14, r30);
  __ rorvw(r10, r9, r22);
  __ udiv(r1, r12, r3);
  __ sdiv(r9, r29, r27);
  __ lslv(r6, r30, r17);
  __ lsrv(r30, r14, r1);
  __ asrv(r18, r0, r21);
  __ rorv(r21, r7, r10);
  __ maddw(r0, r5, r24, r4);
  __ msubw(r30, r15, r18, r1);
  __ madd(r19, r16, r5, r5);
  __ msub(r26, r24, r24, r4);
  __ smaddl(r3, r27, r15, r14);
  __ smsubl(r11, r3, r22, r27);
  __ umaddl(r11, r24, r26, r29);
  __ umsubl(r9, r18, r9, r11);
  __ fmuls(F27, F17, F29);
  __ fdivs(F15, F25, F1);
  __ fadds(F22, F24, F10);
  __ fsubs(F25, F8, F26);
  __ fmuls(F21, F22, F25);
  __ fmuld(F24, F14, F2);
  __ fdivd(F0, F24, F11);
  __ faddd(F5, F9, F7);
  __ fsubd(F17, F10, F5);
  __ fmuld(F30, F23, F3);
  __ fmadds(F2, F4, F15, F15);
  __ fmsubs(F29, F24, F20, F1);
  __ fnmadds(F20, F3, F22, F17);
  __ fnmadds(F21, F0, F2, F18);
  __ fmaddd(F25, F14, F9, F29);
  __ fmsubd(F29, F30, F2, F30);
  __ fnmaddd(F29, F9, F11, F22);
  __ fnmaddd(F9, F1, F21, F21);
  __ fmovs(F24, F8);
  __ fabss(F0, F14);
  __ fnegs(F17, F25);
  __ fsqrts(F2, F24);
  __ fcvts(F30, F28);
  __ fmovd(F17, F20);
  __ fabsd(F7, F0);
  __ fnegd(F30, F18);
  __ fsqrtd(F24, F11);
  __ fcvtd(F5, F30);
  __ fcvtzsw(r15, F3);
  __ fcvtszd(r4, F25);
  __ fcvtzdw(r29, F1);
  __ fcvtzsd(r28, F23);
  __ fmovs(r12, F0);
  __ fmovd(r0, F17);
  __ fmovs(F27, r7);
  __ fmovd(F6, r28);
  __ fcmps(F21, F3);
  __ fcmpd(F12, F22);
  __ fcmps(F8, 0.0);
  __ fcmpd(F24, 0.0);

  Disassembler::decode(entry, __ pc());


}

#define gas_assert(ARG1) assert(ARG1, #ARG1)

// ------------- Stolen from binutils begin -------------------------------------

/* Build the accepted values for immediate logical SIMD instructions.
 *
 * The valid encodings of the immediate value are:
 *   opc<0> j:jjjjj  i:iii:ii  SIMD size  R             S
 *   1      ssssss   rrrrrr       64      UInt(rrrrrr)  UInt(ssssss)
 *   0      0sssss   0rrrrr       32      UInt(rrrrr)   UInt(sssss)
 *   0      10ssss   00rrrr       16      UInt(rrrr)    UInt(ssss)
 *   0      110sss   000rrr       8       UInt(rrr)     UInt(sss)
 *   0      1110ss   0000rr       4       UInt(rr)      UInt(ss)
 *   0      11110s   00000r       2       UInt(r)       UInt(s)
 *   other combinations                   UNPREDICTABLE
 *
 * Let's call E the SIMD size.
 *
 * The immediate value is: S+1 bits '1' rotated to the right by R.
 *
 * The total of valid encodings is 64^2 + 32^2 + ... + 2^2 = 5460.
 *
 * This means we have the following number of distinct values:
 *   - for S = E - 1, all values of R generate a word full of '1'
 *      so we have 2 + 4 + ... + 64 = 126 ways of encoding 0xf...f
 *   - for S != E - 1, all value are obviously distinct
 *      so we have #{ for all E: (E - 1) * R (where R = E) } values
 *        = 64*63 + 32*31 + ... + 2*1 = 5334
 *   - it is obvious that for two different values of E, if S != E - 1
 *      then we can't generate the same value.
 * So the total number of distinct values is 5334 + 1 = 5335 (out of
 * a total of 5460 valid encodings).
 */
#define TOTAL_IMM_NB  5334

typedef struct {
  uint64_t imm;
  uint32_t encoding;
} simd_imm_encoding_v2;

static simd_imm_encoding_v2 simd_immediates_v2[TOTAL_IMM_NB];

static int
simd_imm_encoding_cmp_v2(const void *i1, const void *i2)
{
  const simd_imm_encoding_v2 *imm1 = (const simd_imm_encoding_v2 *)i1;
  const simd_imm_encoding_v2 *imm2 = (const simd_imm_encoding_v2 *)i2;

  if (imm1->imm < imm2->imm)
    return -1;
  if (imm1->imm > imm2->imm)
    return +1;
  return 0;
}

/* immediate bitfield encoding
 * imm13<12> imm13<5:0> imm13<11:6> SIMD size R      S
 * 1         ssssss     rrrrrr      64        rrrrrr ssssss
 * 0         0sssss     0rrrrr      32        rrrrr  sssss
 * 0         10ssss     00rrrr      16        rrrr   ssss
 * 0         110sss     000rrr      8         rrr    sss
 * 0         1110ss     0000rr      4         rr     ss
 * 0         11110s     00000r      2         r      s
 */
static inline int encode_immediate_bitfield(int is64, uint32_t s, uint32_t r)
{
  return (is64 << 12) | (r << 6) | s;
}

static void
build_immediate_table_v2(void) __attribute__ ((constructor));

static void
build_immediate_table_v2(void)
{
  uint32_t log_e, e, s, r, s_mask;
  uint64_t mask, imm;
  int nb_imms;
  int is64;

  nb_imms = 0;
  for (log_e = 1; log_e <= 6; log_e++) {
    e = 1u << log_e;
    if (log_e == 6) {
      is64 = 1;
      mask = 0xffffffffffffffffull;
      s_mask = 0;
    } else {
      is64 = 0;
      mask = (1ull << e) - 1;
      /* log_e  s_mask
       *  1     ((1 << 4) - 1) << 2 = 111100
       *  2     ((1 << 3) - 1) << 3 = 111000
       *  3     ((1 << 2) - 1) << 4 = 110000
       *  4     ((1 << 1) - 1) << 5 = 100000
       *  5     ((1 << 0) - 1) << 6 = 000000
       */
      s_mask = ((1u << (5 - log_e)) - 1) << (log_e + 1);
    }
    for (s = 0; s < e - 1; s++) {
      for (r = 0; r < e; r++) {
        /* s+1 consecutive bits to 1 (s < 63) */
        imm = (1ull << (s + 1)) - 1;
        /* rotate right by r */
        if (r != 0)
          imm = (imm >> r) | ((imm << (e - r)) & mask);
        /* replicate the constant depending on SIMD size */
        switch (log_e) {
        case 1: imm = (imm <<  2) | imm;
        case 2: imm = (imm <<  4) | imm;
        case 3: imm = (imm <<  8) | imm;
        case 4: imm = (imm << 16) | imm;
        case 5: imm = (imm << 32) | imm;
        case 6:
          break;
        default:
          abort ();
        }
        simd_immediates_v2[nb_imms].imm = imm;
        simd_immediates_v2[nb_imms].encoding =
          encode_immediate_bitfield(is64, s | s_mask, r);
        nb_imms++;
      }
    }
  }
  gas_assert(nb_imms == TOTAL_IMM_NB);
  qsort(simd_immediates_v2, nb_imms,
        sizeof(simd_immediates_v2[0]), simd_imm_encoding_cmp_v2);
}

/* Create a valid encoding for imm.  Returns ffffffff since it's an invalid
   encoding.  */
uint32_t
asm_util::encode_immediate_v2(int is32, uint64_t imm)
{
  simd_imm_encoding_v2 imm_enc;
  const simd_imm_encoding_v2 *imm_encoding;

  if (is32) {
    /* Allow all zeros or all ones in top 32-bits, so that
       constant expressions like ~1 are permitted. */
    if (imm >> 32 != 0 && imm >> 32 != 0xffffffff)
      return 0xffffffff;
    /* Replicate the 32 lower bits to the 32 upper bits.  */
    imm &= 0xffffffff;
    imm |= imm << 32;
  }

  imm_enc.imm = imm;
  imm_encoding = (const simd_imm_encoding_v2 *)
    bsearch(&imm_enc, simd_immediates_v2, TOTAL_IMM_NB,
            sizeof(simd_immediates_v2[0]), simd_imm_encoding_cmp_v2);
  if (imm_encoding == NULL)
    return 0xffffffff;
  return imm_encoding->encoding;
}

// ------------- Stolen from binutils end -------------------------------------

bool Assembler::operand_valid_for_logical_immdiate(int is32, uint64_t imm) {
  return encode_immediate_v2(is32, imm) != 0xffffffff;
}

int AbstractAssembler::code_fill_byte() { Unimplemented(); }

// added to make this compile

REGISTER_DEFINITION(Register, noreg);

void MacroAssembler::call_VM_base(Register oop_result,
				  Register java_thread,
				  Register last_java_sp,
				  address  entry_point,
				  int      number_of_arguments,
				  bool     check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             bool check_exceptions) { Unimplemented(); }


void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
			     Register last_java_sp,
			     address entry_point,
			     int number_of_arguments,
			     bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
			     bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             bool check_exceptions) { Unimplemented(); }

void MacroAssembler::call_VM(Register oop_result,
                             Register last_java_sp,
                             address entry_point,
                             Register arg_1,
                             Register arg_2,
                             Register arg_3,
                             bool check_exceptions) { Unimplemented(); }

void MacroAssembler::check_and_handle_earlyret(Register java_thread) {Unimplemented(); }

void MacroAssembler::align(int modulus) { Unimplemented();}

void MacroAssembler::check_and_handle_popframe(Register java_thread) { Unimplemented(); }

RegisterOrConstant MacroAssembler::delayed_value_impl(intptr_t* delayed_value_addr,
                                                      Register tmp,
                                                      int offset) { Unimplemented(); return RegisterOrConstant(r0); }

void MacroAssembler::verify_oop(Register reg, const char* s) { Unimplemented(); }

void MacroAssembler::stop(const char* msg) { Unimplemented(); }

void MacroAssembler::call_VM_leaf_base(address entry_point, int num_args) { Unimplemented(); }

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0) { Unimplemented(); }

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1) { Unimplemented(); }

void MacroAssembler::call_VM_leaf(address entry_point, Register arg_0, Register arg_1, Register arg_2) { Unimplemented(); }

void MacroAssembler::null_check(Register reg, int offset) { Unimplemented(); }

// routine to generate an x86 prolog for a stub function which
// bootstraps into the generated ARM code which directly follows the
// stub
//
// the argument encodes the number of general and fp registers
// passed by the caller and the callng convention (currently just
// the number of general registers and assumes C argument passing)

extern "C" {
int aarch64_stub_prolog_size();
void aarch64_stub_prolog();
void setup_arm_sim(void *sp, int calltype);
}

void MacroAssembler::c_stub_prolog(u_int64_t calltype)
{
  // the addresses for the x86 to ARM entry code we need to use
  address start = pc();
  // printf("start = %lx\n", start);
  int byteCount =  aarch64_stub_prolog_size();
  // printf("byteCount = %x\n", byteCount);
  int instructionCount = (byteCount + 3)/ 4;
  // printf("instructionCount = %x\n", instructionCount);
  for (int i = 0; i < instructionCount; i++) {
    nop();
  }

  memcpy(start, (void*)aarch64_stub_prolog, byteCount);

  // write the address of the setup routine and the call format at the
  // end of into the copied code
  u_int64_t *patch_end = (u_int64_t *)(start + byteCount);
  patch_end[-2] = (u_int64_t)setup_arm_sim;
  patch_end[-1] = calltype;
}
