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

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) { Unimplemented(); }

void InterpreterMacroAssembler::call_VM_base(Register oop_result,
                                             Register java_thread,
                                             Register last_java_sp,
                                             address  entry_point,
                                             int      number_of_arguments,
                                             bool     check_exceptions) { Unimplemented(); }


void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) { Unimplemented(); }


void InterpreterMacroAssembler::load_earlyret_value(TosState state) { Unimplemented(); }


void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) { Unimplemented(); }


void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(
  Register reg,
  int bcp_offset) { Unimplemented(); }


void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) { Unimplemented(); }


void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) { Unimplemented(); }


void InterpreterMacroAssembler::get_cache_and_index_and_bytecode_at_bcp(Register cache,
                                                                        Register index,
                                                                        Register bytecode,
                                                                        int byte_no,
                                                                        int bcp_offset,
                                                                        size_t index_size) { Unimplemented(); }


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

void InterpreterMacroAssembler::pop_ptr(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::pop_i(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::pop_l(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::push_ptr(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::push_i(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::push_l(Register r) { Unimplemented(); }

void InterpreterMacroAssembler::pop(TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::push(TosState state) { Unimplemented(); }


// Helpers for swap and dup
void InterpreterMacroAssembler::load_ptr(int n, Register val) { Unimplemented(); }

void InterpreterMacroAssembler::store_ptr(int n, Register val) { Unimplemented(); }


void InterpreterMacroAssembler::prepare_to_jump_from_interpreted() { Unimplemented(); }


// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) { Unimplemented(); }


// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts.  amd64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_base(TosState state,
                                              address* table,
                                              bool verifyoop) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_only(TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_only_normal(TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::dispatch_only_noverify(TosState state) { Unimplemented(); }


void InterpreterMacroAssembler::dispatch_next(TosState state, int step) { Unimplemented(); }

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
        Register ret_addr,
        bool throw_monitor_exception,
        bool install_monitor_exception,
        bool notify_jvmdi) { Unimplemented(); }

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
void InterpreterMacroAssembler::unlock_object(Register lock_reg) { Unimplemented(); }

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


void InterpreterMacroAssembler::profile_call(Register mdp) { Unimplemented(); }


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



void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) { Unimplemented(); }

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) { Unimplemented(); }
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() { Unimplemented(); }


void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) { Unimplemented(); }

// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, bool preloaded,
                                                        Condition cond, Label* where) { Unimplemented(); }
