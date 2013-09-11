/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1999, 2011, Oracle and/or its affiliates.
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

#include "precompiled.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "classfile/systemDictionary.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/interpreter.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/os.hpp"
#include "runtime/stubRoutines.hpp"

void C1_MacroAssembler::float_cmp(bool is_float, int unordered_result,
				  FloatRegister f0, FloatRegister f1,
				  Register result)
{
  Label done;
  if (is_float) {
    fcmps(f0, f1);
  } else {
    fcmpd(f0, f1);
  }
  if (unordered_result < 0) {
    // we want -1 for unordered or less than, 0 for equal and 1 for
    // greater than.
    mov(result, (u_int64_t)-1L);
    // for FP LT tests less than or unordered
    br(Assembler::LT, done);
    // install 0 for EQ otherwise 1
    csinc(result, zr, zr, Assembler::EQ);
  } else {
    // we want -1 for less than, 0 for equal and 1 for unordered or
    // greater than.
    mov(result, 1L);
    // for FP HI tests greater than or unordered
    br(Assembler::HI, done);
    // install 0 for EQ otherwise ~0
    csinv(result, zr, zr, Assembler::EQ);
  }
  bind(done);
}

int C1_MacroAssembler::lock_object(Register hdr, Register obj, Register disp_hdr, Register scratch, Label& slow_case) {
  const int aligned_mask = BytesPerWord -1;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  Label done, fail;
  int null_check_offset = -1;

  verify_oop(obj);

  // save object being locked into the BasicObjectLock
  str(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));

  if (UseBiasedLocking) {
    assert(scratch != noreg, "should have scratch register at this point");
    null_check_offset = biased_locking_enter(disp_hdr, obj, hdr, scratch, false, done, &slow_case);
  } else {
    null_check_offset = offset();
  }

  // Load object header
  ldr(hdr, Address(obj, hdr_offset));
  // and mark it as unlocked
  orr(hdr, hdr, markOopDesc::unlocked_value);
  // save unlocked object header into the displaced header location on the stack
  str(hdr, Address(disp_hdr, 0));
  // test if object header is still the same (i.e. unlocked), and if so, store the
  // displaced header address in the object header - if it is not the same, get the
  // object header instead
  lea(rscratch2, Address(obj, hdr_offset));
  cmpxchgptr(hdr, disp_hdr, rscratch2, rscratch1, done, fail);
  // if the object header was the same, we're done
  bind(fail);
  // if the object header was not the same, it is now in the hdr register
  // => test if it is a stack pointer into the same stack (recursive locking), i.e.:
  //
  // 1) (hdr & aligned_mask) == 0
  // 2) sp <= hdr
  // 3) hdr <= sp + page_size
  //
  // these 3 tests can be done by evaluating the following expression:
  //
  // (hdr - sp) & (aligned_mask - page_size)
  //
  // assuming both the stack pointer and page_size have their least
  // significant 2 bits cleared and page_size is a power of 2
  mov(rscratch1, sp);
  sub(hdr, hdr, rscratch1);
  ands(hdr, hdr, aligned_mask - os::vm_page_size());
  // for recursive locking, the result is zero => save it in the displaced header
  // location (NULL in the displaced hdr location indicates recursive locking)
  str(hdr, Address(disp_hdr, 0));
  // otherwise we don't care about the result and handle locking via runtime call
  cbnz(hdr, slow_case);
  // done
  bind(done);
  if (PrintBiasedLockingStatistics) {
    addmw(ExternalAddress((address)BiasedLocking::fast_path_entry_count_addr()),
	  1, rscratch1);
  }
  return null_check_offset;
}


void C1_MacroAssembler::unlock_object(Register hdr, Register obj, Register disp_hdr, Label& slow_case) {
  const int aligned_mask = BytesPerWord -1;
  const int hdr_offset = oopDesc::mark_offset_in_bytes();
  assert(hdr != obj && hdr != disp_hdr && obj != disp_hdr, "registers must be different");
  Label done;

  if (UseBiasedLocking) {
    // load object
    ldr(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));
    biased_locking_exit(obj, hdr, done);
  }

  // load displaced header
  ldr(hdr, Address(disp_hdr, 0));
  // if the loaded hdr is NULL we had recursive locking
  // if we had recursive locking, we are done
  cbz(hdr, done);
  if (!UseBiasedLocking) {
    // load object
    ldr(obj, Address(disp_hdr, BasicObjectLock::obj_offset_in_bytes()));
  }
  verify_oop(obj);
  // test if object header is pointing to the displaced header, and if so, restore
  // the displaced header in the object - if the object header is not pointing to
  // the displaced header, get the object header instead
  // if the object header was not pointing to the displaced header,
  // we do unlocking via runtime call
  if (hdr_offset) {
    lea(rscratch1, Address(obj, hdr_offset));
    cmpxchgptr(disp_hdr, hdr, rscratch1, rscratch2, done, slow_case);
  } else {
    cmpxchgptr(disp_hdr, hdr, obj, rscratch2, done, slow_case);
  }
  // done
  bind(done);
}


