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
#include "interp_masm_aarch64.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
#include "oops/methodDataOop.hpp"
#include "oops/methodOop.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiRedefineClassesTrace.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/sharedRuntime.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "thread_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "thread_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "thread_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "thread_bsd.inline.hpp"
#endif


// Implementation of InterpreterMacroAssembler

#ifdef CC_INTERP
void InterpreterMacroAssembler::get_method(Register reg) { Unimplemented(); }
#endif // CC_INTERP

#ifndef CC_INTERP

void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) {
  if (JvmtiExport::can_pop_frame()) {
    Unimplemented(); 
  }
}

void InterpreterMacroAssembler::load_earlyret_value(TosState state) {
  if (JvmtiExport::can_force_early_return()) {
    Unimplemented();
  }
}

void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) {
    if (JvmtiExport::can_force_early_return()) {
      Unimplemented();
    }
}

void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(
  Register reg,
  int bcp_offset) {
  assert(bcp_offset >= 0, "bcp is still pointing to start of bytecode");
  ldrh(reg, Address(rbcp, bcp_offset));
  rev16(reg, reg);
}

void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  if (index_size == sizeof(u2)) {
    load_unsigned_short(index, Address(r13, bcp_offset));
  } else if (index_size == sizeof(u4)) {
    Unimplemented();
    assert(EnableInvokeDynamic, "giant index used only for JSR 292");
  } else if (index_size == sizeof(u1)) {
    assert(EnableInvokeDynamic, "tiny index used only for JSR 292");
    Unimplemented();
  } else {
    ShouldNotReachHere();
  }
}

void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) {
  assert_different_registers(cache, index);
  get_cache_index_at_bcp(index, bcp_offset, index_size);
  if (cache != rcpool)
    mov(cache, rcpool);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  lsl(index, index, 2);
}


void InterpreterMacroAssembler::get_cache_and_index_and_bytecode_at_bcp(Register cache,
                                                                        Register index,
                                                                        Register bytecode,
                                                                        int byte_no,
                                                                        int bcp_offset,
                                                                        size_t index_size) {
  get_cache_and_index_at_bcp(cache, index, bcp_offset, index_size);
  // We use a 32-bit load here since the layout of 64-bit words on
  // little-endian machines allow us that.
  add(rscratch1, cache, index, Assembler::LSL, 3);
  ldrw(bytecode, Address(rscratch1,
			 constantPoolCacheOopDesc::base_offset()
			 + ConstantPoolCacheEntry::indices_offset()));
  const int shift_count = (1 + byte_no) * BitsPerByte;
  ubfx(bytecode, bytecode, shift_count, BitsPerByte);
}

void InterpreterMacroAssembler::get_cache_entry_pointer_at_bcp(Register cache,
                                                               Register tmp,
                                                               int bcp_offset,
                                                               size_t index_size) { Unimplemented(); }


// Generate a subtype check: branch to ok_is_subtype if sub_klass is a
// subtype of super_klass.
//
// Args:
//      rax: superklass
//      Rsub_klass: subklass
//
// Kills:
//      rcx, rdi
void InterpreterMacroAssembler::gen_subtype_check(Register Rsub_klass,
                                                  Label& ok_is_subtype) { Unimplemented(); }



// Java Expression Stack

void InterpreterMacroAssembler::pop_ptr(Register r) {
  ldr(r, post(sp, wordSize));
}

void InterpreterMacroAssembler::pop_i(Register r) {
  ldrw(r, post(sp, wordSize));
}

void InterpreterMacroAssembler::pop_l(Register r) {
  ldr(r, post(sp, 2 * Interpreter::stackElementSize));
}

void InterpreterMacroAssembler::push_ptr(Register r) {
  str(r, pre(sp, -wordSize));
 }

void InterpreterMacroAssembler::push_i(Register r) {
  str(r, pre(sp, -wordSize));
}

void InterpreterMacroAssembler::push_l(Register r) {
  str(r, pre(sp, 2 * -wordSize));
}

void InterpreterMacroAssembler::pop_f(FloatRegister r) {
  ldrs(r, post(sp, wordSize));
}

void InterpreterMacroAssembler::pop_d(FloatRegister r) {
  ldrd(r, post(sp, 2 * Interpreter::stackElementSize));
}

void InterpreterMacroAssembler::push_f(FloatRegister r) {
  strs(r, pre(sp, -wordSize));
}

void InterpreterMacroAssembler::push_d(FloatRegister r) {
  strd(r, pre(sp, 2* -wordSize));
}

