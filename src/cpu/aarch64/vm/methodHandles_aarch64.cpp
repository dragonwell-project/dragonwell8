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

#include "precompiled.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "memory/allocation.inline.hpp"
#include "prims/methodHandles.hpp"

#define __ _masm->

#ifdef PRODUCT
#define BLOCK_COMMENT(str) /* nothing */
#else
#define BLOCK_COMMENT(str) __ block_comment(str)
#endif

#define BIND(label) bind(label); BLOCK_COMMENT(#label ":")

// Workaround for C++ overloading nastiness on '0' for RegisterOrConstant.
static RegisterOrConstant constant(int value) {
  return RegisterOrConstant(value);
}

address MethodHandleEntry::start_compiled_entry(MacroAssembler* _masm,
                                                address interpreted_entry) { Unimplemented(); return 0; }

MethodHandleEntry* MethodHandleEntry::finish_compiled_entry(MacroAssembler* _masm,
                                                address start_addr) { Unimplemented(); return 0; }

// stack walking support

frame MethodHandles::ricochet_frame_sender(const frame& fr, RegisterMap *map) { Unimplemented(); return fr; }

void MethodHandles::ricochet_frame_oops_do(const frame& fr, OopClosure* blk, const RegisterMap* reg_map) { Unimplemented(); }

oop MethodHandles::RicochetFrame::compute_saved_args_layout(bool read_cache, bool write_cache) { Unimplemented(); return 0; }

void MethodHandles::RicochetFrame::generate_ricochet_blob(MacroAssembler* _masm,
                                                          // output params:
                                                          int* bounce_offset,
                                                          int* exception_offset,
                                                          int* frame_size_in_words) { Unimplemented(); }

void MethodHandles::RicochetFrame::enter_ricochet_frame(MacroAssembler* _masm,
                                                        Register rcx_recv,
                                                        Register rax_argv,
                                                        address return_handler,
                                                        Register rbx_temp) { Unimplemented(); }

void MethodHandles::RicochetFrame::leave_ricochet_frame(MacroAssembler* _masm,
                                                        Register rcx_recv,
                                                        Register new_sp_reg,
                                                        Register sender_pc_reg){ Unimplemented(); }

// Emit code to verify that RBP is pointing at a valid ricochet frame.
#ifndef PRODUCT
enum {
  ARG_LIMIT = 255, SLOP = 4,
  // use this parameter for checking for garbage stack movements:
  UNREASONABLE_STACK_MOVE = (ARG_LIMIT + SLOP)
  // the slop defends against false alarms due to fencepost errors
};
#endif

#ifdef ASSERT
void MethodHandles::RicochetFrame::verify_clean(MacroAssembler* _masm) { Unimplemented(); }
#endif //ASSERT

void MethodHandles::load_klass_from_Class(MacroAssembler* _masm, Register klass_reg) { Unimplemented(); }

void MethodHandles::load_conversion_vminfo(MacroAssembler* _masm, Register reg, Address conversion_field_addr) { Unimplemented(); }

void MethodHandles::load_conversion_dest_type(MacroAssembler* _masm, Register reg, Address conversion_field_addr) { Unimplemented(); }

void MethodHandles::load_stack_move(MacroAssembler* _masm,
                                    Register rdi_stack_move,
                                    Register rcx_amh,
                                    bool might_be_negative) { Unimplemented(); }

#ifdef ASSERT
void MethodHandles::RicochetFrame::verify_offsets() { Unimplemented(); }

void MethodHandles::RicochetFrame::verify() const { Unimplemented(); }
#endif //PRODUCT

#ifdef ASSERT
void MethodHandles::verify_argslot(MacroAssembler* _masm,
                                   Register argslot_reg,
                                   const char* error_message) { Unimplemented(); }

void MethodHandles::verify_argslots(MacroAssembler* _masm,
                                    RegisterOrConstant arg_slots,
                                    Register arg_slot_base_reg,
                                    bool negate_argslots,
                                    const char* error_message) { Unimplemented(); }

// Make sure that arg_slots has the same sign as the given direction.
// If (and only if) arg_slots is a assembly-time constant, also allow it to be zero.
void MethodHandles::verify_stack_move(MacroAssembler* _masm,
                                      RegisterOrConstant arg_slots, int direction) { Unimplemented(); }

void MethodHandles::verify_klass(MacroAssembler* _masm,
                                 Register obj, KlassHandle klass,
                                 const char* error_message) { Unimplemented(); }
#endif //ASSERT

