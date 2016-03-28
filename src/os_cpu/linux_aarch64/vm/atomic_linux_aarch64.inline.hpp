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

#ifndef OS_CPU_LINUX_AARCH64_VM_ATOMIC_LINUX_AARCH64_INLINE_HPP
#define OS_CPU_LINUX_AARCH64_VM_ATOMIC_LINUX_AARCH64_INLINE_HPP

#include "runtime/atomic.hpp"
#include "runtime/os.hpp"
#include "vm_version_aarch64.hpp"

// Implementation of class atomic

#define FULL_MEM_BARRIER  __sync_synchronize()
#define READ_MEM_BARRIER  __atomic_thread_fence(__ATOMIC_ACQUIRE);
#define WRITE_MEM_BARRIER __atomic_thread_fence(__ATOMIC_RELEASE);

// CASALW w2, w0, [x1]
#define CASALW          ".word 0b10001000111000101111110000100000;"
// CASAL x2, x0, [x1]
#define CASAL           ".word 0b11001000111000101111110000100000;"
// LDADDALW w0, w2, [x1]
#define LDADDALW        ".word 0b10111000111000000000000000100010;"
// LDADDAL w0, w2, [x1]
#define LDADDAL         ".word 0b11111000111000000000000000100010;"
// SWPW w0, w2, [x1]
#define SWPW            ".word 0b10111000001000001000000000100010;"
// SWP x0, x2, [x1]
#define SWP             ".word 0b11111000001000001000000000100010;"

inline void Atomic::store    (jbyte    store_value, jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, jint*     dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, void*     dest) { *(void**)dest = store_value; }

inline void Atomic::store    (jbyte    store_value, volatile jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, volatile jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, volatile jint*     dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, volatile intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, volatile void*     dest) { *(void* volatile *)dest = store_value; }


inline jint Atomic::add(jint add_value, volatile jint* dest)
{
 if (UseLSE) {
   register jint r_add_value asm("w0") = add_value;
   register volatile jint *r_dest asm("x1") = dest;
   register jint r_result asm("w2");
   __asm volatile(LDADDALW
                  : [_result]"=r"(r_result)
                  : [_add_value]"r"(r_add_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   return r_result+add_value;
 }
 return __sync_add_and_fetch(dest, add_value);
}

inline void Atomic::inc(volatile jint* dest)
{
 add(1, dest);
}

inline void Atomic::inc_ptr(volatile void* dest)
{
 add_ptr(1, dest);
}

inline void Atomic::dec (volatile jint* dest)
{
 add(-1, dest);
}

inline void Atomic::dec_ptr(volatile void* dest)
{
 add_ptr(-1, dest);
}

inline jint Atomic::xchg (jint exchange_value, volatile jint* dest)
{
  if (UseLSE) {
   register jint r_exchange_value asm("w0") = exchange_value;
   register volatile jint *r_dest asm("x1") = dest;
   register jint r_result asm("w2");
   __asm volatile(SWPW
                  : [_result]"=r"(r_result)
                  : [_exchange_value]"r"(r_exchange_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   FULL_MEM_BARRIER;
   return r_result;
  }
  jint res = __sync_lock_test_and_set (dest, exchange_value);
  FULL_MEM_BARRIER;
  return res;
}

inline void* Atomic::xchg_ptr(void* exchange_value, volatile void* dest)
{
  return (void *) xchg_ptr((intptr_t) exchange_value,
                           (volatile intptr_t*) dest);
}

inline jint Atomic::cmpxchg (jint exchange_value, volatile jint* dest, jint compare_value)
{
 if (UseLSE) {
   register jint r_exchange_value asm("w0") = exchange_value;
   register volatile jint *r_dest asm("x1") = dest;
   register jint r_compare_value asm("w2") = compare_value;
   __asm volatile(CASALW
                  : [_compare_value]"+r"(r_compare_value)
                  : [_exchange_value]"r"(r_exchange_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   return r_compare_value;
 }
 return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
}

inline void Atomic::store (jlong store_value, jlong* dest) { *dest = store_value; }
inline void Atomic::store (jlong store_value, volatile jlong* dest) { *dest = store_value; }

inline intptr_t Atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest)
{
 if (UseLSE) {
   register intptr_t r_add_value asm("x0") = add_value;
   register volatile intptr_t *r_dest asm("x1") = dest;
   register intptr_t r_result asm("x2");
   __asm volatile(LDADDAL
                  : [_result]"=r"(r_result)
                  : [_add_value]"r"(r_add_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   return r_result+add_value;
 }
 return __sync_add_and_fetch(dest, add_value);
}

inline void* Atomic::add_ptr(intptr_t add_value, volatile void* dest)
{
  return (void *) add_ptr(add_value, (volatile intptr_t *) dest);
}

inline void Atomic::inc_ptr(volatile intptr_t* dest)
{
 add_ptr(1, dest);
}

inline void Atomic::dec_ptr(volatile intptr_t* dest)
{
 add_ptr(-1, dest);
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value, volatile intptr_t* dest)
{
  if (UseLSE) {
   register intptr_t r_exchange_value asm("x0") = exchange_value;
   register volatile intptr_t *r_dest asm("x1") = dest;
   register intptr_t r_result asm("x2");
   __asm volatile(SWP
                  : [_result]"=r"(r_result)
                  : [_exchange_value]"r"(r_exchange_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   FULL_MEM_BARRIER;
   return r_result;
  }
  intptr_t res = __sync_lock_test_and_set (dest, exchange_value);
  FULL_MEM_BARRIER;
  return res;
}

inline jlong Atomic::cmpxchg (jlong exchange_value, volatile jlong* dest, jlong compare_value)
{
 if (UseLSE) {
   register jlong r_exchange_value asm("x0") = exchange_value;
   register volatile jlong *r_dest asm("x1") = dest;
   register jlong r_compare_value asm("x2") = compare_value;
   __asm volatile(CASAL
                  : [_compare_value]"+r"(r_compare_value)
                  : [_exchange_value]"r"(r_exchange_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   return r_compare_value;
 }
 return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value, volatile intptr_t* dest, intptr_t compare_value)
{
 if (UseLSE) {
   register intptr_t r_exchange_value asm("x0") = exchange_value;
   register volatile intptr_t *r_dest asm("x1") = dest;
   register intptr_t r_compare_value asm("x2") = compare_value;
   __asm volatile(CASAL
                  : [_compare_value]"+r"(r_compare_value)
                  : [_exchange_value]"r"(r_exchange_value),
                    [_dest]"r"(r_dest)
                  : "memory");
   return r_compare_value;
 }
 return __sync_val_compare_and_swap(dest, compare_value, exchange_value);
}

inline void* Atomic::cmpxchg_ptr(void* exchange_value, volatile void* dest, void* compare_value)
{
  return (void *) cmpxchg_ptr((intptr_t) exchange_value,
                              (volatile intptr_t*) dest,
                              (intptr_t) compare_value);
}

inline jlong Atomic::load(volatile jlong* src) { return *src; }

#endif // OS_CPU_LINUX_AARCH64_VM_ATOMIC_LINUX_AARCH64_INLINE_HPP
