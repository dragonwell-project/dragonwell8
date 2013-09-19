/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates.
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
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interpreter.hpp"
#include "nativeInst_aarch64.hpp"
#include "oops/instanceOop.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/top.hpp"
#ifdef COMPILER2
#include "opto/runtime.hpp"
#endif

// Declaration and definition of StubGenerator (no .hpp file).
// For a more detailed description of the stub routine structure
// see the comment in stubRoutines.hpp

#undef __
#define __ _masm->
#define TIMES_OOP Address::sxtw(exact_log2(UseCompressedOops ? 4 : 8))

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#endif

#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

// Stub Code definitions

#if 0
static address handle_unsafe_access() { Unimplemented(); return 0; }
#endif

class StubGenerator: public StubCodeGenerator {
 private:

#ifdef PRODUCT
#define inc_counter_np(counter) (0)
#else
  void inc_counter_np_(int& counter) {
    __ lea(rscratch2, ExternalAddress((address)&counter));
    __ ldrw(rscratch1, Address(rscratch2));
    __ addw(rscratch1, rscratch1, 1);
    __ strw(rscratch1, Address(rscratch2));
  }
#define inc_counter_np(counter) \
  BLOCK_COMMENT("inc_counter " #counter); \
  inc_counter_np_(counter);
#endif

  // Call stubs are used to call Java from C
  //
  // Arguments:
  //    c_rarg0:   call wrapper address                   address
  //    c_rarg1:   result                                 address
  //    c_rarg2:   result type                            BasicType
  //    c_rarg3:   method                                 Method*
  //    c_rarg4:   (interpreter) entry point              address
  //    c_rarg5:   parameters                             intptr_t*
  //    c_rarg6:   parameter size (in words)              int
  //    c_rarg7:   thread                                 Thread*
  //
  // There is no return from the stub itself as any Java result
  // is written to result
  //
  // we save r30 (lr) as the return PC at the base of the frame and
  // link r29 (fp) below it as the frame pointer installing sp (r31)
  // into fp.
  //
  // we save r0-r7, which accounts for all the c arguments.
  //
  // TODO: strictly do we need to save them all? they are treated as
  // volatile by C so could we omit saving the ones we are going to
  // place in global registers (thread? method?) or those we only use
  // during setup of the Java call?
  //
  // we don't need to save r8 which C uses as an indirect result location
  // return register.
  //
  // we don't need to save r9-r15 which both C and Java treat as
  // volatile
  //
  // we don't need to save r16-18 because Java does not use them
  //
  // we save r19-r28 which Java uses as scratch registers and C
  // expects to be callee-save
  //
  // we don't save any FP registers since only v8-v15 are callee-save
  // (strictly only the f and d components) and Java uses them as
  // callee-save. v0-v7 are arg registers and C treats v16-v31 as
  // volatile (as does Java?)
  //
  // so the stub frame looks like this when we enter Java code
  //
  //     [ return_from_Java     ] <--- sp
  //     [ argument word n      ]
  //      ...
  // -19 [ argument word 1      ]
  // -18 [ saved r28            ] <--- sp_after_call
  // -17 [ saved r27            ]
  // -16 [ saved r26            ]
  // -15 [ saved r25            ]
  // -14 [ saved r24            ]
  // -13 [ saved r23            ]
  // -12 [ saved r22            ]
  // -11 [ saved r21            ]
  // -10 [ saved r20            ]
  //  -9 [ saved r19            ]
  //  -8 [ call wrapper    (r0) ]
  //  -7 [ result          (r1) ]
  //  -6 [ result type     (r2) ]
  //  -5 [ method          (r3) ]
  //  -4 [ entry point     (r4) ]
  //  -3 [ parameters      (r5) ]
  //  -2 [ parameter size  (r6) ]
  //  -1 [ thread (r7)          ]
  //   0 [ saved fp       (r29) ] <--- fp == saved sp (r31)
  //   1 [ saved lr       (r30) ]

  // Call stub stack layout word offsets from fp
  enum call_stub_layout {
    sp_after_call_off = -18,
    r28_off            = -18,
    r27_off            = -17,
    r26_off            = -16,
    r25_off            = -15,
    r24_off            = -14,
    r23_off            = -13,
    r22_off            = -12,
    r21_off            = -11,
    r20_off            = -10,
    r19_off            =  -9,
    call_wrapper_off   =  -8,
    result_off         =  -7,
    result_type_off    =  -6,
    method_off         =  -5,
    entry_point_off    =  -4,
    parameters_off     =  -3,
    parameter_size_off =  -2,
    thread_off         =  -1,
    fp_f               =   0,
    retaddr_off        =   1,
  };

  address generate_call_stub(address& return_address) {
    assert((int)frame::entry_frame_after_call_words == -(int)sp_after_call_off + 1 &&
           (int)frame::entry_frame_call_wrapper_offset == (int)call_wrapper_off,
           "adjust this code");

    StubCodeMark mark(this, "StubRoutines", "call_stub");
    address start = __ pc();

    const Address sp_after_call(rfp, sp_after_call_off * wordSize);

    const Address call_wrapper  (rfp, call_wrapper_off   * wordSize);
    const Address result        (rfp, result_off         * wordSize);
    const Address result_type   (rfp, result_type_off    * wordSize);
    const Address method        (rfp, method_off         * wordSize);
    const Address entry_point   (rfp, entry_point_off    * wordSize);
    const Address parameters    (rfp, parameters_off     * wordSize);
    const Address parameter_size(rfp, parameter_size_off * wordSize);

    const Address thread        (rfp, thread_off         * wordSize);

    const Address r28_save      (rfp, r28_off * wordSize);
    const Address r27_save      (rfp, r27_off * wordSize);
    const Address r26_save      (rfp, r26_off * wordSize);
    const Address r25_save      (rfp, r25_off * wordSize);
    const Address r24_save      (rfp, r24_off * wordSize);
    const Address r23_save      (rfp, r23_off * wordSize);
    const Address r22_save      (rfp, r22_off * wordSize);
    const Address r21_save      (rfp, r21_off * wordSize);
    const Address r20_save      (rfp, r20_off * wordSize);
    const Address r19_save      (rfp, r19_off * wordSize);

    // stub code

#ifdef BUILTIN_SIM
    // we need a C prolog to bootstrap the x86 caller into the sim

    __ c_stub_prolog(8, 0, MacroAssembler::ret_type_void);
#endif

    address aarch64_entry = __ pc();

// AED: this should fix Ed's problem -- we only save the sender's SP for our sim
#ifdef BUILTIN_SIM
    // Save sender's SP for stack traces.
// ECN: FIXME
// #if 0
    __ mov(rscratch1, sp);
    __ str(rscratch1, Address(__ pre(sp, -2 * wordSize)));
#endif
    // set up frame and move sp to end of save area
    __ enter();
    __ sub(sp, rfp, -sp_after_call_off * wordSize);

    // save register parameters and Java scratch/global registers
    // n.b. we save thread even though it gets installed in
    // rthread because we want to sanity check rthread later
    __ str(c_rarg7,  thread);
    __ strw(c_rarg6, parameter_size);
    __ str(c_rarg5,  parameters);
    __ str(c_rarg4,  entry_point);
    __ str(c_rarg3,  method);
    __ str(c_rarg2,  result_type);
    __ str(c_rarg1,  result);
    __ str(c_rarg0,  call_wrapper);
    __ str(r19,      r19_save);
    __ str(r20,      r20_save);
    __ str(r21,      r21_save);
    __ str(r22,      r22_save);
    __ str(r23,      r23_save);
    __ str(r24,      r24_save);
    __ str(r25,      r25_save);
    __ str(r26,      r26_save);
    __ str(r27,      r27_save);
    __ str(r28,      r28_save);

    // install Java thread in global register now we have saved
    // whatever value it held
    __ mov(rthread, c_rarg7);
    // And method
    __ mov(rmethod, c_rarg3);

    // set up the heapbase register
    __ reinit_heapbase();

#ifdef ASSERT
    // make sure we have no pending exceptions
    {
      Label L;
      __ ldr(rscratch1, Address(rthread, in_bytes(Thread::pending_exception_offset())));
      __ cmp(rscratch1, (unsigned)NULL_WORD);
      __ br(Assembler::EQ, L);
      __ stop("StubRoutines::call_stub: entered with pending exception");
      __ BIND(L);
    }
#endif
    // pass parameters if any
    __ mov(esp, sp);
    __ sub(sp, sp, os::vm_page_size()); // Move SP out of the way

    BLOCK_COMMENT("pass parameters if any");
    Label parameters_done;
    // parameter count is still in c_rarg6
    // and parameter pointer identifying param 1 is in c_rarg5
    __ cbz(c_rarg6, parameters_done);

    address loop = __ pc();
    __ ldr(rscratch1, Address(__ post(c_rarg5, wordSize)));
    __ subsw(c_rarg6, c_rarg6, 1);
    __ push(rscratch1);
    __ br(Assembler::GT, loop);

    __ BIND(parameters_done);

    // call Java entry -- passing methdoOop, and current sp
    //      rmethod: Method*
    //      r13: sender sp
    BLOCK_COMMENT("call Java function");
    __ mov(r13, sp);
    __ blr(c_rarg4);

    // tell the simulator we have returned to the stub

    // we do this here because the notify will already have been done
    // if we get to the next instruction via an exception
    //
    // n.b. adding this instruction here affects the calculation of
    // whether or not a routine returns to the call stub (used when
    // doing stack walks) since the normal test is to check the return
    // pc against the address saved below. so we may need to allow for
    // this extra instruction in the check.

    if (NotifySimulator) {
      __ notify(Assembler::method_reentry);
    }
    // save current address for use by exception handling code

    return_address = __ pc();

    // store result depending on type (everything that is not
    // T_OBJECT, T_LONG, T_FLOAT or T_DOUBLE is treated as T_INT)
    // n.b. this assumes Java returns an integral result in r0
    // and a floating result in j_farg0
    __ ldr(j_rarg2, result);
    Label is_long, is_float, is_double, exit;
    __ ldr(j_rarg1, result_type);
    __ cmp(j_rarg1, T_OBJECT);
    __ br(Assembler::EQ, is_long);
    __ cmp(j_rarg1, T_LONG);
    __ br(Assembler::EQ, is_long);
    __ cmp(j_rarg1, T_FLOAT);
    __ br(Assembler::EQ, is_float);
    __ cmp(j_rarg1, T_DOUBLE);
    __ br(Assembler::EQ, is_double);

    // handle T_INT case
    __ strw(r0, Address(j_rarg2));

    __ BIND(exit);

    // pop parameters
    __ sub(esp, rfp, -sp_after_call_off * wordSize);

#ifdef ASSERT
    // verify that threads correspond
    {
      Label L, S;
      __ ldr(rscratch1, thread);
      __ cmp(rthread, rscratch1);
      __ br(Assembler::NE, S);
      __ get_thread(rscratch1);
      __ cmp(rthread, rscratch1);
      __ br(Assembler::EQ, L);
      __ BIND(S);
      __ stop("StubRoutines::call_stub: threads must correspond");
      __ BIND(L);
    }
#endif

    // restore callee-save registers
    __ ldr(r28,      r28_save);
    __ ldr(r27,      r27_save);
    __ ldr(r26,      r26_save);
    __ ldr(r25,      r25_save);
    __ ldr(r24,      r24_save);
    __ ldr(r23,      r23_save);
    __ ldr(r22,      r22_save);
    __ ldr(r21,      r21_save);
    __ ldr(r20,      r20_save);
    __ ldr(r19,      r19_save);
    __ ldr(c_rarg0,  call_wrapper);
    __ ldr(c_rarg1,  result);
    __ ldrw(c_rarg2, result_type);
    __ ldr(c_rarg3,  method);
    __ ldr(c_rarg4,  entry_point);
    __ ldr(c_rarg5,  parameters);
    __ ldr(c_rarg6,  parameter_size);
    __ ldr(c_rarg7,  thread);

#ifndef PRODUCT
    // tell the simulator we are about to end Java execution
    if (NotifySimulator) {
      __ notify(Assembler::method_exit);
    }
#endif
    // leave frame and return to caller
    __ leave();
    __ ret(lr);

    // handle return types different from T_INT

    __ BIND(is_long);
    __ str(r0, Address(j_rarg2, 0));
    __ br(Assembler::AL, exit);

    __ BIND(is_float);
    __ strs(j_farg0, Address(j_rarg2, 0));
    __ br(Assembler::AL, exit);

    __ BIND(is_double);
    __ strd(j_farg0, Address(j_rarg2, 0));
    __ br(Assembler::AL, exit);

    return start;
  }

  // Return point for a Java call if there's an exception thrown in
  // Java code.  The exception is caught and transformed into a
  // pending exception stored in JavaThread that can be tested from
  // within the VM.
  //
  // Note: Usually the parameters are removed by the callee. In case
  // of an exception crossing an activation frame boundary, that is
  // not the case if the callee is compiled code => need to setup the
  // rsp.
  //
  // rax: exception oop

  // NOTE: this is used as a target from the signal handler so it
  // needs an x86 prolog which returns into the current simulator
  // executing the generated catch_exception code. so the prolog
  // needs to install rax in a sim register and adjust the sim's
  // restart pc to enter the generated code at the start position
  // then return from native to simulated execution.

  address generate_catch_exception() {
    StubCodeMark mark(this, "StubRoutines", "catch_exception");
    address start = __ pc();

    // same as in generate_call_stub():
    const Address sp_after_call(rfp, sp_after_call_off * wordSize);
    const Address thread        (rfp, thread_off         * wordSize);

#ifdef ASSERT
    // verify that threads correspond
    {
      Label L, S;
      __ ldr(rscratch1, thread);
      __ cmp(rthread, rscratch1);
      __ br(Assembler::NE, S);
      __ get_thread(rscratch1);
      __ cmp(rthread, rscratch1);
      __ br(Assembler::EQ, L);
      __ bind(S);
      __ stop("StubRoutines::catch_exception: threads must correspond");
      __ bind(L);
    }
#endif

    // set pending exception
    __ verify_oop(r0);

    __ str(r0, Address(rthread, Thread::pending_exception_offset()));
    __ mov(rscratch1, (address)__FILE__);
    __ str(rscratch1, Address(rthread, Thread::exception_file_offset()));
    __ movw(rscratch1, (int)__LINE__);
    __ strw(rscratch1, Address(rthread, Thread::exception_line_offset()));

    // complete return to VM
    assert(StubRoutines::_call_stub_return_address != NULL,
           "_call_stub_return_address must have been generated before");
    __ b(StubRoutines::_call_stub_return_address);

    return start;
  }

  // Continuation point for runtime calls returning with a pending
  // exception.  The pending exception check happened in the runtime
  // or native call stub.  The pending exception in Thread is
  // converted into a Java-level exception.
  //
  // Contract with Java-level exception handlers:
  // r0: exception
  // r3: throwing pc
  //
  // NOTE: At entry of this stub, exception-pc must be in LR !!

  // NOTE: this is always used as a jump target within generated code
  // so it just needs to be generated code wiht no x86 prolog

  address generate_forward_exception() {
    StubCodeMark mark(this, "StubRoutines", "forward exception");
    address start = __ pc();

    // Upon entry, LR points to the return address returning into
    // Java (interpreted or compiled) code; i.e., the return address
    // becomes the throwing pc.
    //
    // Arguments pushed before the runtime call are still on the stack
    // but the exception handler will reset the stack pointer ->
    // ignore them.  A potential result in registers can be ignored as
    // well.

#ifdef ASSERT
    // make sure this code is only executed if there is a pending exception
    {
      Label L;
      __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
      __ cbnz(rscratch1, L);
      __ stop("StubRoutines::forward exception: no pending exception (1)");
      __ bind(L);
    }
#endif

    // compute exception handler into r19

    // call the VM to find the handler address associated with the
    // caller address. pass thread in r0 and caller pc (ret address)
    // in r1. n.b. the caller pc is in lr, unlike x86 where it is on
    // the stack.
    __ mov(c_rarg1, lr);
    // lr will be trashed by the VM call so we move it to R19
    // (callee-saved) because we also need to pass it to the handler
    // returned by this call.
    __ mov(r19, lr);
    BLOCK_COMMENT("call exception_handler_for_return_address");
    __ call_VM_leaf(CAST_FROM_FN_PTR(address,
                         SharedRuntime::exception_handler_for_return_address),
                    rthread, c_rarg1);
    // we should not really care that lr is no longer the callee
    // address. we saved the value the handler needs in r19 so we can
    // just copy it to r3. however, the C2 handler will push its own
    // frame and then calls into the VM and the VM code asserts that
    // the PC for the frame above the handler belongs to a compiled
    // Java method. So, we restore lr here to satisfy that assert.
    __ mov(lr, r19);
    // setup r0 & r3 & clear pending exception
    __ mov(r3, r19);
    __ mov(r19, r0);
    __ ldr(r0, Address(rthread, Thread::pending_exception_offset()));
    __ str(zr, Address(rthread, Thread::pending_exception_offset()));

#ifdef ASSERT
    // make sure exception is set
    {
      Label L;
      __ cbnz(r0, L);
      __ stop("StubRoutines::forward exception: no pending exception (2)");
      __ bind(L);
    }
#endif

    // continue at exception handler
    // r0: exception
    // r3: throwing pc
    // r19: exception handler
    __ verify_oop(r0);
    __ br(r19);

    return start;
  }

  // Support for jint atomic::xchg(jint exchange_value, volatile jint* dest)
  //
  // Arguments :
  //    c_rarg0: exchange_value
  //    c_rarg0: dest
  //
  // Result:
  //    *dest <- ex, return (orig *dest)

  // NOTE: not sure this is actually needed but if so it looks like it
  // is called from os-specific code i.e. it needs an x86 prolog

  address generate_atomic_xchg() { return 0; }

  // Support for intptr_t atomic::xchg_ptr(intptr_t exchange_value, volatile intptr_t* dest)
  //
  // Arguments :
  //    c_rarg0: exchange_value
  //    c_rarg1: dest
  //
  // Result:
  //    *dest <- ex, return (orig *dest)

  // NOTE: not sure this is actually needed but if so it looks like it
  // is called from os-specific code i.e. it needs an x86 prolog

  address generate_atomic_xchg_ptr() { return 0; }

  // Support for jint atomic::atomic_cmpxchg(jint exchange_value, volatile jint* dest,
  //                                         jint compare_value)
  //
  // Arguments :
  //    c_rarg0: exchange_value
  //    c_rarg1: dest
  //    c_rarg2: compare_value
  //
  // Result:
  //    if ( compare_value == *dest ) {
  //       *dest = exchange_value
  //       return compare_value;
  //    else
  //       return *dest;
  address generate_atomic_cmpxchg() { return 0; }

  // Support for jint atomic::atomic_cmpxchg_long(jlong exchange_value,
  //                                             volatile jlong* dest,
  //                                             jlong compare_value)
  // Arguments :
  //    c_rarg0: exchange_value
  //    c_rarg1: dest
  //    c_rarg2: compare_value
  //
  // Result:
  //    if ( compare_value == *dest ) {
  //       *dest = exchange_value
  //       return compare_value;
  //    else
  //       return *dest;

  // NOTE: not sure this is actually needed but if so it looks like it
  // is called from os-specific code i.e. it needs an x86 prolog

  address generate_atomic_cmpxchg_long() { return 0; }

  // Support for jint atomic::add(jint add_value, volatile jint* dest)
  //
  // Arguments :
  //    c_rarg0: add_value
  //    c_rarg1: dest
  //
  // Result:
  //    *dest += add_value
  //    return *dest;

  // NOTE: not sure this is actually needed but if so it looks like it
  // is called from os-specific code i.e. it needs an x86 prolog

  address generate_atomic_add() { return 0; }

  // Support for intptr_t atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest)
  //
  // Arguments :
  //    c_rarg0: add_value
  //    c_rarg1: dest
  //
  // Result:
  //    *dest += add_value
  //    return *dest;

  // NOTE: not sure this is actually needed but if so it looks like it
  // is called from os-specific code i.e. it needs an x86 prolog

  address generate_atomic_add_ptr() { return 0; }

  // Support for intptr_t OrderAccess::fence()
  //
  // Arguments :
  //
  // Result:

  // NOTE: this is called from C code so it needs an x86 prolog
  // or else we need to fiddle it with inline asm for now

  address generate_orderaccess_fence() { return 0; }

  // Support for intptr_t get_previous_fp()
  //
  // This routine is used to find the previous frame pointer for the
  // caller (current_frame_guess). This is used as part of debugging
  // ps() is seemingly lost trying to find frames.
  // This code assumes that caller current_frame_guess) has a frame.

  // NOTE: this is called from C code in os_windows.cpp with AMD64. other
  // builds use inline asm -- so we should be ok for aarch64

  address generate_get_previous_fp() { return 0; }

  // Support for intptr_t get_previous_sp()
  //
  // This routine is used to find the previous stack pointer for the
  // caller.

  // NOTE: this is called from C code in os_windows.cpp with AMD64. other
  // builds use inline asm -- so we should be ok for aarch64

  address generate_get_previous_sp() { return 0; }

  // NOTE: these fixup routines appear only to be called from the
  // opto code (they are mentioned in x86_64.ad) so we can do
  // without them for now on aarch64

  address generate_f2i_fixup() { Unimplemented(); return 0; }

  address generate_f2l_fixup() { Unimplemented(); return 0; }

  address generate_d2i_fixup() { Unimplemented(); return 0; }

  address generate_d2l_fixup() { Unimplemented(); return 0; }

  // The following routine generates a subroutine to throw an
  // asynchronous UnknownError when an unsafe access gets a fault that
  // could not be reasonably prevented by the programmer.  (Example:
  // SIGBUS/OBJERR.)

  // NOTE: this is used by the signal handler code as a return address
  // to re-enter Java execution so it needs an x86 prolog which will
  // reenter the simulator executing the generated handler code. so
  // the prolog needs to adjust the sim's restart pc to enter the
  // generated code at the start position then return from native to
  // simulated execution.

  address generate_handler_for_unsafe_access() { return 0; }

  // Non-destructive plausibility checks for oops
  //
  // Arguments:
  //    r0: oop to verify
  //    rscratch1: error message
  //
  // Stack after saving c_rarg3:
  //    [tos + 0]: saved c_rarg3
  //    [tos + 1]: saved c_rarg2
  //    [tos + 2]: saved rscratch2
  //    [tos + 3]: saved lr
  //    [tos + 4]: saved rscratch1
  //    [tos + 5]: saved r0
  address generate_verify_oop() {

    StubCodeMark mark(this, "StubRoutines", "verify_oop");
    address start = __ pc();

    Label exit, error;

    // __ pushf();
    // __ push(r12);

    // save c_rarg2 and c_rarg3
    __ stp(c_rarg3, c_rarg2, Address(__ pre(sp, -16)));

    // __ incrementl(ExternalAddress((address) StubRoutines::verify_oop_count_addr()));
    __ lea(c_rarg2, ExternalAddress((address) StubRoutines::verify_oop_count_addr()));
    __ ldr(c_rarg3, Address(c_rarg2));
    __ add(c_rarg3, c_rarg3, 1);
    __ str(c_rarg3, Address(c_rarg2));

    // object is in r0
    // make sure object is 'reasonable'
    __ cbz(r0, exit); // if obj is NULL it is OK

    // Check if the oop is in the right area of memory
    __ mov(c_rarg3, (intptr_t) Universe::verify_oop_mask());
    __ andr(c_rarg2, r0, c_rarg3);
    __ mov(c_rarg3, (intptr_t) Universe::verify_oop_bits());

    // Compare c_rarg2 and c_rarg3.  We don't use a compare
    // instruction here because the flags register is live.
    __ eor(c_rarg2, c_rarg2, c_rarg3);
    __ cbnz(c_rarg2, error);

    // make sure klass is 'reasonable', which is not zero.
    __ load_klass(r0, r0);  // get klass
    __ cbz(r0, error);      // if klass is NULL it is broken
    // TODO: Future assert that klass is lower 4g memory for UseCompressedKlassPointers

    // return if everything seems ok
    __ bind(exit);

    __ ldp(c_rarg3, c_rarg2, Address(__ post(sp, 16)));
    __ ret(lr);

    // handle errors
    __ bind(error);
    __ ldp(c_rarg3, c_rarg2, Address(__ post(sp, 16)));

    __ push(0x7fffffff, sp);
    // debug(char* msg, int64_t pc, int64_t regs[])
    __ ldr(c_rarg0, Address(sp, rscratch1->encoding()));    // pass address of error message
    __ mov(c_rarg1, Address(sp, lr));             // pass return address
    __ mov(c_rarg2, sp);                          // pass address of regs on stack
#ifndef PRODUCT
    assert(frame::arg_reg_save_area_bytes == 0, "not expecting frame reg save area");
#endif
    BLOCK_COMMENT("call MacroAssembler::debug");
    __ mov(rscratch1, CAST_FROM_FN_PTR(address, MacroAssembler::debug64));
    __ blrt(rscratch1, 3, 0, 1);
    __ pop(0x7fffffff, sp);

    __ ldp(rscratch2, lr, Address(__ post(sp, 2 * wordSize)));
    __ ldp(r0, rscratch1, Address(__ post(sp, 2 * wordSize)));

    __ ret(lr);

    return start;
  }

  //
  // Verify that a register contains clean 32-bits positive value
  // (high 32-bits are 0) so it could be used in 64-bits shifts.
  //
  //  Input:
  //    Rint  -  32-bits value
  //    Rtmp  -  scratch
  //
  void assert_clean_int(Register Rint, Register Rtmp) { Unimplemented(); }

  //  Generate overlap test for array copy stubs
  //
  //  Input:
  //     c_rarg0 - from
  //     c_rarg1 - to
  //     c_rarg2 - element count
  //
  //  Output:
  //     r0   - &from[element count - 1]
  //
  void array_overlap_test(address no_overlap_target, int sf) { Unimplemented(); }
  void array_overlap_test(Label& L_no_overlap, Address::sxtw sf) { __ b(L_no_overlap); }
  void array_overlap_test(address no_overlap_target, Label* NOLp, int sf) { Unimplemented(); }

  // Shuffle first three arg regs on Windows into Linux/Solaris locations.
  //
  // Outputs:
  //    rdi - rcx
  //    rsi - rdx
  //    rdx - r8
  //    rcx - r9
  //
  // Registers r9 and r10 are used to save rdi and rsi on Windows, which latter
  // are non-volatile.  r9 and r10 should not be used by the caller.
  //
  void setup_arg_regs(int nargs = 3) { Unimplemented(); }

  void restore_arg_regs() { Unimplemented(); }

  // Generate code for an array write pre barrier
  //
  //     addr    -  starting address
  //     count   -  element count
  //     tmp     - scratch register
  //
  //     Destroy no registers!
  //
  void  gen_write_ref_array_pre_barrier(Register addr, Register count, bool dest_uninitialized) {
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
    case BarrierSet::G1SATBCT:
    case BarrierSet::G1SATBCTLogging:
      // With G1, don't generate the call if we statically know that the target in uninitialized
      if (!dest_uninitialized) {
	__ push(0x3fffffff, sp);         // integer registers except lr & sp
	if (count == c_rarg0) {
	  if (addr == c_rarg1) {
	    // exactly backwards!!
	    __ stp(c_rarg0, c_rarg1, __ pre(sp, -2 * wordSize));
	    __ ldp(c_rarg1, c_rarg0, __ post(sp, -2 * wordSize));
	  } else {
	    __ mov(c_rarg1, count);
	    __ mov(c_rarg0, addr);
	  }
	} else {
	  __ mov(c_rarg0, addr);
	  __ mov(c_rarg1, count);
	}
	__ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_pre), 2);
	__ pop(0x3fffffff, sp);         // integer registers except lr & sp        }
	break;
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
      case BarrierSet::ModRef:
        break;
      default:
        ShouldNotReachHere();

      }
    }
  }

  //
  // Generate code for an array write post barrier
  //
  //  Input:
  //     start    - register containing starting address of destination array
  //     end      - register containing ending address of destination array
  //     scratch  - scratch register
  //
  //  The input registers are overwritten.
  //  The ending address is inclusive.
  void gen_write_ref_array_post_barrier(Register start, Register end, Register scratch) {
    assert_different_registers(start, end, scratch);
    BarrierSet* bs = Universe::heap()->barrier_set();
    switch (bs->kind()) {
      case BarrierSet::G1SATBCT:
      case BarrierSet::G1SATBCTLogging:

        {
	  __ push(0x3fffffff, sp);         // integer registers except lr & sp
          // must compute element count unless barrier set interface is changed (other platforms supply count)
          assert_different_registers(start, end, scratch);
          __ lea(scratch, Address(end, BytesPerHeapOop));
          __ sub(scratch, scratch, start);               // subtract start to get #bytes
          __ lsr(scratch, scratch, LogBytesPerHeapOop);  // convert to element count
          __ mov(c_rarg0, start);
          __ mov(c_rarg1, scratch);
          __ call_VM_leaf(CAST_FROM_FN_PTR(address, BarrierSet::static_write_ref_array_post), 2);
	  __ pop(0x3fffffff, sp);         // integer registers except lr & sp        }
        }
        break;
      case BarrierSet::CardTableModRef:
      case BarrierSet::CardTableExtension:
        {
          CardTableModRefBS* ct = (CardTableModRefBS*)bs;
          assert(sizeof(*ct->byte_map_base) == sizeof(jbyte), "adjust this code");

          Label L_loop;

           __ lsr(start, start, CardTableModRefBS::card_shift);
           __ add(end, end, BytesPerHeapOop);
           __ lsr(end, end, CardTableModRefBS::card_shift);
           __ sub(end, end, start); // number of bytes to copy

          const Register count = end; // 'end' register contains bytes count now
	  __ mov(scratch, (address)ct->byte_map_base);
          __ add(start, start, scratch);
	  __ BIND(L_loop);
	  __ strb(zr, Address(start, count));
          __ subs(count, count, 1);
          __ br(Assembler::HI, L_loop);
        }
        break;
      default:
        ShouldNotReachHere();

    }
  }

  typedef enum {
    copy_forwards = 1,
    copy_backwards = -1
  } copy_direction;

  void copy_longs_small(Register s, Register d, Register count, copy_direction direction) {
    Label again, around;
    __ cbz(count, around);
    __ bind(again);
    __ ldr(r16, Address(__ adjust(s, wordSize * direction, direction == copy_backwards)));
    __ str(r16, Address(__ adjust(d, wordSize * direction, direction == copy_backwards)));
    __ sub(count, count, 1);
    __ cbnz(count, again);
    __ bind(around);
  }

  void copy_longs(Register s, Register d, Register count, Register tmp, copy_direction direction) {
    __ andr(tmp, count, 3);
    copy_longs_small(s, d, tmp, direction);
    __ andr(count, count, -4);

    Label again, around;
    __ cbz(count, around);
    __ push(r18->bit() | r19->bit(), sp);
    __ bind(again);
    if (direction != copy_backwards) {
      __ prfm(Address(s, direction == copy_forwards ? 4 * wordSize : -6 * wordSize));
      __ prfm(Address(s, direction == copy_forwards ? 6 * wordSize : -8 * wordSize));
    }
    __ ldp(r16, r17, Address(__ adjust(s, 2 * wordSize * direction, direction == copy_backwards)));
    __ ldp(r18, r19, Address(__ adjust(s, 2 * wordSize * direction, direction == copy_backwards)));
    __ stp(r16, r17, Address(__ adjust(d, 2 * wordSize * direction, direction == copy_backwards)));
    __ stp(r18, r19, Address(__ adjust(d, 2 * wordSize * direction, direction == copy_backwards)));
    __ subs(count, count, 4);
    __ cbnz(count, again);
    __ pop(r18->bit() | r19->bit(), sp);
    __ bind(around);
  }

  void copy_memory_small(Register s, Register d, Register count, Register tmp, int step, Label &done) {
    // Small copy: less than one word.
    bool is_backwards = step < 0;
    int granularity = abs(step);

    __ cbz(count, done);
    {
      Label loop;
      __ bind(loop);
      switch (granularity) {
      case 1:
	__ ldrb(tmp, Address(__ adjust(s, step, is_backwards)));
	__ strb(tmp, Address(__ adjust(d, step, is_backwards)));
	break;
      case 2:
	__ ldrh(tmp, Address(__ adjust(s, step, is_backwards)));
	__ strh(tmp, Address(__ adjust(d, step, is_backwards)));
	break;
      case 4:
	__ ldrw(tmp, Address(__ adjust(s, step, is_backwards)));
	__ strw(tmp, Address(__ adjust(d, step, is_backwards)));
	break;
      default:
	assert(false, "copy_memory called with impossible step");
      }
      __ sub(count, count, 1);
      __ cbnz(count, loop);
      __ b(done);
    }
  }

  // All-singing all-dancing memory copy.
  //
  // Copy count units of memory from s to d.  The size of a unit is
  // step, which can be positive or negative depending on the direction
  // of copy.  If is_aligned is false, we align the source address.
  //

  void copy_memory(bool is_aligned, Register s, Register d,
		   Register count, Register tmp, int step) {
    copy_direction direction = step < 0 ? copy_backwards : copy_forwards;
    bool is_backwards = step < 0;
    int granularity = abs(step);

    if (is_backwards) {
      __ lea(s, Address(s, count, Address::uxtw(exact_log2(-step))));
      __ lea(d, Address(d, count, Address::uxtw(exact_log2(-step))));
    }

    if (granularity == wordSize) {
      copy_longs(c_rarg0, c_rarg1, c_rarg2, rscratch1, direction);
      return;
    }

    Label done, large;

    if (! is_aligned) {
      __ cmp(count, wordSize/granularity);
      __ br(Assembler::HS, large);
      copy_memory_small(s, d, count, tmp, step, done);
      __ bind(large);

      // Now we've got the small case out of the way we can align the
      // source address.
      {
	Label skip1, skip2, skip4;

	switch (granularity) {
	case 1:
	  __ tst(s, 1);
	  __ br(Assembler::EQ, skip1);
	  __ ldrb(tmp, Address(__ adjust(s, direction, is_backwards)));
	  __ strb(tmp, Address(__ adjust(d, direction, is_backwards)));
	  __ sub(count, count, 1);
	  __ bind(skip1);
	  // fall through
	case 2:
	  __ tst(s, 2/granularity);
	  __ br(Assembler::EQ, skip2);
	  __ ldrh(tmp, Address(__ adjust(s, 2 * direction, is_backwards)));
	  __ strh(tmp, Address(__ adjust(d, 2 * direction, is_backwards)));
	  __ sub(count, count, 2/granularity);
	  __ bind(skip2);
	  // fall through
	case 4:
	  __ tst(s, 4/granularity);
	  __ br(Assembler::EQ, skip4);
	  __ ldrw(tmp, Address(__ adjust(s, 4 * direction, is_backwards)));
	  __ strw(tmp, Address(__ adjust(d, 4 * direction, is_backwards)));
	  __ sub(count, count, 4/granularity);
	  __ bind(skip4);
	}
      }
    }

    // s is now word-aligned.

    // We have a count of units and some trailing bytes.  Adjust the
    // count and do a bulk copy of words.
    __ lsr(rscratch2, count, exact_log2(wordSize/granularity));
    __ sub(count, count, rscratch2, Assembler::LSL, exact_log2(wordSize/granularity));

    copy_longs(s, d, rscratch2, rscratch1, direction);

    // And the tail.

    copy_memory_small(s, d, count, tmp, step, done);

    __ bind(done);
  }

  // Scan over array at a for count oops, verifying each one.
  // Preserves a and count, clobbers rscratch1 and rscratch2.
  void verify_oop_array (size_t size, Register a, Register count, Register temp) {
    Label loop, end;
    __ mov(rscratch1, a);
    __ mov(rscratch2, zr);
    __ bind(loop);
    __ cmp(rscratch2, count);
    __ br(Assembler::HS, end);
    if (size == (size_t)wordSize) {
      __ ldr(temp, Address(a, rscratch2, Address::uxtw(exact_log2(size))));
      __ verify_oop(temp);
    } else {
      __ ldrw(r16, Address(a, rscratch2, Address::uxtw(exact_log2(size))));
      __ decode_heap_oop(temp); // calls verify_oop
    }
    __ add(rscratch2, rscratch2, size);
    __ b(loop);
    __ bind(end);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_copy(size_t size, bool aligned, bool is_oop, address *entry,
				  const char *name, bool dest_uninitialized = false) {
    Register s = c_rarg0, d = c_rarg1, count = c_rarg2;
    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();
    if (entry != NULL) {
      *entry = __ pc();
      // caller can pass a 64-bit byte count here (from Unsafe.copyMemory)
      BLOCK_COMMENT("Entry:");
    }
    __ enter();
    __ push(r16->bit() | r17->bit(), sp);
    if (is_oop) {
      __ push(d->bit() | count->bit(), sp);
      // no registers are destroyed by this call
      gen_write_ref_array_pre_barrier(d, count, dest_uninitialized);
    }
    copy_memory(aligned, s, d, count, rscratch1, size);
    if (is_oop) {
      __ pop(d->bit() | count->bit(), sp);
      if (VerifyOops)
	verify_oop_array(size, d, count, r16);
      __ sub(count, count, 1); // make an inclusive end pointer
      __ lea(count, Address(d, count, Address::uxtw(exact_log2(size))));
      gen_write_ref_array_post_barrier(d, count, rscratch1);
    }
    __ pop(r16->bit() | r17->bit(), sp);
    __ leave();
    __ ret(lr);
    return start;
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   is_oop  - true => oop array, so generate store check code
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_copy(size_t size, bool aligned, bool is_oop, address nooverlap_target,
				 address *entry, const char *name,
				 bool dest_uninitialized = false) {
    Register s = c_rarg0, d = c_rarg1, count = c_rarg2;

    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    __ cmp(d, s);
    __ br(Assembler::LS, nooverlap_target);

    __ enter();
    __ push(r16->bit() | r17->bit(), sp);
    if (is_oop) {
      __ push(d->bit() | count->bit(), sp);
      // no registers are destroyed by this call
      gen_write_ref_array_pre_barrier(d, count, dest_uninitialized);
    }
    copy_memory(aligned, s, d, count, rscratch1, -size);
    if (is_oop) {
      __ pop(d->bit() | count->bit(), sp);
      if (VerifyOops)
	verify_oop_array(size, d, count, r16);
      __ sub(count, count, 1); // make an inclusive end pointer
      __ lea(count, Address(d, count, Address::uxtw(exact_log2(size))));
      gen_write_ref_array_post_barrier(d, count, rscratch1);
    }
    __ pop(r16->bit() | r17->bit(), sp);
    __ leave();
    __ ret(lr);

    return start;
}

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_byte_copy_entry is set to the no-overlap entry point  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_byte_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_byte_copy().
  //
  address generate_disjoint_byte_copy(bool aligned, address* entry, const char *name) {
    return generate_disjoint_copy(sizeof (jbyte), aligned, /*is_oop*/false, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-, 2-, or 1-byte boundaries,
  // we let the hardware handle it.  The one to eight bytes within words,
  // dwords or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_byte_copy(bool aligned, address nooverlap_target,
                                      address* entry, const char *name) {
    return generate_conjoint_copy(sizeof (jbyte), aligned, /*is_oop*/false, nooverlap_target, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4- or 2-byte boundaries, we
  // let the hardware handle it.  The two or four words within dwords
  // or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  // Side Effects:
  //   disjoint_short_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_short_copy().
  //
  address generate_disjoint_short_copy(bool aligned,
				       address* entry, const char *name) {
    return generate_disjoint_copy(sizeof (jshort), aligned, /*is_oop*/false, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4- or 2-byte boundaries, we
  // let the hardware handle it.  The two or four words within dwords
  // or qwords that span cache line boundaries will still be loaded
  // and stored atomically.
  //
  address generate_conjoint_short_copy(bool aligned, address nooverlap_target,
                                       address *entry, const char *name) {
    return generate_conjoint_copy(sizeof (jshort), aligned, /*is_oop*/false, nooverlap_target, entry, name);

  }
  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  // Side Effects:
  //   disjoint_int_copy_entry is set to the no-overlap entry point
  //   used by generate_conjoint_int_oop_copy().
  //
  address generate_disjoint_int_copy(bool aligned, address *entry,
					 const char *name, bool dest_uninitialized = false) {
    const bool not_oop = false;
    return generate_disjoint_copy(sizeof (jint), aligned, not_oop, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord == 8-byte boundary
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as ssize_t, can be zero
  //
  // If 'from' and/or 'to' are aligned on 4-byte boundaries, we let
  // the hardware handle it.  The two dwords within qwords that span
  // cache line boundaries will still be loaded and stored atomicly.
  //
  address generate_conjoint_int_copy(bool aligned, address nooverlap_target,
				     address *entry, const char *name,
				     bool dest_uninitialized = false) {
    const bool not_oop = false;
    return generate_conjoint_copy(sizeof (jint), aligned, not_oop, nooverlap_target, entry, name);
  }


  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord boundary == 8 bytes
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as size_t, can be zero
  //
  // Side Effects:
  //   disjoint_oop_copy_entry or disjoint_long_copy_entry is set to the
  //   no-overlap entry point used by generate_conjoint_long_oop_copy().
  //
  address generate_disjoint_long_copy(bool aligned, address *entry,
                                          const char *name, bool dest_uninitialized = false) {
    const bool not_oop = false;
    return generate_disjoint_copy(sizeof (jlong), aligned, not_oop, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord boundary == 8 bytes
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as size_t, can be zero
  //
  address generate_conjoint_long_copy(bool aligned,
				      address nooverlap_target, address *entry,
				      const char *name, bool dest_uninitialized = false) {
    const bool not_oop = false;
    return generate_conjoint_copy(sizeof (jlong), aligned, not_oop, nooverlap_target, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord boundary == 8 bytes
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as size_t, can be zero
  //
  // Side Effects:
  //   disjoint_oop_copy_entry or disjoint_long_copy_entry is set to the
  //   no-overlap entry point used by generate_conjoint_long_oop_copy().
  //
  address generate_disjoint_oop_copy(bool aligned, address *entry,
				     const char *name, bool dest_uninitialized = false) {
    const bool is_oop = true;
    if (UseCompressedOops)
      return generate_disjoint_copy(sizeof (jint), aligned, is_oop, entry, name);
    else
      return generate_disjoint_copy(sizeof (jlong), aligned, is_oop, entry, name);
  }

  // Arguments:
  //   aligned - true => Input and output aligned on a HeapWord boundary == 8 bytes
  //             ignored
  //   name    - stub name string
  //
  // Inputs:
  //   c_rarg0   - source array address
  //   c_rarg1   - destination array address
  //   c_rarg2   - element count, treated as size_t, can be zero
  //
  address generate_conjoint_oop_copy(bool aligned,
				     address nooverlap_target, address *entry,
				     const char *name, bool dest_uninitialized = false) {
    const bool is_oop = true;
    if (UseCompressedOops)
      return generate_conjoint_copy(sizeof (jint), aligned, is_oop, nooverlap_target, entry, name);
    else
      return generate_conjoint_copy(sizeof (jlong), aligned, is_oop, nooverlap_target, entry, name);
  }


  // Helper for generating a dynamic type check.
  // Smashes rscratch1.
  void generate_type_check(Register sub_klass,
                           Register super_check_offset,
                           Register super_klass,
                           Label& L_success) {
    assert_different_registers(sub_klass, super_check_offset, super_klass);

    BLOCK_COMMENT("type_check:");

    Label L_miss;

    __ check_klass_subtype_fast_path(sub_klass, super_klass, noreg,        &L_success, &L_miss, NULL,
                                     super_check_offset);
    __ check_klass_subtype_slow_path(sub_klass, super_klass, noreg, noreg, &L_success, NULL);

    // Fall through on failure!
    __ BIND(L_miss);
  }

  //
  //  Generate checkcasting array copy stub
  //
  //  Input:
  //    c_rarg0   - source array address
  //    c_rarg1   - destination array address
  //    c_rarg2   - element count, treated as ssize_t, can be zero
  //    c_rarg3   - size_t ckoff (super_check_offset)
  //    c_rarg4   - oop ckval (super_klass)
  //
  //  Output:
  //    r0 ==  0  -  success
  //    r0 == -1^K - failure, where K is partial transfer count
  //
  address generate_checkcast_copy(const char *name, address *entry,
                                  bool dest_uninitialized = false) {

    Label L_load_element, L_store_element, L_do_card_marks, L_done;

    // Input registers (after setup_arg_regs)
    const Register from        = c_rarg0;   // source array address
    const Register to          = c_rarg1;   // destination array address
    const Register length      = c_rarg2;   // elements count
    const Register ckoff       = c_rarg3;   // super_check_offset
    const Register ckval       = c_rarg4;   // super_klass

    // Registers used as temps (r16, r17, r18, r19, r20 are save-on-entry)
    const Register end_from    = from;  // source array end address
    const Register end_to      = r16;   // destination array end address
    const Register count       = r20;   // -(count_remaining)
    const Register r17_length  = r17;   // saved copy of length
    // End pointers are inclusive, and if length is not zero they point
    // to the last unit copied:  end_to[0] := end_from[0]

    const Register copied_oop  = r18;    // actual oop copied
    const Register r19_klass   = r19;    // oop._klass

    //---------------------------------------------------------------
    // Assembler stub will be used for this call to arraycopy
    // if the two arrays are subtypes of Object[] but the
    // destination array type is not equal to or a supertype
    // of the source type.  Each element must be separately
    // checked.

    __ align(CodeEntryAlignment);
    StubCodeMark mark(this, "StubRoutines", name);
    address start = __ pc();

    __ enter(); // required for proper stackwalking of RuntimeStub frame

#ifdef ASSERT
    // caller guarantees that the arrays really are different
    // otherwise, we would have to make conjoint checks
    { Label L;
      array_overlap_test(L, TIMES_OOP);
      __ stop("checkcast_copy within a single array");
      __ bind(L);
    }
#endif //ASSERT

    // Caller of this entry point must set up the argument registers.
    if (entry != NULL) {
      *entry = __ pc();
      BLOCK_COMMENT("Entry:");
    }

    __ push(r16->bit() | r17->bit() | r18->bit() | r19->bit() | r20->bit(), sp);

#ifdef ASSERT
    BLOCK_COMMENT("assert consistent ckoff/ckval");
    // The ckoff and ckval must be mutually consistent,
    // even though caller generates both.
    { Label L;
      int sco_offset = in_bytes(Klass::super_check_offset_offset());
      __ ldrw(count, Address(ckval, sco_offset));
      __ cmpw(ckoff, count);
      __ br(Assembler::EQ, L);
      __ stop("super_check_offset inconsistent");
      __ bind(L);
    }
#endif //ASSERT

    // Loop-invariant addresses.  They are exclusive end pointers.
    Address end_from_addr(from, length, TIMES_OOP);
    Address   end_to_addr(to,   length, TIMES_OOP);
    // Loop-variant addresses.  They assume post-incremented count < 0.
    Address from_element_addr(end_from, count, TIMES_OOP);
    Address   to_element_addr(end_to,   count, TIMES_OOP);

    gen_write_ref_array_pre_barrier(to, count, dest_uninitialized);

    // Copy from low to high addresses, indexed from the end of each array.
    __ lea(end_from, end_from_addr);
    __ lea(end_to, end_to_addr);
    __ mov(r17_length, length);        // save a copy of the length
    __ neg(count, length);    // negate and test the length
    __ cbnz(count, L_load_element);

    // Empty array:  Nothing to do.
    __ mov(r0, zr);                  // return 0 on (trivial) success
    __ b(L_done);

    // ======== begin loop ========
    // (Loop is rotated; its entry is L_load_element.)
    // Loop control:
    //   for (count = -count; count != 0; count++)
    // Base pointers src, dst are biased by 8*(count-1),to last element.
    __ align(OptoLoopAlignment);

    __ BIND(L_store_element);
    __ store_heap_oop(to_element_addr, copied_oop);  // store the oop
    __ add(count, count, 1);               // increment the count toward zero
    __ cbz(count, L_do_card_marks);

    // ======== loop entry is here ========
    __ BIND(L_load_element);
    __ load_heap_oop(copied_oop, from_element_addr); // load the oop
    __ cbz(copied_oop, L_store_element);

    __ load_klass(r19_klass, copied_oop);// query the object klass
    generate_type_check(r19_klass, ckoff, ckval, L_store_element);
    // ======== end loop ========

    // It was a real error; we must depend on the caller to finish the job.
    // Register r0 = -1 * number of *remaining* oops, r17 = *total* oops.
    // Emit GC store barriers for the oops we have copied and report
    // their number to the caller.
    assert_different_registers(r0, r17_length, count, to, end_to, ckoff);
    __ lea(end_to, to_element_addr);
    __ add(end_to, end_to, -heapOopSize);      // make an inclusive end pointer
    gen_write_ref_array_post_barrier(to, end_to, rscratch1);
    __ add(r0, r17_length, count);                // K = (original - remaining) oops
    __ eon(r0, r0, zr);                       // report (-1^K) to caller
    __ b(L_done);

    // Come here on success only.
    __ BIND(L_do_card_marks);
    __ add(end_to, end_to, -heapOopSize);         // make an inclusive end pointer
    gen_write_ref_array_post_barrier(to, end_to, rscratch1);
    __ mov(r0, zr);                  // return 0 on success

    // Common exit point (success or failure).
    __ BIND(L_done);
    __ pop(r16->bit() | r17->bit() | r18->bit() | r19->bit() | r20->bit(), sp);
    inc_counter_np(SharedRuntime::_checkcast_array_copy_ctr);
    __ leave(); // required for proper stackwalking of RuntimeStub frame
    __ ret(lr);

    return start;
  }

  //
  //  Generate 'unsafe' array copy stub
  //  Though just as safe as the other stubs, it takes an unscaled
  //  size_t argument instead of an element count.
  //
  //  Input:
  //    c_rarg0   - source array address
  //    c_rarg1   - destination array address
  //    c_rarg2   - byte count, treated as ssize_t, can be zero
  //
  // Examines the alignment of the operands and dispatches
  // to a long, int, short, or byte copy loop.
  //
  address generate_unsafe_copy(const char *name,
                               address byte_copy_entry, address short_copy_entry,
                               address int_copy_entry, address long_copy_entry) { Unimplemented(); return 0; }

  // Perform range checks on the proposed arraycopy.
  // Kills temp, but nothing else.
  // Also, clean the sign bits of src_pos and dst_pos.
  void arraycopy_range_checks(Register src,     // source array oop (c_rarg0)
                              Register src_pos, // source position (c_rarg1)
                              Register dst,     // destination array oo (c_rarg2)
                              Register dst_pos, // destination position (c_rarg3)
                              Register length,
                              Register temp,
                              Label& L_failed) { Unimplemented(); }

  //
  //  Generate generic array copy stubs
  //
  //  Input:
  //    c_rarg0    -  src oop
  //    c_rarg1    -  src_pos (32-bits)
  //    c_rarg2    -  dst oop
  //    c_rarg3    -  dst_pos (32-bits)
  // not Win64
  //    c_rarg4    -  element count (32-bits)
  // Win64
  //    rsp+40     -  element count (32-bits)
  //
  //  Output:
  //    rax ==  0  -  success
  //    rax == -1^K - failure, where K is partial transfer count
  //
  address generate_generic_copy(const char *name,
                                address byte_copy_entry, address short_copy_entry,
                                address int_copy_entry, address oop_copy_entry,
                                address long_copy_entry, address checkcast_copy_entry) { Unimplemented(); return 0; }

  // These stubs get called from some dumb test routine.
  // I'll write them properly when they're called from
  // something that's actually doing something.
  static void fake_arraycopy_stub(address src, address dst, int count) {
    assert(count == 0, "huh?");
  }


  void generate_arraycopy_stubs() {
    address entry;
    address entry_jbyte_arraycopy;
    address entry_jshort_arraycopy;
    address entry_jint_arraycopy;
    address entry_oop_arraycopy;
    address entry_jlong_arraycopy;
    address entry_checkcast_arraycopy;

    //*** jbyte
    // Always need aligned and unaligned versions
    StubRoutines::_jbyte_disjoint_arraycopy         = generate_disjoint_byte_copy(false, &entry,
                                                                                  "jbyte_disjoint_arraycopy");
    StubRoutines::_jbyte_arraycopy                  = generate_conjoint_byte_copy(false, entry,
                                                                                  &entry_jbyte_arraycopy,
                                                                                  "jbyte_arraycopy");
    StubRoutines::_arrayof_jbyte_disjoint_arraycopy = generate_disjoint_byte_copy(true, &entry,
                                                                                  "arrayof_jbyte_disjoint_arraycopy");
    StubRoutines::_arrayof_jbyte_arraycopy          = generate_conjoint_byte_copy(true, entry, NULL,
                                                                                  "arrayof_jbyte_arraycopy");

    //*** jshort
    // Always need aligned and unaligned versions
    StubRoutines::_jshort_disjoint_arraycopy         = generate_disjoint_short_copy(false, &entry,
                                                                                    "jshort_disjoint_arraycopy");
    StubRoutines::_jshort_arraycopy                  = generate_conjoint_short_copy(false, entry,
                                                                                    &entry_jshort_arraycopy,
                                                                                    "jshort_arraycopy");
    StubRoutines::_arrayof_jshort_disjoint_arraycopy = generate_disjoint_short_copy(true, &entry,
                                                                                    "arrayof_jshort_disjoint_arraycopy");
    StubRoutines::_arrayof_jshort_arraycopy          = generate_conjoint_short_copy(true, entry, NULL,
                                                                                    "arrayof_jshort_arraycopy");

    //*** jint
    // Aligned versions
    StubRoutines::_arrayof_jint_disjoint_arraycopy = generate_disjoint_int_copy(true, &entry,
										"arrayof_jint_disjoint_arraycopy");
    StubRoutines::_arrayof_jint_arraycopy          = generate_conjoint_int_copy(true, entry, &entry_jint_arraycopy,
										"arrayof_jint_arraycopy");
    // In 64 bit we need both aligned and unaligned versions of jint arraycopy.
    // entry_jint_arraycopy always points to the unaligned version
    StubRoutines::_jint_disjoint_arraycopy         = generate_disjoint_int_copy(false, &entry,
										"jint_disjoint_arraycopy");
    StubRoutines::_jint_arraycopy                  = generate_conjoint_int_copy(false, entry,
										&entry_jint_arraycopy,
										"jint_arraycopy");

    //*** jlong
    // It is always aligned
    StubRoutines::_arrayof_jlong_disjoint_arraycopy = generate_disjoint_long_copy(true, &entry,
										  "arrayof_jlong_disjoint_arraycopy");
    StubRoutines::_arrayof_jlong_arraycopy          = generate_conjoint_long_copy(true, entry, &entry_jlong_arraycopy,
										  "arrayof_jlong_arraycopy");
    StubRoutines::_jlong_disjoint_arraycopy         = StubRoutines::_arrayof_jlong_disjoint_arraycopy;
    StubRoutines::_jlong_arraycopy                  = StubRoutines::_arrayof_jlong_arraycopy;

    //*** oops
    {
      // With compressed oops we need unaligned versions; notice that
      // we overwrite entry_oop_arraycopy.
      bool aligned = !UseCompressedOops;

      StubRoutines::_arrayof_oop_disjoint_arraycopy
	= generate_disjoint_oop_copy(aligned, &entry, "arrayof_oop_disjoint_arraycopy");
      StubRoutines::_arrayof_oop_arraycopy
	= generate_conjoint_oop_copy(aligned, entry, &entry_oop_arraycopy, "arrayof_oop_arraycopy");
      // Aligned versions without pre-barriers
      StubRoutines::_arrayof_oop_disjoint_arraycopy_uninit
	= generate_disjoint_oop_copy(aligned, &entry, "arrayof_oop_disjoint_arraycopy_uninit",
				     /*dest_uninitialized*/true);
      StubRoutines::_arrayof_oop_arraycopy_uninit
	= generate_conjoint_oop_copy(aligned, entry, NULL, "arrayof_oop_arraycopy_uninit",
				     /*dest_uninitialized*/true);
    }

    StubRoutines::_oop_disjoint_arraycopy            = StubRoutines::_arrayof_oop_disjoint_arraycopy;
    StubRoutines::_oop_arraycopy                     = StubRoutines::_arrayof_oop_arraycopy;
    StubRoutines::_oop_disjoint_arraycopy_uninit     = StubRoutines::_arrayof_oop_disjoint_arraycopy_uninit;
    StubRoutines::_oop_arraycopy_uninit              = StubRoutines::_arrayof_oop_arraycopy_uninit;

    StubRoutines::_checkcast_arraycopy        = generate_checkcast_copy("checkcast_arraycopy", &entry_checkcast_arraycopy);
    StubRoutines::_checkcast_arraycopy_uninit = generate_checkcast_copy("checkcast_arraycopy_uninit", NULL,
                                                                        /*dest_uninitialized*/true);
  }

  void generate_math_stubs() { Unimplemented(); }

#undef __
#define __ masm->

  // Continuation point for throwing of implicit exceptions that are
  // not handled in the current activation. Fabricates an exception
  // oop and initiates normal exception dispatching in this
  // frame. Since we need to preserve callee-saved values (currently
  // only for C2, but done for C1 as well) we need a callee-saved oop
  // map and therefore have to make these stubs into RuntimeStubs
  // rather than BufferBlobs.  If the compiler needs all registers to
  // be preserved between the fault point and the exception handler
  // then it must assume responsibility for that in
  // AbstractCompiler::continuation_for_implicit_null_exception or
  // continuation_for_implicit_division_by_zero_exception. All other
  // implicit exceptions (e.g., NullPointerException or
  // AbstractMethodError on entry) are either at call sites or
  // otherwise assume that stack unwinding will be initiated, so
  // caller saved registers were assumed volatile in the compiler.

  // NOTE: this needs carefully checking to see where the generated
  // code gets called from for each generated error
  //
  // WrongMethodTypeException : jumped to directly from generated method
  // handle code.
  //
  // StackOverflowError : jumped to directly from generated code in
  // cpp and template interpreter. the generated code address also
  // appears to be returned from the signal handler as the re-entry
  // address forJava execution to continue from. This means it needs
  // to be enterable from x86 code. Hmm, we may need to expose both an
  // x86 prolog and the address of the generated ARM code and clients
  // will have to be mdoified to pick the correct one.
  //
  // AbstractMethodError : never jumped to from generated code but the
  // generated code address appears to be returned from the signal
  // handler as the re-entry address for Java execution to continue
  // from. This means it needs to be enterable from x86 code. So, we
  // will need to provide this one with an x86 prolog as per
  // StackOverflowError
  //
  // IncompatibleClassChangeError : only appears to be jumped to
  // directly from vtableStubs code
  //
  // NullPointerException : never jumped to from generated code but
  // the generated code address appears to be returned from the signal
  // handler as the re-entry address for Java execution to continue
  // from. This means it needs to be enterable from x86 code. So, we
  // will need to provide this one with an x86 prolog as per
  // StackOverflowError


  address generate_throw_exception(const char* name,
                                   address runtime_entry,
                                   Register arg1 = noreg,
                                   Register arg2 = noreg) {
    // Information about frame layout at time of blocking runtime call.
    // Note that we only have to preserve callee-saved registers since
    // the compilers are responsible for supplying a continuation point
    // if they expect all registers to be preserved.
    // n.b. aarch64 asserts that frame::arg_reg_save_area_bytes == 0
    enum layout {
      rfp_off = 0,
      rfp_off2,
      return_off,
      return_off2,
      framesize // inclusive of return address
    };

    int insts_size = 512;
    int locs_size  = 64;

    CodeBuffer code(name, insts_size, locs_size);
    OopMapSet* oop_maps  = new OopMapSet();
    MacroAssembler* masm = new MacroAssembler(&code);

    address start = __ pc();

    // This is an inlined and slightly modified version of call_VM
    // which has the ability to fetch the return PC out of
    // thread-local storage and also sets up last_Java_sp slightly
    // differently than the real call_VM

    __ enter(); // Save FP and LR before call

    assert(is_even(framesize/2), "sp not 16-byte aligned");

    // lr and fp are already in place
    __ sub(sp, rfp, ((unsigned)framesize-4) << LogBytesPerInt); // prolog

    int frame_complete = __ pc() - start;

    // Set up last_Java_sp and last_Java_fp
    address the_pc = __ pc();
    __ set_last_Java_frame(sp, rfp, (address)NULL, rscratch1);

    // Call runtime
    if (arg1 != noreg) {
      assert(arg2 != c_rarg1, "clobbered");
      __ mov(c_rarg1, arg1);
    }
    if (arg2 != noreg) {
      __ mov(c_rarg2, arg2);
    }
    __ mov(c_rarg0, rthread);
    BLOCK_COMMENT("call runtime_entry");
    __ mov(rscratch1, runtime_entry);
    __ blrt(rscratch1, 3 /* number_of_arguments */, 0, 1);

    // Generate oop map
    OopMap* map = new OopMap(framesize, 0);

    oop_maps->add_gc_map(the_pc - start, map);

    __ reset_last_Java_frame(true, true);

    __ leave();

    // check for pending exceptions
#ifdef ASSERT
    Label L;
    __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
    __ cbnz(rscratch1, L);
    __ should_not_reach_here();
    __ bind(L);
#endif // ASSERT
    __ b(RuntimeAddress(StubRoutines::forward_exception_entry()));


    // codeBlob framesize is in words (not VMRegImpl::slot_size)
    RuntimeStub* stub =
      RuntimeStub::new_runtime_stub(name,
                                    &code,
                                    frame_complete,
                                    (framesize >> (LogBytesPerWord - LogBytesPerInt)),
                                    oop_maps, false);
    return stub->entry_point();
  }

  // Initialization
  void generate_initial() {
    // Generate initial stubs and initializes the entry points

    // entry points that exist in all platforms Note: This is code
    // that could be shared among different platforms - however the
    // benefit seems to be smaller than the disadvantage of having a
    // much more complicated generator structure. See also comment in
    // stubRoutines.hpp.

    StubRoutines::_forward_exception_entry = generate_forward_exception();

    StubRoutines::_call_stub_entry =
      generate_call_stub(StubRoutines::_call_stub_return_address);

    // is referenced by megamorphic call
    StubRoutines::_catch_exception_entry = generate_catch_exception();

    // atomic calls
    StubRoutines::_atomic_xchg_entry         = generate_atomic_xchg();
    StubRoutines::_atomic_xchg_ptr_entry     = generate_atomic_xchg_ptr();
    StubRoutines::_atomic_cmpxchg_entry      = generate_atomic_cmpxchg();
    StubRoutines::_atomic_cmpxchg_long_entry = generate_atomic_cmpxchg_long();
    StubRoutines::_atomic_add_entry          = generate_atomic_add();
    StubRoutines::_atomic_add_ptr_entry      = generate_atomic_add_ptr();
    StubRoutines::_fence_entry               = generate_orderaccess_fence();

    StubRoutines::_handler_for_unsafe_access_entry =
      generate_handler_for_unsafe_access();

    // platform dependent
    StubRoutines::x86::_get_previous_fp_entry = generate_get_previous_fp();
    StubRoutines::x86::_get_previous_sp_entry = generate_get_previous_sp();

    // Build this early so it's available for the interpreter.
    StubRoutines::_throw_StackOverflowError_entry =
      generate_throw_exception("StackOverflowError throw_exception",
                               CAST_FROM_FN_PTR(address,
                                                SharedRuntime::
                                                throw_StackOverflowError));
  }

  void generate_all() {
    // support for verify_oop (must happen after universe_init)
    StubRoutines::_verify_oop_subroutine_entry     = generate_verify_oop();
    StubRoutines::_throw_AbstractMethodError_entry =
      generate_throw_exception("AbstractMethodError throw_exception",
                               CAST_FROM_FN_PTR(address,
                                                SharedRuntime::
                                                throw_AbstractMethodError));

    StubRoutines::_throw_IncompatibleClassChangeError_entry =
      generate_throw_exception("IncompatibleClassChangeError throw_exception",
                               CAST_FROM_FN_PTR(address,
                                                SharedRuntime::
                                                throw_IncompatibleClassChangeError));

    StubRoutines::_throw_NullPointerException_at_call_entry =
      generate_throw_exception("NullPointerException at call throw_exception",
                               CAST_FROM_FN_PTR(address,
                                                SharedRuntime::
                                                throw_NullPointerException_at_call));

    // arraycopy stubs used by compilers
    generate_arraycopy_stubs();
  }

 public:
  StubGenerator(CodeBuffer* code, bool all) : StubCodeGenerator(code) {
    if (all) {
      generate_all();
    } else {
      generate_initial();
    }
  }
}; // end class declaration

void StubGenerator_generate(CodeBuffer* code, bool all) {
  StubGenerator g(code, all);
}
