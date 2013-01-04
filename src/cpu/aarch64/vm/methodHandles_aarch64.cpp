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

<<<<<<< HEAD
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
                                                          int* frame_size_in_words) {
  __ call_Unimplemented();
}

void MethodHandles::RicochetFrame::enter_ricochet_frame(MacroAssembler* _masm,
                                                        Register rcx_recv,
                                                        Register rax_argv,
                                                        address return_handler,
                                                        Register rbx_temp) { __ call_Unimplemented(); }

void MethodHandles::RicochetFrame::leave_ricochet_frame(MacroAssembler* _masm,
                                                        Register rcx_recv,
                                                        Register new_sp_reg,
                                                        Register sender_pc_reg){ __ call_Unimplemented(); }

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
void MethodHandles::RicochetFrame::verify_clean(MacroAssembler* _masm) { __ call_Unimplemented(); }
#endif //ASSERT

void MethodHandles::load_klass_from_Class(MacroAssembler* _masm, Register klass_reg) { __ call_Unimplemented(); }
=======
void MethodHandles::load_klass_from_Class(MacroAssembler* _masm, Register klass_reg) {
  if (VerifyMethodHandles)
    verify_klass(_masm, klass_reg, SystemDictionary::WK_KLASS_ENUM_NAME(java_lang_Class),
                 "MH argument is a Class");
  __ ldr(klass_reg, Address(klass_reg, java_lang_Class::klass_offset_in_bytes()));
}
>>>>>>> 2b777357... Fixed recent 6 TCK failures for method handles

void MethodHandles::load_conversion_vminfo(MacroAssembler* _masm, Register reg, Address conversion_field_addr) { __ call_Unimplemented(); }

void MethodHandles::load_conversion_dest_type(MacroAssembler* _masm, Register reg, Address conversion_field_addr) { __ call_Unimplemented(); }

void MethodHandles::load_stack_move(MacroAssembler* _masm,
                                    Register rdi_stack_move,
                                    Register rcx_amh,
                                    bool might_be_negative) { __ call_Unimplemented(); }

#ifdef ASSERT
void MethodHandles::RicochetFrame::verify_offsets() {  }

void MethodHandles::RicochetFrame::verify() const {  }
#endif //PRODUCT

#ifdef ASSERT
<<<<<<< HEAD
void MethodHandles::verify_argslot(MacroAssembler* _masm,
                                   Register argslot_reg,
                                   const char* error_message) { __ call_Unimplemented(); }
=======
void MethodHandles::verify_klass(MacroAssembler* _masm,
                                 Register obj, SystemDictionary::WKID klass_id,
                                 const char* error_message) {
  Klass** klass_addr = SystemDictionary::well_known_klass_addr(klass_id);
  KlassHandle klass = SystemDictionary::well_known_klass(klass_id);
  Register temp = rscratch2;
  Register temp2 = rscratch1; // used by MacroAssembler::cmpptr
  Label L_ok, L_bad;
  BLOCK_COMMENT("verify_klass {");
  __ verify_oop(obj);
  __ cbz(obj, L_bad);
  __ push(temp);
  __ push(temp2);
  __ load_klass(temp, obj);
  __ cmpptr(temp, ExternalAddress((address) klass_addr));
  __ br(Assembler::EQ, L_ok);
  intptr_t super_check_offset = klass->super_check_offset();
  __ ldr(temp, Address(temp, super_check_offset));
  __ cmpptr(temp, ExternalAddress((address) klass_addr));
  __ br(Assembler::EQ, L_ok);
  __ pop(temp2);
  __ pop(temp); 
  __ bind(L_bad);
  __ stop(error_message);
  __ BIND(L_ok);
  __ pop(temp2);
  __ pop(temp); 
  BLOCK_COMMENT("} verify_klass");
}

void MethodHandles::verify_ref_kind(MacroAssembler* _masm, int ref_kind, Register member_reg, Register temp) {  }
>>>>>>> 4d5df24... First cut of method handles.

void MethodHandles::verify_argslots(MacroAssembler* _masm,
                                    RegisterOrConstant arg_slots,
                                    Register arg_slot_base_reg,
                                    bool negate_argslots,
                                    const char* error_message) { __ call_Unimplemented(); }