// Defines obj, preserves var_size_in_bytes
void C1_MacroAssembler::try_allocate(Register obj, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2, Label& slow_case) {
  if (UseTLAB) {
    tlab_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, t2, slow_case);
  } else {
    eden_allocate(obj, var_size_in_bytes, con_size_in_bytes, t1, slow_case);
    incr_allocated_bytes(noreg, var_size_in_bytes, con_size_in_bytes, t1);
  }
}

void C1_MacroAssembler::initialize_header(Register obj, Register klass, Register len, Register t1, Register t2) {
  assert_different_registers(obj, klass, len);
  if (UseBiasedLocking && !len->is_valid()) {
    assert_different_registers(obj, klass, len, t1, t2);
    ldr(t1, Address(klass, Klass::prototype_header_offset()));
  } else {
    // This assumes that all prototype bits fit in an int32_t
    mov(t1, (int32_t)(intptr_t)markOopDesc::prototype());
  }
  str(t1, Address(obj, oopDesc::mark_offset_in_bytes()));

  if (UseCompressedKlassPointers) { // Take care not to kill klass
    encode_klass_not_null(t1, klass);
    strw(t1, Address(obj, oopDesc::klass_offset_in_bytes()));
  } else {
    str(klass, Address(obj, oopDesc::klass_offset_in_bytes()));
  }

  if (len->is_valid()) {
    strw(len, Address(obj, arrayOopDesc::length_offset_in_bytes()));
  } else if (UseCompressedKlassPointers) {
    store_klass_gap(obj, zr);
  }
}

// Zero words; len is in bytes
// Destroys all registers except addr
// len must be a nonzero multiple of wordSize
void C1_MacroAssembler::zero_memory(Register addr, Register len, Register t1) {
  assert_different_registers(addr, len, t1, rscratch1, rscratch2);

#ifdef ASSERT
  { Label L;
    tst(len, BytesPerWord - 1);
    br(Assembler::EQ, L);
    stop("len is not a multiple of BytesPerWord");
    bind(L);
  }
#endif

#ifndef PRODUCT
  block_comment("zero memory");
#endif

  Label finished;

  lsr(len, len, LogBytesPerWord);
  mov(rscratch1, addr);

  // The algorithm first zeroes words until the number of words
  // remaining is a multiple of 8, then enters a loop that writes 8
  // words at a time.  The idea is to get small arrays done quickly
  // because these are the common case.  Also, we don't want to bloat
  // the VM with a lot of code: it's a compromise between speed and
  // size.  We should really emit the large block out of line.

  Label is_even;
  tst(len, 1);
  br(Assembler::EQ, is_even);
  str(zr, Address(post(rscratch1, wordSize)));
  sub(len, len, 1);
  bind(is_even);

  // len is now a multiple of 2

  {
    // Initialize the first few words.  This loop iterates at most 3
    // times.

    Label bottom, loop;
    mov(t1, zr);
    b(bottom);

    bind(loop);
    stp(zr, t1, Address(post(rscratch1, 2 * wordSize)));
    sub(len, len, 2);
    bind(bottom);
    tst(len, 7);
    br(NE, loop);
  }

  // len is now a multiple of 8

  cbz(len, finished);

  Label top;
  bind(top);
  stp(zr, t1, Address(post(rscratch1, 2 * wordSize)));
  stp(zr, t1, Address(post(rscratch1, 2 * wordSize)));
  stp(zr, t1, Address(post(rscratch1, 2 * wordSize)));
  stp(zr, t1, Address(post(rscratch1, 2 * wordSize)));
  sub(len, len, 8);
  cbnz(len, top);

  bind(finished);
}

