/*
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/assembler.hpp"
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "oops/arrayOop.hpp"
#include "oops/methodDataOop.hpp"
#include "oops/methodOop.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/timer.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/debug.hpp"
#include <sys/types.h>

#include "../../../../../../simulator/simulator.hpp"

#define __ _masm->

#ifndef CC_INTERP

//-----------------------------------------------------------------------------

extern "C" void entry(CodeBuffer*);

//-----------------------------------------------------------------------------

address TemplateInterpreterGenerator::generate_StackOverflowError_handler() { __ call_Unimplemented(); return 0; }

address TemplateInterpreterGenerator::generate_ArrayIndexOutOfBounds_handler(
        const char* name) { __ call_Unimplemented(); return 0; }

address TemplateInterpreterGenerator::generate_ClassCastException_handler() { __ call_Unimplemented(); return 0; }

address TemplateInterpreterGenerator::generate_exception_handler_common(
        const char* name, const char* message, bool pass_oop) { __ call_Unimplemented(); return 0; }


address TemplateInterpreterGenerator::generate_continuation_for(TosState state) { __ call_Unimplemented(); return 0; }


// FIXME: I have no idea what this is supposed to be for.  It looks up
// an entry in the constant pool, ands it with 0xff, and adds that to
// the stack pointer.
address TemplateInterpreterGenerator::generate_return_entry_for(TosState state, int step) {
  address entry = __ pc();

  // Restore stack bottom in case i2c adjusted stack
  __ ldr(rscratch1, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
  __ mov(sp, rscratch1);
  // and NULL it as marker that esp is now tos until next java call
  __ str(zr, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
  __ restore_bcp();
  __ restore_locals();

  Label L_got_cache, L_giant_index;
  if (EnableInvokeDynamic) {
    // __ cmpb(Address(r13, 0), Bytecodes::_invokedynamic);
    // __ jcc(Assembler::equal, L_giant_index);
  }
  __ get_cache_and_index_at_bcp(r1, r2, sizeof(u2));
  __ bind(L_got_cache);
  __ add(rscratch1, r1, r2, Assembler::LSL, 3);
  __ ldr(r1, Address(rscratch1,
		     in_bytes(constantPoolCacheOopDesc::base_offset()) +
		     3 * wordSize));
  __ andr(r1, r1, 0xff);
  __ mov(rscratch1, sp);
  __ add(rscratch1, rscratch1, r1, Assembler::LSL, 3);
  __ mov(sp, rscratch1);
  __ dispatch_next(state, step);

  // out of the main line of code...
  if (EnableInvokeDynamic) {
    __ bind(L_giant_index);
    // __ get_cache_and_index_at_bcp(rbx, rcx, 1, sizeof(u4));
    // __ jmp(L_got_cache);
  }

  return entry;
}

address TemplateInterpreterGenerator::generate_deopt_entry_for(TosState state,
                                                               int step) { __ call_Unimplemented(); return 0; }

int AbstractInterpreter::BasicType_as_index(BasicType type) {
  int i = 0;
  switch (type) {
    case T_BOOLEAN: i = 0; break;
    case T_CHAR   : i = 1; break;
    case T_BYTE   : i = 2; break;
    case T_SHORT  : i = 3; break;
    case T_INT    : i = 4; break;
    case T_LONG   : i = 5; break;
    case T_VOID   : i = 6; break;
    case T_FLOAT  : i = 7; break;
    case T_DOUBLE : i = 8; break;
    case T_OBJECT : i = 9; break;
    case T_ARRAY  : i = 9; break;
    default       : ShouldNotReachHere();
  }
  assert(0 <= i && i < AbstractInterpreter::number_of_result_handlers,
         "index out of bounds");
  return i;
}


address TemplateInterpreterGenerator::generate_result_handler_for(
        BasicType type) {
    address entry = __ pc();
  switch (type) {
  case T_BOOLEAN: /* nothing to do */        break;
  case T_CHAR   : /* nothing to do */        break;
  case T_BYTE   : /* nothing to do */        break;
  case T_SHORT  : /* nothing to do */        break;
  case T_INT    : /* nothing to do */        break;
  case T_LONG   : /* nothing to do */        break;
  case T_VOID   : /* nothing to do */        break;
  case T_FLOAT  : /* nothing to do */        break;
  case T_DOUBLE : /* nothing to do */        break;
  case T_OBJECT :
    // retrieve result from frame
    __ ldr(r0, Address(rfp, frame::interpreter_frame_oop_temp_offset*wordSize));
    // and verify it
    __ verify_oop(r0);
    break;
  default       : ShouldNotReachHere();
  }
  __ ret(lr);                                  // return from result handler
  return entry;
}