<<<<<<< HEAD
// Make sure that arg_slots has the same sign as the given direction.
// If (and only if) arg_slots is a assembly-time constant, also allow it to be zero.
void MethodHandles::verify_stack_move(MacroAssembler* _masm,
                                      RegisterOrConstant arg_slots, int direction) { __ call_Unimplemented(); }

void MethodHandles::verify_klass(MacroAssembler* _masm,
                                 Register obj, KlassHandle klass,
                                 const char* error_message) { __ call_Unimplemented(); }
#endif //ASSERT

void MethodHandles::jump_from_method_handle(MacroAssembler* _masm, Register method, Register temp) { __ call_Unimplemented(); }

// Code generation
address MethodHandles::generate_method_handle_interpreter_entry(MacroAssembler* _masm) { __ call_Unimplemented(); }

// Helper to insert argument slots into the stack.
// arg_slots must be a multiple of stack_move_unit() and < 0
// rax_argslot is decremented to point to the new (shifted) location of the argslot
// But, rdx_temp ends up holding the original value of rax_argslot.
void MethodHandles::insert_arg_slots(MacroAssembler* _masm,
                                     RegisterOrConstant arg_slots,
                                     Register rax_argslot,
                                     Register rbx_temp, Register rdx_temp) { __ call_Unimplemented(); }

// Helper to remove argument slots from the stack.
// arg_slots must be a multiple of stack_move_unit() and > 0
void MethodHandles::remove_arg_slots(MacroAssembler* _masm,
                                     RegisterOrConstant arg_slots,
                                     Register rax_argslot,
                                     Register rbx_temp, Register rdx_temp) { __ call_Unimplemented(); }

// Helper to copy argument slots to the top of the stack.
// The sequence starts with rax_argslot and is counted by slot_count
// slot_count must be a multiple of stack_move_unit() and >= 0
// This function blows the temps but does not change rax_argslot.
void MethodHandles::push_arg_slots(MacroAssembler* _masm,
                                   Register rax_argslot,
                                   RegisterOrConstant slot_count,
                                   int skip_words_count,
                                   Register rbx_temp, Register rdx_temp) { __ call_Unimplemented(); }

// in-place movement; no change to rsp
// blows rax_temp, rdx_temp
void MethodHandles::move_arg_slots_up(MacroAssembler* _masm,
                                      Register rbx_bottom,  // invariant
                                      Address  top_addr,     // can use rax_temp
                                      RegisterOrConstant positive_distance_in_slots,
                                      Register rax_temp, Register rdx_temp) { __ call_Unimplemented(); }

// in-place movement; no change to rsp
// blows rax_temp, rdx_temp
void MethodHandles::move_arg_slots_down(MacroAssembler* _masm,
                                        Address  bottom_addr,  // can use rax_temp
                                        Register rbx_top,      // invariant
                                        RegisterOrConstant negative_distance_in_slots,
                                        Register rax_temp, Register rdx_temp) { __ call_Unimplemented(); }

// Copy from a field or array element to a stacked argument slot.
// is_element (ignored) says whether caller is loading an array element instead of an instance field.
void MethodHandles::move_typed_arg(MacroAssembler* _masm,
                                   BasicType type, bool is_element,
                                   Address slot_dest, Address value_src,
                                   Register rbx_temp, Register rdx_temp) { __ call_Unimplemented(); }

void MethodHandles::move_return_value(MacroAssembler* _masm, BasicType type,
                                      Address return_slot) { __ call_Unimplemented(); }

