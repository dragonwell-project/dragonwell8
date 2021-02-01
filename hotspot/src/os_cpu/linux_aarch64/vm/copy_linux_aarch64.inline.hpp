/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_AARCH64_VM_COPY_LINUX_AARCH64_INLINE_HPP
#define OS_CPU_LINUX_AARCH64_VM_COPY_LINUX_AARCH64_INLINE_HPP

#define COPY_SMALL(from, to, count)                                     \
{                                                                       \
        long tmp0, tmp1, tmp2, tmp3;                                    \
        long tmp4, tmp5, tmp6, tmp7;                                    \
  __asm volatile(                                                       \
"       adr     %[t0], 0f;"                                             \
"       add     %[t0], %[t0], %[cnt], lsl #6;"                          \
"       br      %[t0];"                                                 \
"       .align  6;"                                                     \
"0:"                                                                    \
"       b       1f;"                                                    \
"8:"                                                                    \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldp     %[t1], %[t2], [%[s], #8];"                              \
"       ldp     %[t3], %[t4], [%[s], #24];"                             \
"       ldr     %[t5], [%[s], #40];"                                    \
"       tbz     %[d], #3, 10f;"                                         \
"9:"                                                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       stp     %[t1], %[t2], [%[d], #8];"                              \
"       stp     %[t3], %[t4], [%[d], #24];"                             \
"       str     %[t5], [%[d], #40];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       ldr     %[t0], [%[s], #0];"                                     \
"       str     %[t0], [%[d], #0];"                                     \
"       b       1f;"                                                    \
"2:"                                                                    \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldp     %[t1], %[t2], [%[s], #8];"                              \
"       ldp     %[t3], %[t4], [%[s], #24];"                             \
"       ldp     %[t5], %[t6], [%[s], #40];"                             \
"       ldr     %[t7], [%[s], #56];"                                    \
"       tbz     %[d], #3, 4f;"                                          \
"3:"                                                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       stp     %[t1], %[t2], [%[d], #8];"                              \
"       stp     %[t3], %[t4], [%[d], #24];"                             \
"       stp     %[t5], %[t6], [%[d], #40];"                             \
"       str     %[t7], [%[d], #56];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldr     %[t1], [%[s], #8];"                                     \
"       str     %[t0], [%[d], #0];"                                     \
"       str     %[t1], [%[d], #8];"                                     \
"       b       1f;"                                                    \
"5:"                                                                    \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldp     %[t1], %[t2], [%[s], #8];"                              \
"       ldp     %[t3], %[t4], [%[s], #24];"                             \
"       ldp     %[t5], %[t6], [%[s], #40];"                             \
"       tbz     %[d], #3, 7f;"                                          \
"6:"                                                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       stp     %[t1], %[t2], [%[d], #8];"                              \
"       stp     %[t3], %[t4], [%[d], #24];"                             \
"       stp     %[t5], %[t6], [%[d], #40];"                             \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldr     %[t1], [%[s], #8];"                                     \
"       ldr     %[t2], [%[s], #16];"                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       str     %[t1], [%[d], #8];"                                     \
"       str     %[t2], [%[d], #16];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldr     %[t1], [%[s], #8];"                                     \
"       ldr     %[t2], [%[s], #16];"                                    \
"       ldr     %[t3], [%[s], #24];"                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       str     %[t1], [%[d], #8];"                                     \
"       str     %[t2], [%[d], #16];"                                    \
"       str     %[t3], [%[d], #24];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       ldr     %[t0], [%[s], #0];"                                     \
"       ldr     %[t1], [%[s], #8];"                                     \
"       ldr     %[t2], [%[s], #16];"                                    \
"       ldr     %[t3], [%[s], #24];"                                    \
"       ldr     %[t4], [%[s], #32];"                                    \
"       str     %[t0], [%[d], #0];"                                     \
"       str     %[t1], [%[d], #8];"                                     \
"       str     %[t2], [%[d], #16];"                                    \
"       str     %[t3], [%[d], #24];"                                    \
"       str     %[t4], [%[d], #32];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       tbnz    %[s], #3, 8b;"                                          \
"       ldp     %[t0], %[t1], [%[s], #0];"                              \
"       ldp     %[t2], %[t3], [%[s], #16];"                             \
"       ldp     %[t4], %[t5], [%[s], #32];"                             \
"       tbnz    %[d], #3, 9b;"                                          \
"10:"                                                                   \
"       stp     %[t0], %[t1], [%[d], #0];"                              \
"       stp     %[t2], %[t3], [%[d], #16];"                             \
"       stp     %[t4], %[t5], [%[d], #32];"                             \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       tbnz    %[s], #3, 5b;"                                          \
"       ldp     %[t0], %[t1], [%[s], #0];"                              \
"       ldp     %[t2], %[t3], [%[s], #16];"                             \
"       ldp     %[t4], %[t5], [%[s], #32];"                             \
"       ldr     %[t6], [%[s], #48];"                                    \
"       tbnz    %[d], #3, 6b;"                                          \
"7:"                                                                    \
"       stp     %[t0], %[t1], [%[d], #0];"                              \
"       stp     %[t2], %[t3], [%[d], #16];"                             \
"       stp     %[t4], %[t5], [%[d], #32];"                             \
"       str     %[t6], [%[d], #48];"                                    \
"       b       1f;"                                                    \
"       .align  6;"                                                     \
"       tbnz    %[s], #3, 2b;"                                          \
"       ldp     %[t0], %[t1], [%[s], #0];"                              \
"       ldp     %[t2], %[t3], [%[s], #16];"                             \
"       ldp     %[t4], %[t5], [%[s], #32];"                             \
"       ldp     %[t6], %[t7], [%[s], #48];"                             \
"       tbnz    %[d], #3, 3b;"                                          \
"4:"                                                                    \
"       stp     %[t0], %[t1], [%[d], #0];"                              \
"       stp     %[t2], %[t3], [%[d], #16];"                             \
"       stp     %[t4], %[t5], [%[d], #32];"                             \
"       stp     %[t6], %[t7], [%[d], #48];"                             \
"1:"                                                                    \
  : [s]"+r"(from), [d]"+r"(to), [cnt]"+r"(count),                       \
    [t0]"=&r"(tmp0), [t1]"=&r"(tmp1), [t2]"=&r"(tmp2), [t3]"=&r"(tmp3), \
    [t4]"=&r"(tmp4), [t5]"=&r"(tmp5), [t6]"=&r"(tmp6), [t7]"=&r"(tmp7)  \
  :                                                                     \
  : "memory", "cc");                                                    \
}

static void pd_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  __asm volatile( "prfm pldl1strm, [%[s], #0];" :: [s]"r"(from) : "memory");
  if (__builtin_expect(count <= 8, 1)) {
    COPY_SMALL(from, to, count);
    return;
  }
  _Copy_conjoint_words(from, to, count);
}

static void pd_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  if (__builtin_constant_p(count)) {
    memcpy(to, from, count * sizeof(HeapWord));
    return;
  }
  __asm volatile( "prfm pldl1strm, [%[s], #0];" :: [s]"r"(from) : "memory");
  if (__builtin_expect(count <= 8, 1)) {
    COPY_SMALL(from, to, count);
    return;
  }
  _Copy_disjoint_words(from, to, count);
}

static void pd_disjoint_words_atomic(HeapWord* from, HeapWord* to, size_t count) {
  __asm volatile( "prfm pldl1strm, [%[s], #0];" :: [s]"r"(from) : "memory");
  if (__builtin_expect(count <= 8, 1)) {
    COPY_SMALL(from, to, count);
    return;
  }
  _Copy_disjoint_words(from, to, count);
}

static void pd_aligned_conjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  pd_conjoint_words(from, to, count);
}

static void pd_aligned_disjoint_words(HeapWord* from, HeapWord* to, size_t count) {
  pd_disjoint_words(from, to, count);
}

static void pd_conjoint_bytes(void* from, void* to, size_t count) {
  (void)memmove(to, from, count);
}

static void pd_conjoint_bytes_atomic(void* from, void* to, size_t count) {
  pd_conjoint_bytes(from, to, count);
}

static void pd_conjoint_jshorts_atomic(jshort* from, jshort* to, size_t count) {
  _Copy_conjoint_jshorts_atomic(from, to, count);
}

static void pd_conjoint_jints_atomic(jint* from, jint* to, size_t count) {
  _Copy_conjoint_jints_atomic(from, to, count);
}

static void pd_conjoint_jlongs_atomic(jlong* from, jlong* to, size_t count) {
  _Copy_conjoint_jlongs_atomic(from, to, count);
}

static void pd_conjoint_oops_atomic(oop* from, oop* to, size_t count) {
  assert(!UseCompressedOops, "foo!");
  assert(BytesPerLong == BytesPerOop, "jlongs and oops must be the same size");
  _Copy_conjoint_jlongs_atomic((jlong*)from, (jlong*)to, count);
}

static void pd_arrayof_conjoint_bytes(HeapWord* from, HeapWord* to, size_t count) {
  _Copy_arrayof_conjoint_bytes(from, to, count);
}

static void pd_arrayof_conjoint_jshorts(HeapWord* from, HeapWord* to, size_t count) {
  _Copy_arrayof_conjoint_jshorts(from, to, count);
}

static void pd_arrayof_conjoint_jints(HeapWord* from, HeapWord* to, size_t count) {
   _Copy_arrayof_conjoint_jints(from, to, count);
}

static void pd_arrayof_conjoint_jlongs(HeapWord* from, HeapWord* to, size_t count) {
  _Copy_arrayof_conjoint_jlongs(from, to, count);
}

static void pd_arrayof_conjoint_oops(HeapWord* from, HeapWord* to, size_t count) {
  assert(!UseCompressedOops, "foo!");
  assert(BytesPerLong == BytesPerOop, "jlongs and oops must be the same size");
  _Copy_arrayof_conjoint_jlongs(from, to, count);
}

#endif // OS_CPU_LINUX_AARCH64_VM_COPY_LINUX_AARCH64_INLINE_HPP