// preserves obj, destroys len_in_bytes
void C1_MacroAssembler::initialize_body(Register obj, Register len_in_bytes, int hdr_size_in_bytes, Register t1) {
  Label done;
  assert(obj != len_in_bytes && obj != t1 && t1 != len_in_bytes, "registers must be different");
  assert((hdr_size_in_bytes & (BytesPerWord - 1)) == 0, "header size is not a multiple of BytesPerWord");
  Register index = len_in_bytes;
  // index is positive and ptr sized
  subs(index, index, hdr_size_in_bytes);
  br(Assembler::EQ, done);
  // note: for the remaining code to work, index must be a multiple of BytesPerWord
#ifdef ASSERT
  { Label L;
    tst(index, BytesPerWord - 1);
    br(Assembler::EQ, L);
    stop("index is not a multiple of BytesPerWord");
    bind(L);
  }
#endif

  // Preserve obj
  if (hdr_size_in_bytes)
    add(obj, obj, hdr_size_in_bytes);
  zero_memory(obj, index, t1);
  if (hdr_size_in_bytes)
    sub(obj, obj, hdr_size_in_bytes);

  // done
  bind(done);
}


void C1_MacroAssembler::allocate_object(Register obj, Register t1, Register t2, int header_size, int object_size, Register klass, Label& slow_case) {
  assert_different_registers(obj, t1, t2); // XXX really?
  assert(header_size >= 0 && object_size >= header_size, "illegal sizes");

  try_allocate(obj, noreg, object_size * BytesPerWord, t1, t2, slow_case);

  initialize_object(obj, klass, noreg, object_size * HeapWordSize, t1, t2);
}

void C1_MacroAssembler::initialize_object(Register obj, Register klass, Register var_size_in_bytes, int con_size_in_bytes, Register t1, Register t2) {
  assert((con_size_in_bytes & MinObjAlignmentInBytesMask) == 0,
         "con_size_in_bytes is not multiple of alignment");
  const int hdr_size_in_bytes = instanceOopDesc::header_size() * HeapWordSize;

  initialize_header(obj, klass, noreg, t1, t2);

  // clear rest of allocated space
  const Register index = t2;
  const int threshold = 8 * BytesPerWord;   // approximate break even point for code size (see comments below)
  if (var_size_in_bytes != noreg) {
    mov(index, var_size_in_bytes);
    initialize_body(obj, index, hdr_size_in_bytes, t1);
  } else if (con_size_in_bytes <= threshold) {
    // use explicit null stores
    int i = hdr_size_in_bytes;
    if (i < con_size_in_bytes && i % (2 * BytesPerWord)) {
      str(zr, Address(obj, i));
      i += BytesPerWord;
    }
    if (i < con_size_in_bytes) {
      mov(t1, zr);
    }
    for (; i < con_size_in_bytes; i += 2 * BytesPerWord)
      stp(zr, t1, Address(obj, i));
  } else if (con_size_in_bytes > hdr_size_in_bytes) {
    block_comment("zero memory");
    // use loop to null out the fields
    // initialize last object field first if odd number of fields
    int words = (con_size_in_bytes - hdr_size_in_bytes) / BytesPerWord;
    mov(index,  words / 8);
    lea(rscratch1, Address(obj, (con_size_in_bytes & - ((2 * BytesPerWord)-1))));
    // initialize last object field if constant size is odd
    if ((words % 2) != 0) {
      str(zr, Address(obj, con_size_in_bytes - (1*BytesPerWord)));
      words--;
    }
    // initialize remaining object fields
    mov(t1, zr);
    {
      Label top, entry_point;

      int remainder = words % 8;
      if (remainder != 0)
	b(entry_point);

      bind(top);
      sub(index, index, 1);
      stp(zr, t1, pre(rscratch1, -2 * BytesPerWord));
      if (remainder == 6) bind(entry_point);
      stp(zr, t1, pre(rscratch1, -2 * BytesPerWord));
      if (remainder == 4) bind(entry_point);
      stp(zr, t1, pre(rscratch1, -2 * BytesPerWord));
      if (remainder == 2) bind(entry_point);
      stp(zr, t1, pre(rscratch1, -2 * BytesPerWord));

      cbnz(index, top);
    }
  }

  if (CURRENT_ENV->dtrace_alloc_probes()) {
    assert(obj == r0, "must be");
    call(RuntimeAddress(Runtime1::entry_for(Runtime1::dtrace_object_alloc_id)));
  }

  verify_oop(obj);
}
void C1_MacroAssembler::allocate_array(Register obj, Register len, Register t1, Register t2, int header_size, int f, Register klass, Label& slow_case) {
  assert_different_registers(obj, len, t1, t2, klass);

  // determine alignment mask
  assert(!(BytesPerWord & 1), "must be a multiple of 2 for masking code to work");

  // check for negative or excessive length
  mov(rscratch1, (int32_t)max_array_allocation_length);
  cmp(len, rscratch1);
  br(Assembler::HS, slow_case);

  const Register arr_size = t2; // okay to be the same
  // align object end
  mov(arr_size, (int32_t)header_size * BytesPerWord + MinObjAlignmentInBytesMask);
  add(arr_size, arr_size, len, ext::uxtw, f);
  andr(arr_size, arr_size, ~MinObjAlignmentInBytesMask);

  try_allocate(obj, arr_size, 0, t1, t2, slow_case);

  initialize_header(obj, klass, len, t1, t2);

  // clear rest of allocated space
  const Register len_zero = len;
  initialize_body(obj, arr_size, header_size * BytesPerWord, len_zero);

  if (CURRENT_ENV->dtrace_alloc_probes()) {
    assert(obj == r0, "must be");
    bl(RuntimeAddress(Runtime1::entry_for(Runtime1::dtrace_object_alloc_id)));
  }

  verify_oop(obj);
}