#ifndef PRODUCT
#define DESCRIBE_RICOCHET_OFFSET(rf, name) \
  values.describe(frame_no, (intptr_t *) (((uintptr_t)rf) + MethodHandles::RicochetFrame::name##_offset_in_bytes()), #name)

void MethodHandles::RicochetFrame::describe(const frame* fr, FrameValues& values, int frame_no) {  }
#endif // ASSERT
=======
void MethodHandles::jump_from_method_handle(MacroAssembler* _masm, Register method, Register temp,
                                            bool for_compiler_entry) {
  assert(method == rmethod, "interpreter calling convention");
  __ verify_method_ptr(method);

  if (!for_compiler_entry && JvmtiExport::can_post_interpreter_events()) {
    Label run_compiled_code;
    // JVMTI events, such as single-stepping, are implemented partly by avoiding running
    // compiled code in threads for which the event is enabled.  Check here for
    // interp_only_mode if these events CAN be enabled.

    __ ldrb(rscratch1, Address(rthread, JavaThread::interp_only_mode_offset()));
    __ cbnz(rscratch1, run_compiled_code);
    __ b(Address(method, Method::interpreter_entry_offset()));
    __ BIND(run_compiled_code);
  }

  const ByteSize entry_offset = for_compiler_entry ? Method::from_compiled_offset() :
                                                     Method::from_interpreted_offset();
  __ ldr(rscratch1,Address(method, entry_offset));
  __ br(rscratch1);
}

void MethodHandles::jump_to_lambda_form(MacroAssembler* _masm,
                                        Register recv, Register method_temp,
                                        Register temp2,
                                        bool for_compiler_entry) {
  BLOCK_COMMENT("jump_to_lambda_form {");
  // This is the initial entry point of a lazy method handle.
  // After type checking, it picks up the invoker from the LambdaForm.
  assert_different_registers(recv, method_temp, temp2);
  assert(recv != noreg, "required register");
  assert(method_temp == rmethod, "required register for loading method");

  //NOT_PRODUCT({ FlagSetting fs(TraceMethodHandles, true); trace_method_handle(_masm, "LZMH"); });

  // Load the invoker, as MH -> MH.form -> LF.vmentry
  __ verify_oop(recv);
  __ load_heap_oop(method_temp, Address(recv, NONZERO(java_lang_invoke_MethodHandle::form_offset_in_bytes())));
  __ verify_oop(method_temp);
  __ load_heap_oop(method_temp, Address(method_temp, NONZERO(java_lang_invoke_LambdaForm::vmentry_offset_in_bytes())));
  __ verify_oop(method_temp);
  // the following assumes that a Method* is normally compressed in the vmtarget field:
  __ ldr(method_temp, Address(method_temp, NONZERO(java_lang_invoke_MemberName::vmtarget_offset_in_bytes())));

  if (VerifyMethodHandles && !for_compiler_entry) {
    // make sure recv is already on stack
    __ load_sized_value(temp2,
                        Address(method_temp, Method::size_of_parameters_offset()),
                        sizeof(u2), /*is_signed*/ false);
    // assert(sizeof(u2) == sizeof(Method::_size_of_parameters), "");
    Label L;
    __ ldr(rscratch1, __ argument_address(temp2, -1));
    __ cmp(recv, rscratch1);
    __ br(Assembler::EQ, L);
    __ ldr(r0, __ argument_address(temp2, -1));
    __ hlt(0);
    __ BIND(L);
  }

  jump_from_method_handle(_masm, method_temp, temp2, for_compiler_entry);
  BLOCK_COMMENT("} jump_to_lambda_form");
}

// Code generation
address MethodHandles::generate_method_handle_interpreter_entry(MacroAssembler* _masm,
                                                                vmIntrinsics::ID iid) {
  const bool not_for_compiler_entry = false;  // this is the interpreter entry
  assert(is_signature_polymorphic(iid), "expected invoke iid");
  if (iid == vmIntrinsics::_invokeGeneric ||
      iid == vmIntrinsics::_compiledLambdaForm) {
    // Perhaps surprisingly, the symbolic references visible to Java are not directly used.
    // They are linked to Java-generated adapters via MethodHandleNatives.linkMethod.
    // They all allow an appendix argument.
    __ hlt(0);           // empty stubs make SG sick
    return NULL;
  }

  // rsi/r13: sender SP (must preserve; see prepare_to_jump_from_interpreted)
  // rbx: Method*
  // rdx: argument locator (parameter slot count, added to rsp)
  // rcx: used as temp to hold mh or receiver
  // rax, rdi: garbage temps, blown away
  Register argp   = r3;   // argument list ptr, live on error paths
  Register temp   = r0;
  Register mh     = r1;   // MH receiver; dies quickly and is recycled

  address code_start = __ pc();

  // here's where control starts out:
  __ align(CodeEntryAlignment);
  address entry_point = __ pc();

  if (VerifyMethodHandles) {
    Label L;
    BLOCK_COMMENT("verify_intrinsic_id {");
    __ ldrb(rscratch1, Address(rmethod, Method::intrinsic_id_offset_in_bytes()));
    __ cmp(rscratch1, (int) iid);
    __ br(Assembler::EQ, L);
    if (iid == vmIntrinsics::_linkToVirtual ||
        iid == vmIntrinsics::_linkToSpecial) {
      // could do this for all kinds, but would explode assembly code size
      trace_method_handle(_masm, "bad Method*::intrinsic_id");
    }
    __ hlt(0);
    __ bind(L);
    BLOCK_COMMENT("} verify_intrinsic_id");
  }

  // First task:  Find out how big the argument list is.
  Address r3_first_arg_addr;
  int ref_kind = signature_polymorphic_intrinsic_ref_kind(iid);
  assert(ref_kind != 0 || iid == vmIntrinsics::_invokeBasic, "must be _invokeBasic or a linkTo intrinsic");
  if (ref_kind == 0 || MethodHandles::ref_kind_has_receiver(ref_kind)) {
    __ load_sized_value(argp,
                        Address(rmethod, Method::size_of_parameters_offset()),
                        sizeof(u2), /*is_signed*/ false);
    // assert(sizeof(u2) == sizeof(Method::_size_of_parameters), "");
    r3_first_arg_addr = __ argument_address(argp, -1);
  } else {
    DEBUG_ONLY(argp = noreg);
  }

  if (!is_signature_polymorphic_static(iid)) {
    __ ldr(mh, r3_first_arg_addr);
    DEBUG_ONLY(argp = noreg);
  }

  // rdx_first_arg_addr is live!

  if (TraceMethodHandles) {
    const char* name = vmIntrinsics::name_at(iid);
    if (*name == '_')  name += 1;
    const size_t len = strlen(name) + 50;
    char* qname = NEW_C_HEAP_ARRAY(char, len, mtInternal);
    const char* suffix = "";
    if (vmIntrinsics::method_for(iid) == NULL ||
        !vmIntrinsics::method_for(iid)->access_flags().is_public()) {
      if (is_signature_polymorphic_static(iid))
        suffix = "/static";
      else
        suffix = "/private";
    }
    jio_snprintf(qname, len, "MethodHandle::interpreter_entry::%s%s", name, suffix);
    // note: stub look for mh in rcx
    trace_method_handle(_masm, qname);
  }

  if (iid == vmIntrinsics::_invokeBasic) {
    generate_method_handle_dispatch(_masm, iid, mh, noreg, not_for_compiler_entry);

  } else {
    // Adjust argument list by popping the trailing MemberName argument.
    Register recv = noreg;
    if (MethodHandles::ref_kind_has_receiver(ref_kind)) {
      // Load the receiver (not the MH; the actual MemberName's receiver) up from the interpreter stack.
      __ ldr(recv = r2, r3_first_arg_addr);
    }
    DEBUG_ONLY(argp = noreg);
    Register rmember = rmethod;  // MemberName ptr; incoming method ptr is dead now
    __ pop(rmember);             // extract last argument
    generate_method_handle_dispatch(_masm, iid, recv, rmember, not_for_compiler_entry);
  }

  if (PrintMethodHandleStubs) {
    address code_end = __ pc();
    tty->print_cr("--------");
    tty->print_cr("method handle interpreter entry for %s", vmIntrinsics::name_at(iid));
    Disassembler::decode(code_start, code_end);
    tty->cr();
  }

  return entry_point;
}


void MethodHandles::generate_method_handle_dispatch(MacroAssembler* _masm,
                                                    vmIntrinsics::ID iid,
                                                    Register receiver_reg,
                                                    Register member_reg,
                                                    bool for_compiler_entry) {
  assert(is_signature_polymorphic(iid), "expected invoke iid");
  // temps used in this code are not used in *either* compiled or interpreted calling sequences
  Register temp1 = r4;
  Register temp2 = r5;
  Register temp3 = r3;
  if (for_compiler_entry) {
    assert(receiver_reg == (iid == vmIntrinsics::_linkToStatic ? noreg : j_rarg0), "only valid assignment");
    assert_different_registers(temp1,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
    assert_different_registers(temp2,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
    assert_different_registers(temp3,        j_rarg0, j_rarg1, j_rarg2, j_rarg3, j_rarg4, j_rarg5);
  }

  assert_different_registers(temp1, temp2, temp3, receiver_reg);
  assert_different_registers(temp1, temp2, temp3, member_reg);
  if (!for_compiler_entry)
    assert_different_registers(temp1, temp2, temp3, saved_last_sp_register());  // don't trash lastSP

  if (iid == vmIntrinsics::_invokeBasic) {
    // indirect through MH.form.vmentry.vmtarget
    jump_to_lambda_form(_masm, receiver_reg, rmethod, temp1, for_compiler_entry);

  } else {
    // The method is a member invoker used by direct method handles.
    if (VerifyMethodHandles) {
      // make sure the trailing argument really is a MemberName (caller responsibility)
      verify_klass(_masm, member_reg, SystemDictionary::WK_KLASS_ENUM_NAME(java_lang_invoke_MemberName),
                   "MemberName required for invokeVirtual etc.");
    }

    Address member_clazz(    member_reg, NONZERO(java_lang_invoke_MemberName::clazz_offset_in_bytes()));
    Address member_vmindex(  member_reg, NONZERO(java_lang_invoke_MemberName::vmindex_offset_in_bytes()));
    Address member_vmtarget( member_reg, NONZERO(java_lang_invoke_MemberName::vmtarget_offset_in_bytes()));

    Register temp1_recv_klass = temp1;
    if (iid != vmIntrinsics::_linkToStatic) {
      __ verify_oop(receiver_reg);
      if (iid == vmIntrinsics::_linkToSpecial) {
        // Don't actually load the klass; just null-check the receiver.
        __ null_check(receiver_reg);
      } else {
        // load receiver klass itself
        __ null_check(receiver_reg, oopDesc::klass_offset_in_bytes());
        __ load_klass(temp1_recv_klass, receiver_reg);
        __ verify_klass_ptr(temp1_recv_klass);
      }
      BLOCK_COMMENT("check_receiver {");
      // The receiver for the MemberName must be in receiver_reg.
      // Check the receiver against the MemberName.clazz
      if (VerifyMethodHandles && iid == vmIntrinsics::_linkToSpecial) {
        // Did not load it above...
        __ load_klass(temp1_recv_klass, receiver_reg);
        __ verify_klass_ptr(temp1_recv_klass);
      }
      if (VerifyMethodHandles && iid != vmIntrinsics::_linkToInterface) {
        Label L_ok;
        Register temp2_defc = temp2;
        __ load_heap_oop(temp2_defc, member_clazz);
        load_klass_from_Class(_masm, temp2_defc);
        __ verify_klass_ptr(temp2_defc);
        __ check_klass_subtype(temp1_recv_klass, temp2_defc, temp3, L_ok);
        // If we get here, the type check failed!
	__ hlt(0);
        // __ STOP("receiver class disagrees with MemberName.clazz");
        __ bind(L_ok);
      }
      BLOCK_COMMENT("} check_receiver");
    }
    if (iid == vmIntrinsics::_linkToSpecial ||
        iid == vmIntrinsics::_linkToStatic) {
      DEBUG_ONLY(temp1_recv_klass = noreg);  // these guys didn't load the recv_klass
    }

    // Live registers at this point:
    //  member_reg - MemberName that was the trailing argument
    //  temp1_recv_klass - klass of stacked receiver, if needed
    //  rsi/r13 - interpreter linkage (if interpreted)
    //  r2, rdx, rsi, rdi, r8, r8 - compiler arguments (if compiled)

    bool method_is_live = false;
    switch (iid) {
    case vmIntrinsics::_linkToSpecial:
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeSpecial, member_reg, temp3);
      }
      __ ldr(rmethod, member_vmtarget);
      method_is_live = true;
      break;

    case vmIntrinsics::_linkToStatic:
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeStatic, member_reg, temp3);
      }
      __ ldr(rmethod, member_vmtarget);
      method_is_live = true;
      break;

    case vmIntrinsics::_linkToVirtual:
    {
      // same as TemplateTable::invokevirtual,
      // minus the CP setup and profiling:

      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeVirtual, member_reg, temp3);
      }

      // pick out the vtable index from the MemberName, and then we can discard it:
      Register temp2_index = temp2;
      __ ldr(temp2_index, member_vmindex);

      if (VerifyMethodHandles) {
        Label L_index_ok;
        __ cmpw(temp2_index, 0U);
        __ br(Assembler::GE, L_index_ok);
        __ hlt(0);
        __ BIND(L_index_ok);
      }

      // Note:  The verifier invariants allow us to ignore MemberName.clazz and vmtarget
      // at this point.  And VerifyMethodHandles has already checked clazz, if needed.

      // get target Method* & entry point
      __ lookup_virtual_method(temp1_recv_klass, temp2_index, rmethod);
      method_is_live = true;
      break;
    }

    case vmIntrinsics::_linkToInterface:
    {
      // same as TemplateTable::invokeinterface
      // (minus the CP setup and profiling, with different argument motion)
      if (VerifyMethodHandles) {
        verify_ref_kind(_masm, JVM_REF_invokeInterface, member_reg, temp3);
      }

      Register temp3_intf = temp3;
      __ load_heap_oop(temp3_intf, member_clazz);
      load_klass_from_Class(_masm, temp3_intf);
      __ verify_klass_ptr(temp3_intf);

      Register rindex = rmethod;
      __ ldr(rindex, member_vmindex);
      if (VerifyMethodHandles) {
        Label L;
        __ cmpw(rindex, 0U);
        __ br(Assembler::GE, L);
        __ hlt(0);
        __ bind(L);
      }

      // given intf, index, and recv klass, dispatch to the implementation method
      Label L_no_such_interface;
      __ lookup_interface_method(temp1_recv_klass, temp3_intf,
                                 // note: next two args must be the same:
                                 rindex, rmethod,
                                 temp2,
                                 L_no_such_interface);

      __ verify_method_ptr(rmethod);
      jump_from_method_handle(_masm, rmethod, temp2, for_compiler_entry);
      __ hlt(0);

      __ bind(L_no_such_interface);
      __ b(RuntimeAddress(StubRoutines::throw_IncompatibleClassChangeError_entry()));
      break;
    }

    default:
      fatal(err_msg("unexpected intrinsic %d: %s", iid, vmIntrinsics::name_at(iid)));
      break;
    }

    if (method_is_live) {
      // live at this point:  rmethod, rsi/r13 (if interpreted)

      // After figuring out which concrete method to call, jump into it.
      // Note that this works in the interpreter with no data motion.
      // But the compiled version will require that r2_recv be shifted out.
      __ verify_method_ptr(rmethod);
      jump_from_method_handle(_masm, rmethod, temp1, for_compiler_entry);
    }
  }
}
>>>>>>> 4d5df24... First cut of method handles.

#ifndef PRODUCT
extern "C" void print_method_handle(oop mh);
void trace_method_handle_stub(const char* adaptername,
                              oop mh,
                              intptr_t* saved_regs,
                              intptr_t* entry_sp) {  }

// The stub wraps the arguments in a struct on the stack to avoid
// dealing with the different calling conventions for passing 6
// arguments.
struct MethodHandleStubArguments {
  const char* adaptername;
  oopDesc* mh;
  intptr_t* saved_regs;
  intptr_t* entry_sp;
};
void trace_method_handle_stub_wrapper(MethodHandleStubArguments* args) {  }

void MethodHandles::trace_method_handle(MacroAssembler* _masm, const char* adaptername) {  }
#endif //PRODUCT

// which conversion op types are implemented here?
int MethodHandles::adapter_conversion_ops_supported_mask() { return 0; }

//------------------------------------------------------------------------------
// MethodHandles::generate_method_handle_stub
//
// Generate an "entry" field for a method handle.
// This determines how the method handle will respond to calls.
void MethodHandles::generate_method_handle_stub(MacroAssembler* _masm, MethodHandles::EntryKind ek) { __ call_Unimplemented(); }
