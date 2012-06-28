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
#if 0 // x86 defines these.  What a kludge!
REGISTER_DEFINITION(Register, r8);
REGISTER_DEFINITION(Register, r9);
REGISTER_DEFINITION(Register, r10);
REGISTER_DEFINITION(Register, r11);
REGISTER_DEFINITION(Register, r12);
REGISTER_DEFINITION(Register, r13);
REGISTER_DEFINITION(Register, r14);
REGISTER_DEFINITION(Register, r15);
#endif
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
  Assembler_aarch64 _masm(cb);
  address entry = __ pc();

  __ addwi(r0, r1, 99);
  __ addswi(r0, r1, 4);

  __ adr(r2, entry);
  __ orri(r0, r12, 0x780000);
  
  __ movn(r3, 0x77);

  __ extr(r0, r1, r2, 12);
  __ extrw(r9, r10, r11, 19);

  __ bl(entry);

  __ cbz(r29, entry);
  __ cbnzw(r23, entry);

  __ tbz(r19, 30, entry);
  __ tbnz(r19, 60, entry);

  __ beq(entry);
  __ ble(entry);
  __ bcc(entry);

  __ hlt(4);
  __ nop();

  __ ret(r7);
  __ eret();

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

bool Assembler_aarch64::operand_valid_for_logical_immdiate(int is32, uint64_t imm) {
  return encode_immediate_v2(is32, imm) != 0xffffffff;
}