void C1_MacroAssembler::inline_cache_check(Register receiver, Register iCache) {
  verify_oop(receiver);
  // explicit NULL check not needed since load from [klass_offset] causes a trap
  // check against inline cache
  assert(!MacroAssembler::needs_explicit_null_check(oopDesc::klass_offset_in_bytes()), "must add explicit null check");
  int start_offset = offset();

  load_klass(rscratch1, receiver);
  cmp(rscratch1, iCache);

  // if icache check fails, then jump to runtime routine
  // Note: RECEIVER must still contain the receiver!
  Label dont;
  br(Assembler::EQ, dont);
  b(RuntimeAddress(SharedRuntime::get_ic_miss_stub()));
  bind(dont);
  const int ic_cmp_size = 4 * 4;
  assert(UseCompressedKlassPointers || offset() - start_offset == ic_cmp_size, "check alignment in emit_method_entry");
}


void C1_MacroAssembler::build_frame(int frame_size_in_bytes) {
  // Make sure there is enough stack space for this method's activation.
  // Note that we do this before doing an enter().
  generate_stack_overflow_check(frame_size_in_bytes);
  enter();
  sub(sp, sp, frame_size_in_bytes); // does not emit code for frame_size == 0
  if (NotifySimulator) {
    notify(Assembler::method_entry);
  }
}


void C1_MacroAssembler::remove_frame(int frame_size_in_bytes) {
  add(sp, sp, frame_size_in_bytes);  // Does not emit code for frame_size == 0
  ldp(rfp, lr, Address(post(sp, 2 * wordSize)));
  if (NotifySimulator) {
    notify(Assembler::method_reentry);
  }
}


void C1_MacroAssembler::unverified_entry(Register receiver, Register ic_klass) { Unimplemented(); }


void C1_MacroAssembler::verified_entry() {
}

#ifndef PRODUCT

void C1_MacroAssembler::verify_stack_oop(int stack_offset) {
  if (!VerifyOops) return;
  verify_oop_addr(Address(sp, stack_offset), "oop");
}

void C1_MacroAssembler::verify_not_null_oop(Register r) {
  if (!VerifyOops) return;
  Label not_null;
  cbnz(r, not_null);
  stop("non-null oop required");
  bind(not_null);
  verify_oop(r);
}

void C1_MacroAssembler::invalidate_registers(bool inv_r0, bool inv_r19, bool inv_r2, bool inv_r3, bool inv_r4, bool inv_r5) {
#ifdef ASSERT
  static int nn;
  if (inv_r0) mov(r0, 0xDEAD);
  if (inv_r19) mov(r19, 0xDEAD);
  if (inv_r2) mov(r2, nn++);
  if (inv_r3) mov(r3, 0xDEAD);
  if (inv_r4) mov(r4, 0xDEAD);
  if (inv_r5) mov(r5, 0xDEAD);
#endif
}
#endif // ifndef PRODUCT