void InterpreterMacroAssembler::pop(TosState state) {
  switch (state) {
  case atos: pop_ptr();                 break;
  case btos:
  case ctos:
  case stos:
  case itos: pop_i();                   break;
  case ltos: pop_l();                   break;
  case ftos: pop_f();                   break;
  case dtos: pop_d();                   break;
  case vtos: /* nothing to do */        break;
  default:   ShouldNotReachHere();
  }
  verify_oop(r0, state);
}

void InterpreterMacroAssembler::push(TosState state) {
  verify_oop(r0, state);
  switch (state) {
  case atos: push_ptr();                break;
  case btos:
  case ctos:
  case stos:
  case itos: push_i();                  break;
  case ltos: push_l();                  break;
  case ftos: push_f();                  break;
  case dtos: push_d();                  break;
  case vtos: /* nothing to do */        break;
  default  : ShouldNotReachHere();
  }
}

// Helpers for swap and dup
void InterpreterMacroAssembler::load_ptr(int n, Register val) { Unimplemented(); }

void InterpreterMacroAssembler::store_ptr(int n, Register val) { Unimplemented(); }

void InterpreterMacroAssembler::prepare_to_jump_from_interpreted() {
  // record last_sp
  str(r10, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
}

// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) {
  prepare_to_jump_from_interpreted();

  if (JvmtiExport::can_post_interpreter_events()) {
    Label run_compiled_code;
    // JVMTI events, such as single-stepping, are implemented partly by avoiding running
    // compiled code in threads for which the event is enabled.  Check here for
    // interp_only_mode if these events CAN be enabled.
    // interp_only is an int, on little endian it is sufficient to test the byte only
    // Is a cmpl faster?
    ldr(rscratch1, Address(rthread, JavaThread::interp_only_mode_offset()));
    cbz(rscratch1, run_compiled_code);
    ldr(rscratch1, Address(method, methodOopDesc::interpreter_entry_offset()));
    br(rscratch1);
    bind(run_compiled_code);
  }

  ldr(rscratch1, Address(method, methodOopDesc::from_interpreted_offset()));
  br(rscratch1);
}

// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts.  amd64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) {
}

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) {
    dispatch_next(state, step);
}

void InterpreterMacroAssembler::dispatch_base(TosState state,
                                              address* table,
                                              bool verifyoop) {
    if (VerifyActivationFrameSize) {
      Unimplemented();
    }
    if (verifyoop) {
      verify_oop(r0, state);
    }
    mov(rscratch2, (address)table);
    ldr(rscratch2, Address(rscratch2, rscratch1, Address::lsl(3)));
    br(rscratch2);
}