address TemplateInterpreterGenerator::generate_safept_entry_for(
        TosState state,
        address runtime_entry) { __ call_Unimplemented(); return 0; }



// Helpers for commoning out cases in the various type of method entries.
//


// increment invocation count & check for overflow
//
// Note: checking for negative value instead of overflow
//       so we have a 'sticky' overflow test
//
// rbx: method
// ecx: invocation counter
//
void InterpreterGenerator::generate_counter_incr(
        Label* overflow,
        Label* profile_method,
        Label* profile_method_continue) {
  // FIXME: We'll need this once we have a compiler.
}

void InterpreterGenerator::generate_counter_overflow(Label* do_continue) {
  // FIXME: We'll need this once we have a compiler.
}

// See if we've got enough room on the stack for locals plus overhead.
// The expression stack grows down incrementally, so the normal guard
// page mechanism will work for that.
//
// NOTE: Since the additional locals are also always pushed (wasn't
// obvious in generate_method_entry) so the guard should work for them
// too.
//
// Args:
//      rdx: number of additional locals this frame needs (what we must check)
//      rbx: methodOop
//
// Kills:
//      rax
void InterpreterGenerator::generate_stack_overflow_check(void) {  }

// Allocate monitor and lock method (asm interpreter)
//
// Args:
//      rbx: methodOop
//      r14: locals
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ...(param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterGenerator::lock_method(void) {
  __ call_Unimplemented();
}

// Generate a fixed interpreter frame. This is identical setup for
// interpreted methods and for native methods hence the shared code.
//
// Args:
//      rax: return address
//      rmethod: methodOop
//      r14: pointer to locals
//      rscratch1: sender sp
//      rdx: cp cache
void TemplateInterpreterGenerator::generate_fixed_frame(bool native_call) {
  // initialize fixed part of activation frame
  __ push(r0);        // save return address
  __ enter();          // save old & set new rfp
  __ push(rscratch1);        // set sender sp
  __ push(zr); // leave last_sp as null
  __ ldr(rbcp, Address(rmethod, methodOopDesc::const_offset()));      // get constMethodOop
  __ ldr(rscratch1, Address(rmethod, constMethodOopDesc::codes_offset())); // get codebase
  __ add(rbcp, rbcp, rscratch1);
  __ push(rmethod);        // save methodOop
  if (ProfileInterpreter) {
    // Label method_data_continue;
    // __ movptr(rdx, Address(rbx, in_bytes(methodOopDesc::method_data_offset())));
    // __ testptr(rdx, rdx);
    // __ jcc(Assembler::zero, method_data_continue);
    // __ addptr(rdx, in_bytes(methodDataOopDesc::data_offset()));
    // __ bind(method_data_continue);
    // __ push(rdx);      // set the mdp (method data pointer)
  } else {
    __ push(zr);
  }

  __ ldr(r3, Address(rmethod, methodOopDesc::constants_offset()));
  __ ldr(r3, Address(rmethod, constantPoolOopDesc::cache_offset_in_bytes()));
  __ push(r3); // set constant pool cache
  __ push(rlocals); // set locals pointer
  if (native_call) {
    __ push(zr); // no bcp
  } else {
    __ push(rbcp); // set bcp
  }
  __ push(0); // reserve word for pointer to expression stack bottom
  __ mov(resp, sp); // set expression stack bottom
}

// End of helpers

// Various method entries
//------------------------------------------------------------------------------------------------------------------------
//
//

// Call an accessor method (assuming it is resolved, otherwise drop
// into vanilla (slow path) entry
address InterpreterGenerator::generate_accessor_entry(void) {
  return NULL;
}

// Method entry for java.lang.ref.Reference.get.
address InterpreterGenerator::generate_Reference_get_entry(void) {
  return NULL;
}

// Interpreter stub for calling a native method. (asm interpreter)
// This sets up a somewhat different looking stack for calling the
// native method than the typical interpreter frame setup.
address InterpreterGenerator::generate_native_entry(bool synchronized) {
  // determine code generation flags
  bool inc_counter  = UseCompiler || CountCompiledCalls;

  // rmethod: methodOop
  // rscratch1: sender sp

  address entry_point = __ pc();

  const Address size_of_parameters(rmethod, methodOopDesc::
                                        size_of_parameters_offset());
  const Address invocation_counter(rmethod, methodOopDesc::
                                        invocation_counter_offset() +
                                        InvocationCounter::counter_offset());
  const Address access_flags      (rmethod, methodOopDesc::access_flags_offset());

  // get parameter size (always needed)
  __ load_unsigned_short(r2, size_of_parameters);

  // native calls don't need the stack size check since they have no
  // expression stack and the arguments are already on the stack and
  // we only add a handful of words to the stack

  // rmethod: methodOop
  // r2: size of parameters
  // rscratch1: sender sp
  __ pop(r0);                                       // get return address

  // for natives the size of locals is zero

  // compute beginning of parameters (rlocals)
  __ add(rlocals, sp, r2, Assembler::LSL, 3);
  __ add(rlocals, rlocals, wordSize);

  // add 2 zero-initialized slots for native calls
  // initialize result_handler slot
  __ push(zr);
  // slot for oop temp
  // (static native method holder mirror/jni oop result)
  __ push(zr);

  if (inc_counter) {
    __ prfm(invocation_counter);  // (pre-)fetch invocation count
  }

  // initialize fixed part of activation frame
  generate_fixed_frame(true);

  // make sure method is native & not abstract
#ifdef ASSERT
  __ ldr(r0, access_flags);
  {
    Label L;
    __ tst(r0, JVM_ACC_NATIVE);
    __ br(Assembler::NE, L);
    __ stop("tried to execute non-native method as native");
    __ bind(L);
  }
  {
    Label L;
    __ tst(r0, JVM_ACC_ABSTRACT);
    __ br(Assembler::EQ, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception
  // handler would try to exit the monitor of synchronized methods
  // which hasn't been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation
  // will check this flag.

  __ add(rscratch1, rthread,
	 in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  __ mov(rscratch2, true);
  __ strb(rscratch2, rscratch1);
  
  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
  if (inc_counter) {
    generate_counter_incr(&invocation_counter_overflow, NULL, NULL);
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);

  bang_stack_shadow_pages(true);

  // reset the _do_not_unlock_if_synchronized flag
  __ add(rscratch1, rthread,
	 in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  __ strb(zr, rscratch1);

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
  if (synchronized) {
    lock_method();
  } else {
    // no synchronization necessary
#ifdef ASSERT
    {
      Label L;
      __ ldr(r0, access_flags);
      __ tst(r0, JVM_ACC_SYNCHRONIZED);
      __ br(Assembler::EQ, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // start execution
#ifdef ASSERT
  {
    Label L;
    const Address monitor_block_top(rfp,
                 frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ ldr(r0, monitor_block_top);
    __ cmp(r0, sp);  // FIXME: rsp or resp??
    __ br(Assembler::EQ, L);
    __ stop("broken stack frame setup in interpreter");
    __ bind(L);
  }
#endif

  // jvmti support
  __ notify_method_entry();

  // work registers
  const Register t = r17;

  // allocate space for parameters
  __ verify_oop(rmethod);
  __ load_unsigned_short(t,
                         Address(rmethod,
                                 methodOopDesc::size_of_parameters_offset()));
  __ lsl(t, t, Interpreter::logStackElementSize);

  __ sub(sp, sp, t);
  __ sub(sp, sp, frame::arg_reg_save_area_bytes); // windows
  __ andr(sp, sp, -16); // must be 16 byte boundary

  // get signature handler
  {
    Label L;
    __ ldr(t, Address(rmethod, methodOopDesc::signature_handler_offset()));
    __ cbz(t, L);
    __ call_VM(noreg,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::prepare_native_call),
               rmethod);
    __ ldr(t, Address(rmethod, methodOopDesc::signature_handler_offset()));
    __ bind(L);
  }

  // call signature handler
  assert(InterpreterRuntime::SignatureHandlerGenerator::from() == rscratch2,
         "adjust this code");
  assert(InterpreterRuntime::SignatureHandlerGenerator::to() == sp,
         "adjust this code");
  assert(InterpreterRuntime::SignatureHandlerGenerator::temp() == rscratch1,
          "adjust this code");

  // The generated handlers do not touch RBX (the method oop).
  // However, large signatures cannot be cached and are generated
  // each time here.  The slow-path generator can do a GC on return,
  // so we must reload it after the call.
  __ call(t);
  __ get_method(rmethod);        // slow path can do a GC, reload RBX


  // result handler is in rax
  // set result handler
  __ ldr(r0, Address(rfp,
		     (frame::interpreter_frame_result_handler_offset) * wordSize));

  // pass mirror handle if static call
  {
    Label L;
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    __ ldr(t, Address(rmethod, methodOopDesc::access_flags_offset()));
    __ tst(t, JVM_ACC_STATIC);
    __ br(Assembler::EQ, L);
    // get mirror
    __ ldr(t, Address(rmethod, methodOopDesc::constants_offset()));
    __ ldr(t, Address(t, constantPoolOopDesc::pool_holder_offset_in_bytes()));
    __ ldr(t, Address(t, mirror_offset));
    // copy mirror into activation frame
    __ str(t, Address(rfp, frame::interpreter_frame_oop_temp_offset * wordSize));
    // pass handle to mirror
    __ add(c_rarg1, rfp, frame::interpreter_frame_oop_temp_offset * wordSize);
    __ bind(L);
  }

  // get native function entry point
  {
    Label L;
    __ ldr(r0, Address(rmethod, methodOopDesc::native_function_offset()));
    address unsatisfied = (SharedRuntime::native_method_throw_unsatisfied_link_error_entry());
    __ mov(rscratch2, unsatisfied);
    __ ldr(rscratch2, rscratch2);
    __ cmp(r0, rscratch2);
    __ br(Assembler::NE, L);
    __ call_VM(noreg,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::prepare_native_call),
               rmethod);
    __ get_method(rmethod);
    __ verify_oop(rmethod);
    __ ldr(r0, Address(rmethod, methodOopDesc::native_function_offset()));
    __ bind(L);
  }

  // pass JNIEnv
  __ add(c_rarg0, rthread, in_bytes(JavaThread::jni_environment_offset()));

  // It is enough that the pc() points into the right code
  // segment. It does not have to be the correct return pc.
  __ set_last_Java_frame(sp, rfp, (address) __ pc());

  // change thread state
#ifdef ASSERT
  {
    Label L;
    __ ldr(t, Address(rthread, JavaThread::thread_state_offset()));
    __ cmp(t, _thread_in_Java);
    __ br(Assembler::EQ, L);
    __ stop("Wrong thread state in native stub");
    __ bind(L);
  }
#endif

  // Change state to native
  __ mov(rscratch1, _thread_in_native);
  __ str(rscratch1, Address(rthread, JavaThread::thread_state_offset()));

  // Call the native method.
  __ blr(r0);
  // result potentially in rax or xmm0

  // NOTE: The order of these pushes is known to frame::interpreter_frame_result
  // in order to extract the result of a method call. If the order of these
  // pushes change or anything else is added to the stack then the code in
  // interpreter_frame_result must also change.

  __ push(dtos);
  __ push(ltos);

  // change thread state
  __ mov(rscratch1, _thread_in_native_trans);
  __ str(rscratch1, Address(rthread, JavaThread::thread_state_offset()));

  if (os::is_MP()) {
    if (UseMembar) {
      // Force this write out before the read below
      __ dmb(Assembler::SY);
    } else {
      // Write serialization page so VM thread can do a pseudo remote membar.
      // We use the current thread pointer to calculate a thread specific
      // offset to write to within the page. This minimizes bus traffic
      // due to cache line collision.
      __ serialize_memory(rthread, rscratch2);
    }
  }

  // check for safepoint operation in progress and/or pending suspend requests
  {
    Label Continue;
    __ mov(rscratch2, SafepointSynchronize::address_of_state());
    __ ldr(rscratch2, rscratch2);
    __ cmp(rscratch2, SafepointSynchronize::_not_synchronized);

    Label L;
    __ br(Assembler::NE, L);
    __ ldrw(rscratch2, Address(rthread, JavaThread::suspend_flags_offset()));
    __ cbz(rscratch2, Continue);
    __ bind(L);

    // Don't use call_VM as it will see a possible pending exception
    // and forward it and never return here preventing us from
    // clearing _last_native_pc down below.  Also can't use
    // call_VM_leaf either as it will check to see if r13 & r14 are
    // preserved and correspond to the bcp/locals pointers. So we do a
    // runtime call by hand.
    //
    __ mov(c_rarg0, rthread);
    __ mov(r16, sp); // remember sp
    __ sub(rscratch1, sp, frame::arg_reg_save_area_bytes); // windows
    __ andr(rscratch1, rscratch1, -16); // align stack as required by ABI
    __ mov(sp, rscratch1);
    __ bl((CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans)));
    __ mov(sp, r16); // restore sp
    __ reinit_heapbase();
    __ bind(Continue);
  }

  // change thread state
  __ mov(rscratch1, _thread_in_Java);
  __ str(rscratch1, Address(rthread, JavaThread::thread_state_offset()));

  // reset_last_Java_frame
  __ reset_last_Java_frame(true, true);

  // reset handle block
  __ ldr(t, Address(rthread, JavaThread::active_handles_offset()));
  __ str(zr, Address(t, JNIHandleBlock::top_offset_in_bytes()));

  // If result is an oop unbox and store it in frame where gc will see it
  // and result handler will pick it up

  {
    Label no_oop, store_result;
    __ mov(t, (AbstractInterpreter::result_handler(T_OBJECT)));
    __ ldr(rscratch1, Address(rfp, frame::interpreter_frame_result_handler_offset*wordSize));
    __ cmp(t, rscratch1);
    __ br(Assembler::NE, no_oop);
    // retrieve result
    __ pop(ltos);
    __ cbz(r0, store_result);
    __ ldr(r0, Address(r0, 0));
    __ bind(store_result);
    __ ldr(r0, Address(rfp, frame::interpreter_frame_oop_temp_offset*wordSize));
    // keep stack depth as expected by pushing oop which will eventually be discarde
    __ push(ltos);
    __ bind(no_oop);
  }


  {
    Label no_reguard;
    __ add(rscratch1, rthread, in_bytes(JavaThread::stack_guard_state_offset()));
    __ ldr(rscratch1, rscratch1);
    __ cmp(rscratch1, JavaThread::stack_guard_yellow_disabled);
    __ br(Assembler::NE, no_reguard);

    __ pusha(); // XXX only save smashed registers
    __ mov(c_rarg0, rthread);
    __ mov(r16, sp); // remember sp
    __ sub(rscratch1, sp, frame::arg_reg_save_area_bytes); // windows
    __ andr(rscratch1, rscratch1, -16); // align stack as required by ABI
    __ mov(sp, rscratch1);
    __ bl((CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages)));
    __ mov(sp, r16); // restore sp
    __ popa(); // XXX only restore smashed registers
    __ reinit_heapbase();
    __ bind(no_reguard);
  }


  // The method register is junk from after the thread_in_native transition
  // until here.  Also can't call_VM until the bcp has been
  // restored.  Need bcp for throwing exception below so get it now.
  __ get_method(rmethod);
  __ verify_oop(rmethod);

  // restore bcp to have legal interpreter frame, i.e., bci == 0 <=>
  // rbcp == code_base()
  __ ldr(rbcp, Address(rmethod, methodOopDesc::const_offset()));   // get constMethodOop
  __ add(rbcp, rbcp, in_bytes(constMethodOopDesc::codes_offset()));          // get codebase
  // handle exceptions (exception handling will handle unlocking!)
  {
    Label L;
    __ ldr(rscratch1, Address(rthread, Thread::pending_exception_offset()));
    __ cbz(rscratch1, L);
    // Note: At some point we may want to unify this with the code
    // used in call_VM_base(); i.e., we should use the
    // StubRoutines::forward_exception code. For now this doesn't work
    // here because the rsp is not correctly set at this point.
    __ MacroAssembler::call_VM(noreg,
                               CAST_FROM_FN_PTR(address,
                               InterpreterRuntime::throw_pending_exception));
    __ should_not_reach_here();
    __ bind(L);
  }

  // do unlocking if necessary
  {
    Label L;
    __ ldr(t, Address(rmethod, methodOopDesc::access_flags_offset()));
    __ tst(t, JVM_ACC_SYNCHRONIZED);
    __ br(Assembler::EQ, L);
    // the code below should be shared with interpreter macro
    // assembler implementation
    {
      Label unlock;
      // BasicObjectLock will be first in list, since this is a
      // synchronized method. However, need to check that the object
      // has not been unlocked by an explicit monitorexit bytecode.

      // monitor expect in c_rarg1 for slow unlock path
      __ ldr (c_rarg1, Address(rfp,   // address of first monitor
			       (intptr_t)(frame::interpreter_frame_initial_sp_offset *
					  wordSize - sizeof(BasicObjectLock))));

      __ ldr(t, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
      __ cbz(t, unlock);

      // Entry already unlocked, need to throw exception
      __ MacroAssembler::call_VM(noreg,
                                 CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
      __ should_not_reach_here();

      __ bind(unlock);
      __ unlock_object(c_rarg1);
    }
    __ bind(L);
  }

  // jvmti support
  // Note: This must happen _after_ handling/throwing any exceptions since
  //       the exception handler code notifies the runtime of method exits
  //       too. If this happens before, method entry/exit notifications are
  //       not properly paired (was bug - gri 11/22/99).
  __ notify_method_exit(vtos, InterpreterMacroAssembler::NotifyJVMTI);

  // restore potential result in edx:eax, call result handler to
  // restore potential result in ST0 & handle result

  __ pop(ltos);
  __ pop(dtos);

  __ ldr(t, Address(rfp,
		    (frame::interpreter_frame_result_handler_offset) * wordSize));
  __ br(t);

  // remove activation
  __ ldr(t, Address(rfp,
		    frame::interpreter_frame_sender_sp_offset *
		    wordSize)); // get sender sp
  __ leave();                                // remove frame anchor
  __ pop(rscratch1);                         // get return address
  __ mov(sp, t);                             // set sp to sender sp
  __ br(rscratch1);

  if (inc_counter) {
    // Handle overflow of counter and compile method
    __ bind(invocation_counter_overflow);
    generate_counter_overflow(&continue_after_compile);
  }

  return entry_point;
}

//
// Generic interpreted method entry to (asm) interpreter
//
address InterpreterGenerator::generate_normal_entry(bool synchronized) {
  // determine code generation flags
  bool inc_counter  = UseCompiler || CountCompiledCalls;

  // rscratch1: sender sp
  address entry_point = __ pc();

  const Address size_of_parameters(rmethod,
                                   methodOopDesc::size_of_parameters_offset());
  const Address size_of_locals(rmethod, methodOopDesc::size_of_locals_offset());
  const Address invocation_counter(rmethod,
                                   methodOopDesc::invocation_counter_offset() +
                                   InvocationCounter::counter_offset());
  const Address access_flags(r1, methodOopDesc::access_flags_offset());

  // get parameter size (always needed)
  __ load_unsigned_short(r2, size_of_parameters);

  // r1: methodOop
  // r2: size of parameters
  // rscratch1: sender_sp (could differ from sp+wordSize if we were called via c2i )

  __ load_unsigned_short(r3, size_of_locals); // get size of locals in words
  __ sub(r3, r2, r2); // r3 = no. of additional locals

  // YYY
//   __ incrementl(rdx);
//   __ andl(rdx, -2);

  // see if we've got enough room on the stack for locals plus overhead.
  generate_stack_overflow_check();

  // get return address
  __ pop(r0);

  // compute beginning of parameters (rlocals)
  __ add(rlocals, sp, r2, Assembler::LSL, 3);
  __ add(rlocals, rlocals, wordSize);

  // rdx - # of additional locals
  // allocate space for locals
  // explicitly initialize locals
  {
    Label exit, loop;
    __ ands(r3, r3, r3);
    __ br(Assembler::LE, exit); // do nothing if rdx <= 0
    __ bind(loop);
    __ push((int) NULL_WORD); // initialize local variables
    __ subs(r3, r3, 1); // until everything initialized
    __ br(Assembler::GT, loop);
    __ bind(exit);
  }

  // (pre-)fetch invocation count
  if (inc_counter) {
    __ ldr(r2, invocation_counter);
  }
  // initialize fixed part of activation frame
  generate_fixed_frame(false);

  // make sure method is not native & not abstract
#ifdef ASSERT
  __ ldr(r0, access_flags);
  {
    Label L;
    __ tst(r0, JVM_ACC_NATIVE);
    __ br(Assembler::EQ, L);
    __ stop("tried to execute native method as non-native");
    __ bind(L);
  }
  {
    Label L;
    __ tst(r0, JVM_ACC_ABSTRACT);
    __ br(Assembler::EQ, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception
  // handler would try to exit the monitor of synchronized methods
  // which hasn't been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation
  // will check this flag.

  __ add(rscratch1, rthread,
	 in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  __ mov(rscratch2, true);
  __ strb(rscratch2, rscratch1);
  
  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
  Label profile_method;
  Label profile_method_continue;
  if (inc_counter) {
    generate_counter_incr(&invocation_counter_overflow,
                          &profile_method,
                          &profile_method_continue);
    if (ProfileInterpreter) {
      __ bind(profile_method_continue);
    }
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);

  // check for synchronized interpreted methods
  bang_stack_shadow_pages(false);

  // reset the _do_not_unlock_if_synchronized flag
  __ add(rscratch1, rthread,
	 in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  __ strb(zr, rscratch1);

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
  if (synchronized) {
    // Allocate monitor and lock method
    lock_method();
  } else {
    // no synchronization necessary
#ifdef ASSERT
    {
      Label L;
      __ ldr(r0, access_flags);
      __ tst(r0, JVM_ACC_SYNCHRONIZED);
      __ br(Assembler::EQ, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // start execution
#ifdef ASSERT
  {
    Label L;
     const Address monitor_block_top (rfp,
                 frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ ldr(r0, monitor_block_top);
    __ cmp(r0, sp);
    __ br(Assembler::EQ, L);
    __ stop("broken stack frame setup in interpreter");
    __ bind(L);
  }
#endif

  // jvmti support
  __ notify_method_entry();

  __ dispatch_next(vtos);

  // invocation counter overflow
  if (inc_counter) {
    if (ProfileInterpreter) {
      // We have decided to profile this method in the interpreter
      __ bind(profile_method);
      __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::profile_method));
      __ set_method_data_pointer_for_bcp();
      __ get_method(r1);
      __ b(profile_method_continue);
    }
    // Handle overflow of counter and compile method
    __ bind(invocation_counter_overflow);
    generate_counter_overflow(&continue_after_compile);
  }

  return entry_point;
}

// Entry points
//
// Here we generate the various kind of entries into the interpreter.
// The two main entry type are generic bytecode methods and native
// call method.  These both come in synchronized and non-synchronized
// versions but the frame layout they create is very similar. The
// other method entry types are really just special purpose entries
// that are really entry and interpretation all in one. These are for
// trivial methods like accessor, empty, or special math methods.
//
// When control flow reaches any of the entry types for the interpreter
// the following holds ->
//
// Arguments:
//
// rbx: methodOop
//
// Stack layout immediately at entry
//
// [ return address     ] <--- rsp
// [ parameter n        ]
//   ...
// [ parameter 1        ]
// [ expression stack   ] (caller's java expression stack)

// Assuming that we don't go to one of the trivial specialized entries
// the stack will look like below when we are ready to execute the
// first bytecode (or call the native routine). The register usage
// will be as the template based interpreter expects (see
// interpreter_amd64.hpp).
//
// local variables follow incoming parameters immediately; i.e.
// the return address is moved to the end of the locals).
//
// [ monitor entry      ] <--- rsp
//   ...
// [ monitor entry      ]
// [ expr. stack bottom ]
// [ saved r13          ]
// [ current r14        ]
// [ methodOop          ]
// [ saved ebp          ] <--- rbp
// [ return address     ]
// [ local variable m   ]
//   ...
// [ local variable 1   ]
// [ parameter n        ]
//   ...
// [ parameter 1        ] <--- r14

address AbstractInterpreterGenerator::generate_method_entry(
                                        AbstractInterpreter::MethodKind kind) {
  // determine code generation flags
  bool synchronized = false;
  address entry_point = NULL;

  switch (kind) {
  case Interpreter::zerolocals             :                                                                             break;
  case Interpreter::zerolocals_synchronized: synchronized = true;                                                        break;
  case Interpreter::native                 : entry_point = ((InterpreterGenerator*) this)->generate_native_entry(false); break;
  case Interpreter::native_synchronized    : entry_point = ((InterpreterGenerator*) this)->generate_native_entry(true);  break;
  case Interpreter::empty                  : entry_point = ((InterpreterGenerator*) this)->generate_empty_entry();       break;
  case Interpreter::accessor               : entry_point = ((InterpreterGenerator*) this)->generate_accessor_entry();    break;
  case Interpreter::abstract               : entry_point = ((InterpreterGenerator*) this)->generate_abstract_entry();    break;
  case Interpreter::method_handle          : entry_point = ((InterpreterGenerator*) this)->generate_method_handle_entry();break;

  case Interpreter::java_lang_math_sin     : // fall thru
  case Interpreter::java_lang_math_cos     : // fall thru
  case Interpreter::java_lang_math_tan     : // fall thru
  case Interpreter::java_lang_math_abs     : // fall thru
  case Interpreter::java_lang_math_log     : // fall thru
  case Interpreter::java_lang_math_log10   : // fall thru
  case Interpreter::java_lang_math_sqrt    : // fall thru
  case Interpreter::java_lang_math_pow     : // fall thru
  case Interpreter::java_lang_math_exp     : entry_point = ((InterpreterGenerator*) this)->generate_math_entry(kind);    break;
  case Interpreter::java_lang_ref_reference_get
                                           : entry_point = ((InterpreterGenerator*)this)->generate_Reference_get_entry(); break;
  default                                  : ShouldNotReachHere();                                                       break;
  }

  if (entry_point) {
    return entry_point;
  }

  return ((InterpreterGenerator*) this)->
                                generate_normal_entry(synchronized);
}


// These should never be compiled since the interpreter will prefer
// the compiled version to the intrinsic version.
bool AbstractInterpreter::can_be_compiled(methodHandle m) { return false; }

// How much stack a method activation needs in words.
int AbstractInterpreter::size_top_interpreter_activation(methodOop method) {
  const int entry_size = frame::interpreter_frame_monitor_size();

  // total overhead size: entry_size + (saved rfp thru expr stack
  // bottom).  be sure to change this if you add/subtract anything
  // to/from the overhead area
  const int overhead_size =
    -(frame::interpreter_frame_initial_sp_offset) + entry_size;

  const int stub_code = frame::entry_frame_after_call_words;
  const int extra_stack = methodOopDesc::extra_stack_entries();
  const int method_stack = (method->max_locals() + method->max_stack() + extra_stack) *
                           Interpreter::stackElementWords;
  return (overhead_size + method_stack + stub_code);
}

int AbstractInterpreter::layout_activation(methodOop method,
                                           int tempcount,
                                           int popframe_extra_args,
                                           int moncount,
                                           int caller_actual_parameters,
                                           int callee_param_count,
                                           int callee_locals,
                                           frame* caller,
                                           frame* interpreter_frame,
                                           bool is_top_frame) { Unimplemented(); return 0; }

//-----------------------------------------------------------------------------
// Exceptions

void TemplateInterpreterGenerator::generate_throw_exception() { __ call_Unimplemented(); }


//
// JVMTI ForceEarlyReturn support
//
address TemplateInterpreterGenerator::generate_earlyret_entry_for(TosState state) { __ call_Unimplemented(); return 0; }
// end of ForceEarlyReturn support


//-----------------------------------------------------------------------------
// Helper for vtos entry point generation

void TemplateInterpreterGenerator::set_vtos_entry_points(Template* t,
                                                         address& bep,
                                                         address& cep,
                                                         address& sep,
                                                         address& aep,
                                                         address& iep,
                                                         address& lep,
                                                         address& fep,
                                                         address& dep,
                                                         address& vep) {
  assert(t->is_valid() && t->tos_in() == vtos, "illegal template");
  Label L;
  aep = __ pc();  __ push_ptr();  __ b(L);
  fep = __ pc();  __ push_f();    __ b(L);
  dep = __ pc();  __ push_d();    __ b(L);
  lep = __ pc();  __ push_l();    __ b(L);
  bep = cep = sep =
  iep = __ pc();  __ push_i();
  vep = __ pc();
  __ bind(L);
  generate_and_dispatch(t);
}

//-----------------------------------------------------------------------------
// Generation of individual instructions

// helpers for generate_and_dispatch


InterpreterGenerator::InterpreterGenerator(StubQueue* code)
  : TemplateInterpreterGenerator(code) {
   generate_all(); // down here so it can be "virtual"
}

//-----------------------------------------------------------------------------

// Non-product code
#ifndef PRODUCT
address TemplateInterpreterGenerator::generate_trace_code(TosState state) { Unimplemented(); return 0; }

void TemplateInterpreterGenerator::count_bytecode() { Unimplemented(); }

void TemplateInterpreterGenerator::histogram_bytecode(Template* t) { Unimplemented(); }

void TemplateInterpreterGenerator::histogram_bytecode_pair(Template* t) { Unimplemented(); }


void TemplateInterpreterGenerator::trace_bytecode(Template* t) { Unimplemented(); }


void TemplateInterpreterGenerator::stop_interpreter_at() { Unimplemented(); }
#endif // !PRODUCT
#endif // ! CC_INTERP