void MethodHandles::jump_from_method_handle(MacroAssembler* _masm, Register method, Register temp) { Unimplemented(); }

// Code generation
address MethodHandles::generate_method_handle_interpreter_entry(MacroAssembler* _masm) { Unimplemented(); }

// Helper to insert argument slots into the stack.
// arg_slots must be a multiple of stack_move_unit() and < 0
// rax_argslot is decremented to point to the new (shifted) location of the argslot
// But, rdx_temp ends up holding the original value of rax_argslot.
void MethodHandles::insert_arg_slots(MacroAssembler* _masm,
                                     RegisterOrConstant arg_slots,
                                     Register rax_argslot,
                                     Register rbx_temp, Register rdx_temp) { Unimplemented(); }

// Helper to remove argument slots from the stack.
// arg_slots must be a multiple of stack_move_unit() and > 0
void MethodHandles::remove_arg_slots(MacroAssembler* _masm,
                                     RegisterOrConstant arg_slots,
                                     Register rax_argslot,
                                     Register rbx_temp, Register rdx_temp) { Unimplemented(); }

// Helper to copy argument slots to the top of the stack.
// The sequence starts with rax_argslot and is counted by slot_count
// slot_count must be a multiple of stack_move_unit() and >= 0
// This function blows the temps but does not change rax_argslot.
void MethodHandles::push_arg_slots(MacroAssembler* _masm,
                                   Register rax_argslot,
                                   RegisterOrConstant slot_count,
                                   int skip_words_count,
                                   Register rbx_temp, Register rdx_temp) { Unimplemented(); }

// in-place movement; no change to rsp
// blows rax_temp, rdx_temp
void MethodHandles::move_arg_slots_up(MacroAssembler* _masm,
                                      Register rbx_bottom,  // invariant
                                      Address  top_addr,     // can use rax_temp
                                      RegisterOrConstant positive_distance_in_slots,
                                      Register rax_temp, Register rdx_temp) { Unimplemented(); }

// in-place movement; no change to rsp
// blows rax_temp, rdx_temp
void MethodHandles::move_arg_slots_down(MacroAssembler* _masm,
                                        Address  bottom_addr,  // can use rax_temp
                                        Register rbx_top,      // invariant
                                        RegisterOrConstant negative_distance_in_slots,
                                        Register rax_temp, Register rdx_temp) { Unimplemented(); }

// Copy from a field or array element to a stacked argument slot.
// is_element (ignored) says whether caller is loading an array element instead of an instance field.
void MethodHandles::move_typed_arg(MacroAssembler* _masm,
                                   BasicType type, bool is_element,
                                   Address slot_dest, Address value_src,
                                   Register rbx_temp, Register rdx_temp) { Unimplemented(); }

void MethodHandles::move_return_value(MacroAssembler* _masm, BasicType type,
                                      Address return_slot) { Unimplemented(); }

#ifndef PRODUCT
#define DESCRIBE_RICOCHET_OFFSET(rf, name) \
  values.describe(frame_no, (intptr_t *) (((uintptr_t)rf) + MethodHandles::RicochetFrame::name##_offset_in_bytes()), #name)

void MethodHandles::RicochetFrame::describe(const frame* fr, FrameValues& values, int frame_no) { Unimplemented(); }
#endif // ASSERT

#ifndef PRODUCT
extern "C" void print_method_handle(oop mh);
void trace_method_handle_stub(const char* adaptername,
                              oop mh,
                              intptr_t* saved_regs,
                              intptr_t* entry_sp) { Unimplemented(); }

// The stub wraps the arguments in a struct on the stack to avoid
// dealing with the different calling conventions for passing 6
// arguments.
struct MethodHandleStubArguments {
  const char* adaptername;
  oopDesc* mh;
  intptr_t* saved_regs;
  intptr_t* entry_sp;
};
void trace_method_handle_stub_wrapper(MethodHandleStubArguments* args) { Unimplemented(); }

void MethodHandles::trace_method_handle(MacroAssembler* _masm, const char* adaptername) { Unimplemented(); }
#endif //PRODUCT

// which conversion op types are implemented here?
int MethodHandles::adapter_conversion_ops_supported_mask() { Unimplemented(); }

//------------------------------------------------------------------------------
// MethodHandles::generate_method_handle_stub
//
// Generate an "entry" field for a method handle.
// This determines how the method handle will respond to calls.
void MethodHandles::generate_method_handle_stub(MacroAssembler* _masm, MethodHandles::EntryKind ek) { Unimplemented(); }