void InterpreterMacroAssembler::dispatch_only(TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_only_normal(TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_only_noverify(TosState state) { Unimplemented(); }


void InterpreterMacroAssembler::dispatch_next(TosState state, int step) {
  // load next bytecode
  ldrb(rscratch1, Address(pre(rbcp, step)));
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_via(TosState state, address* table) { Unimplemented(); }

// remove activation
//
// Unlock the receiver if this is a synchronized method.
// Unlock any Java monitors from syncronized blocks.
// Remove the activation from the stack.
//
// If there are locked Java monitors
//    If throw_monitor_exception
//       throws IllegalMonitorStateException
//    Else if install_monitor_exception
//       installs IllegalMonitorStateException
//    Else
//       no error processing
void InterpreterMacroAssembler::remove_activation(
        TosState state,
        bool throw_monitor_exception,
        bool install_monitor_exception,
        bool notify_jvmdi) {
  // Note: Registers r3 xmm0 may be in use for the
  // result check if synchronized method
  Label unlocked, unlock, no_unlock;

  // get the value of _do_not_unlock_if_synchronized into r3
  const Address do_not_unlock_if_synchronized(rthread,
    in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  ldrb(r3, do_not_unlock_if_synchronized);
  strb(zr, do_not_unlock_if_synchronized); // reset the flag

 // get method access flags
  ldr(r1, Address(rfp, frame::interpreter_frame_method_offset * wordSize));
  ldr(r2, Address(r1, methodOopDesc::access_flags_offset()));
  tst(r2, JVM_ACC_SYNCHRONIZED);
  br(Assembler::EQ, unlocked);

  // Don't unlock anything if the _do_not_unlock_if_synchronized flag
  // is set.
  cbz(r3, no_unlock);

  call_Unimplemented();

  bind(unlock);
  unlock_object(c_rarg1);

  // Check that for block-structured locking (i.e., that all locked
  // objects has been unlocked)
  bind(unlocked);

  // rax: Might contain return value

  // Check that all monitors are unlocked
  {
    Label loop, exception, entry, restart;
    const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
    const Address monitor_block_top(
        rfp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    const Address monitor_block_bot(
        rfp, frame::interpreter_frame_initial_sp_offset * wordSize);

    bind(restart);
    // We use c_rarg1 so that if we go slow path it will be the correct
    // register for unlock_object to pass to VM directly
    ldr(r10, monitor_block_top); // points to current entry, starting
                                  // with top-most entry
    sub(r1, rfp, -frame::interpreter_frame_initial_sp_offset * wordSize);  // points to word before bottom of
                                  // monitor block
    b(entry);

    // Entry already locked, need to throw exception
    bind(exception);

    if (throw_monitor_exception) {
      // Throw exception
      MacroAssembler::call_VM(noreg,
                              CAST_FROM_FN_PTR(address, InterpreterRuntime::
                                   throw_illegal_monitor_state_exception));
      should_not_reach_here();
    } else {
      // Stack unrolling. Unlock object and install illegal_monitor_exception.
      // Unlock does not block, so don't have to worry about the frame.
      // We don't have to preserve c_rarg1 since we are going to throw an exception.

      push(state);
      unlock_object(c_rarg1);
      pop(state);

      if (install_monitor_exception) {
        call_VM(noreg, CAST_FROM_FN_PTR(address,
                                        InterpreterRuntime::
                                        new_illegal_monitor_state_exception));
      }

      b(restart);
    }

    bind(loop);
    // check if current entry is used
    ldr(rscratch1, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
    cbnz(rscratch1, exception);

    add(c_rarg1, c_rarg1, entry_size); // otherwise advance to next entry
    bind(entry);
    cmp(c_rarg1, r10); // check if bottom reached
    br(Assembler::NE, loop); // if not at bottom then check this entry
  }

  bind(no_unlock);

  // jvmti support
  if (notify_jvmdi) {
    notify_method_exit(state, NotifyJVMTI);    // preserve TOSCA
  } else {
    notify_method_exit(state, SkipNotifyJVMTI); // preserve TOSCA
  }

  // remove activation
  // get sender sp
  ldr(r1,
      Address(rfp, frame::interpreter_frame_sender_sp_offset * wordSize));
  leave();                           // remove frame anchor
  mov(sp, r1);                      // set sp to sender sp
}

#endif // C_INTERP

// Lock object
//
// Args:
//      c_rarg1: BasicObjectLock to be used for locking
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, .. (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::lock_object(Register lock_reg) { Unimplemented(); }


// Unlocks an object. Used in monitorexit bytecode and
// remove_activation.  Throws an IllegalMonitorException if object is
// not locked by current thread.
//
// Args:
//      c_rarg1: BasicObjectLock for lock
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ... (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::unlock_object(Register lock_reg) { call_Unimplemented(); }

#ifndef CC_INTERP

void InterpreterMacroAssembler::test_method_data_pointer(Register mdp,
                                                         Label& zero_continue) { Unimplemented(); }


// Set the method data pointer for the current bcp.
void InterpreterMacroAssembler::set_method_data_pointer_for_bcp() { Unimplemented(); }

void InterpreterMacroAssembler::verify_method_data_pointer() { Unimplemented(); }


void InterpreterMacroAssembler::set_mdp_data_at(Register mdp_in,
                                                int constant,
                                                Register value) { Unimplemented(); }


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      int constant,
                                                      bool decrement) { Unimplemented(); }

void InterpreterMacroAssembler::increment_mdp_data_at(Address data,
                                                      bool decrement) { Unimplemented(); }


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      Register reg,
                                                      int constant,
                                                      bool decrement) { Unimplemented(); }

void InterpreterMacroAssembler::set_mdp_flag_at(Register mdp_in,
                                                int flag_byte_constant) { Unimplemented(); }



void InterpreterMacroAssembler::test_mdp_data_at(Register mdp_in,
                                                 int offset,
                                                 Register value,
                                                 Register test_value_out,
                                                 Label& not_equal_continue) { Unimplemented(); }


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     int offset_of_disp) { Unimplemented(); }


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     Register reg,
                                                     int offset_of_disp) { Unimplemented(); }


void InterpreterMacroAssembler::update_mdp_by_constant(Register mdp_in,
                                                       int constant) { Unimplemented(); }



void InterpreterMacroAssembler::update_mdp_for_ret(Register return_bci) { Unimplemented(); }


void InterpreterMacroAssembler::profile_taken_branch(Register mdp,
                                                     Register bumped_count) { Unimplemented(); }


void InterpreterMacroAssembler::profile_not_taken_branch(Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_call(Register mdp) { 
  if (ProfileInterpreter) {
    Unimplemented(); 
  }
}

void InterpreterMacroAssembler::profile_final_call(Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_virtual_call(Register receiver,
                                                     Register mdp,
                                                     Register reg2,
                                                     bool receiver_can_be_null) { Unimplemented(); }

// This routine creates a state machine for updating the multi-row
// type profile at a virtual call site (or other type-sensitive bytecode).
// The machine visits each row (of receiver/count) until the receiver type
// is found, or until it runs out of rows.  At the same time, it remembers
// the location of the first empty row.  (An empty row records null for its
// receiver, and can be allocated for a newly-observed receiver type.)
// Because there are two degrees of freedom in the state, a simple linear
// search will not work; it must be a decision tree.  Hence this helper
// function is recursive, to generate the required tree structured code.
// It's the interpreter, so we are trading off code space for speed.
// See below for example code.
void InterpreterMacroAssembler::record_klass_in_profile_helper(
                                        Register receiver, Register mdp,
                                        Register reg2, int start_row,
                                        Label& done, bool is_virtual_call) { Unimplemented(); }

// Example state machine code for three profile rows:
//   // main copy of decision tree, rooted at row[1]
//   if (row[0].rec == rec) { row[0].incr(); goto done; }
//   if (row[0].rec != NULL) {
//     // inner copy of decision tree, rooted at row[1]
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[1].rec != NULL) {
//       // degenerate decision tree, rooted at row[2]
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       if (row[2].rec != NULL) { count.incr(); goto done; } // overflow
//       row[2].init(rec); goto done;
//     } else {
//       // remember row[1] is empty
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       row[1].init(rec); goto done;
//     }
//   } else {
//     // remember row[0] is empty
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[2].rec == rec) { row[2].incr(); goto done; }
//     row[0].init(rec); goto done;
//   }
//   done:

void InterpreterMacroAssembler::record_klass_in_profile(Register receiver,
                                                        Register mdp, Register reg2,
                                                        bool is_virtual_call) { Unimplemented(); }

void InterpreterMacroAssembler::profile_ret(Register return_bci,
                                            Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_null_seen(Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_typecheck_failed(Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_typecheck(Register mdp, Register klass, Register reg2) { Unimplemented(); }


void InterpreterMacroAssembler::profile_switch_default(Register mdp) { Unimplemented(); }


void InterpreterMacroAssembler::profile_switch_case(Register index,
                                                    Register mdp,
                                                    Register reg2) { Unimplemented(); }



void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) {
  if (state == atos) {
    MacroAssembler::verify_oop(reg);
  }
}

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) { ; }
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() { 
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  if (JvmtiExport::can_post_interpreter_events()) {
    Label L;
    ldr(r3, Address(rthread, JavaThread::interp_only_mode_offset()));
    tst(r3, ~0);
    br(Assembler::EQ, L);
    call_VM(noreg, CAST_FROM_FN_PTR(address,
                                    InterpreterRuntime::post_method_entry));
    bind(L);
  }

  {
    SkipIfEqual skip(this, &DTraceMethodProbes, false);
    get_method(c_rarg1);
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry),
                 rthread, c_rarg1);
  }

  // RedefineClasses() tracing support for obsolete method entry
  if (RC_TRACE_IN_RANGE(0x00001000, 0x00002000)) {
    get_method(c_rarg1);
    call_VM_leaf(
      CAST_FROM_FN_PTR(address, SharedRuntime::rc_trace_method_entry),
      rthread, c_rarg1);
  }

 }


void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) {
  if (mode == NotifyJVMTI && JvmtiExport::can_post_interpreter_events()) {
    call_Unimplemented();
  }
}

// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, bool preloaded,
                                                        Condition cond, Label* where) { Unimplemented(); }

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) {
  // interpreter specific
  //
  // Note: No need to save/restore rbcp & rlocals pointer since these
  //       are callee saved registers and no blocking/ GC can happen
  //       in leaf calls.
#ifdef ASSERT
  {
    Label L;
    ldr(rscratch1, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    cbz(rscratch1, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments);
}

void InterpreterMacroAssembler::call_VM_base(Register oop_result,
                                             Register java_thread,
                                             Register last_java_sp,
                                             address  entry_point,
                                             int      number_of_arguments,
                                             bool     check_exceptions) {
  // interpreter specific
  //
  // Note: Could avoid restoring locals ptr (callee saved) - however doesn't
  //       really make a difference for these runtime calls, since they are
  //       slow anyway. Btw., bcp must be saved/restored since it may change
  //       due to GC.
  // assert(java_thread == noreg , "not expecting a precomputed java thread");
  save_bcp();
#ifdef ASSERT
  {
    Label L;
    ldr(rscratch1, Address(rfp, frame::interpreter_frame_last_sp_offset * wordSize));
    cbz(rscratch1, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_base(oop_result, noreg, last_java_sp,
                               entry_point, number_of_arguments,
                               check_exceptions);
  // interpreter specific
  restore_bcp();
  restore_locals();
}

