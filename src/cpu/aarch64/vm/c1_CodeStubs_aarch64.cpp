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
#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "nativeInst_aarch64.hpp"
#include "runtime/sharedRuntime.hpp"
#include "vmreg_aarch64.inline.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif


#define __ ce->masm()->

float ConversionStub::float_zero = 0.0;
double ConversionStub::double_zero = 0.0;

static Register as_reg(LIR_Opr op) {
  return op->is_double_cpu() ? op->as_register_lo() : op->as_register();
}

void ConversionStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);

  // FIXME: Agh, this is so painful

  __ enter();
  __ sub(sp, sp, 2 * wordSize);
  __ push(0x3fffffff, sp);         // integer registers except lr & sp
  for (int i = 30; i >= 0; i -= 2) // caller-saved fp registers
    if (i < 8 || i > 15)
      __ stpd(as_FloatRegister(i), as_FloatRegister(i+1),
	      Address(__ pre(sp, -2 * wordSize)));

  switch(bytecode()) {
  case Bytecodes::_f2i:
    {
      if (v0 != input()->as_float_reg())
	__ fmovs(v0, input()->as_float_reg());
      __ call_VM_leaf_base1(CAST_FROM_FN_PTR(address, SharedRuntime::f2i),
			    0, 1, MacroAssembler::ret_type_integral);
    }
    break;
  case Bytecodes::_d2i:
    {
      if (v0 != input()->as_double_reg())
	__ fmovd(v0, input()->as_double_reg());
      __ call_VM_leaf_base1(CAST_FROM_FN_PTR(address, SharedRuntime::d2i),
			    0, 1, MacroAssembler::ret_type_integral);
    }
    break;
  case Bytecodes::_f2l:
    {
      if (v0 != input()->as_float_reg())
	__ fmovs(v0, input()->as_float_reg());
      __ call_VM_leaf_base1(CAST_FROM_FN_PTR(address, SharedRuntime::f2l),
			    0, 1, MacroAssembler::ret_type_integral);
    }
    break;
  case Bytecodes::_d2l:
    {
      if (v0 != input()->as_double_reg())
	__ fmovd(v0, input()->as_double_reg());
      __ call_VM_leaf_base1(CAST_FROM_FN_PTR(address, SharedRuntime::d2l),
			    0, 1, MacroAssembler::ret_type_integral);
    }
    break;
  default:
    ShouldNotReachHere();
  }

  __ str(r0, Address(rfp, -wordSize));

  for (int i = 0; i < 32; i += 2)
    if (i < 8 || i > 15)
      __ ldpd(as_FloatRegister(i), as_FloatRegister(i+1),
	      Address(__ post(sp, 2 * wordSize)));
  __ pop(0x3fffffff, sp);

  __ ldr(as_reg(result()), Address(rfp, -wordSize));
  __ leave();

  __ b(_continuation);
}

void CounterOverflowStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);
  ce->store_parameter(_method->as_register(), 1);
  ce->store_parameter(_bci, 0);
  __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::counter_overflow_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  __ b(_continuation);
}

RangeCheckStub::RangeCheckStub(CodeEmitInfo* info, LIR_Opr index,
                               bool throw_index_out_of_bounds_exception)
  : _throw_index_out_of_bounds_exception(throw_index_out_of_bounds_exception)
  , _index(index)
{
  assert(info != NULL, "must have info");
  _info = new CodeEmitInfo(info);
}

void RangeCheckStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);
  if (_info->deoptimize_on_exception()) {
    address a = Runtime1::entry_for(Runtime1::predicate_failed_trap_id);
    __ call(RuntimeAddress(a));
    ce->add_call_info_here(_info);
    ce->verify_oop_map(_info);
    debug_only(__ should_not_reach_here());
    return;
  }

  if (_index->is_cpu_register()) {
    __ mov(rscratch1, _index->as_register());
  } else {
    __ mov(rscratch1, _index->as_jint());
  }
  Runtime1::StubID stub_id;
  if (_throw_index_out_of_bounds_exception) {
    stub_id = Runtime1::throw_index_exception_id;
  } else {
    stub_id = Runtime1::throw_range_check_failed_id;
  }
  __ call(RuntimeAddress(Runtime1::entry_for(stub_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  debug_only(__ should_not_reach_here());
}

PredicateFailedStub::PredicateFailedStub(CodeEmitInfo* info) {
  _info = new CodeEmitInfo(info);
}

void PredicateFailedStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);
  address a = Runtime1::entry_for(Runtime1::predicate_failed_trap_id);
  __ call(RuntimeAddress(a));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  debug_only(__ should_not_reach_here());
}

void DivByZeroStub::emit_code(LIR_Assembler* ce) {
  if (_offset != -1) {
    ce->compilation()->implicit_exception_table()->append(_offset, __ offset());
  }
  __ bind(_entry);
  __ bl(Address(Runtime1::entry_for(Runtime1::throw_div0_exception_id), relocInfo::runtime_call_type));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
#ifdef ASSERT
  __ should_not_reach_here();
#endif
}



// Implementation of NewInstanceStub

NewInstanceStub::NewInstanceStub(LIR_Opr klass_reg, LIR_Opr result, ciInstanceKlass* klass, CodeEmitInfo* info, Runtime1::StubID stub_id) {
  _result = result;
  _klass = klass;
  _klass_reg = klass_reg;
  _info = new CodeEmitInfo(info);
  assert(stub_id == Runtime1::new_instance_id                 ||
         stub_id == Runtime1::fast_new_instance_id            ||
         stub_id == Runtime1::fast_new_instance_init_check_id,
         "need new_instance id");
  _stub_id   = stub_id;
}



void NewInstanceStub::emit_code(LIR_Assembler* ce) {
  assert(__ rsp_offset() == 0, "frame size should be fixed");
  __ bind(_entry);
  __ mov(r3, _klass_reg->as_register());
  __ bl(RuntimeAddress(Runtime1::entry_for(_stub_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  assert(_result->as_register() == r0, "result must in rax,");
  __ b(_continuation);
}


// Implementation of NewTypeArrayStub

// Implementation of NewTypeArrayStub

NewTypeArrayStub::NewTypeArrayStub(LIR_Opr klass_reg, LIR_Opr length, LIR_Opr result, CodeEmitInfo* info) {
  _klass_reg = klass_reg;
  _length = length;
  _result = result;
  _info = new CodeEmitInfo(info);
}


void NewTypeArrayStub::emit_code(LIR_Assembler* ce) {
  assert(__ rsp_offset() == 0, "frame size should be fixed");
  __ bind(_entry);
  assert(_length->as_register() == r19, "length must in r19,");
  assert(_klass_reg->as_register() == r3, "klass_reg must in r3");
  __ bl(RuntimeAddress(Runtime1::entry_for(Runtime1::new_type_array_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  assert(_result->as_register() == r0, "result must in r0");
  __ b(_continuation);
}


// Implementation of NewObjectArrayStub

NewObjectArrayStub::NewObjectArrayStub(LIR_Opr klass_reg, LIR_Opr length, LIR_Opr result, CodeEmitInfo* info) {
  _klass_reg = klass_reg;
  _result = result;
  _length = length;
  _info = new CodeEmitInfo(info);
}


void NewObjectArrayStub::emit_code(LIR_Assembler* ce) {
  assert(__ rsp_offset() == 0, "frame size should be fixed");
  __ bind(_entry);
  assert(_length->as_register() == r19, "length must in r19,");
  assert(_klass_reg->as_register() == r3, "klass_reg must in r3");
  __ bl(RuntimeAddress(Runtime1::entry_for(Runtime1::new_object_array_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  assert(_result->as_register() == r0, "result must in r0");
  __ b(_continuation);
}
// Implementation of MonitorAccessStubs

MonitorEnterStub::MonitorEnterStub(LIR_Opr obj_reg, LIR_Opr lock_reg, CodeEmitInfo* info)
: MonitorAccessStub(obj_reg, lock_reg)
{
  _info = new CodeEmitInfo(info);
}


void MonitorEnterStub::emit_code(LIR_Assembler* ce) {
  assert(__ rsp_offset() == 0, "frame size should be fixed");
  __ bind(_entry);
  ce->store_parameter(_obj_reg->as_register(),  1);
  ce->store_parameter(_lock_reg->as_register(), 0);
  Runtime1::StubID enter_id;
  if (ce->compilation()->has_fpu_code()) {
    enter_id = Runtime1::monitorenter_id;
  } else {
    enter_id = Runtime1::monitorenter_nofpu_id;
  }
  __ bl(RuntimeAddress(Runtime1::entry_for(enter_id)));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  __ b(_continuation);
}


void MonitorExitStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);
  if (_compute_lock) {
    // lock_reg was destroyed by fast unlocking attempt => recompute it
    ce->monitor_address(_monitor_ix, _lock_reg);
  }
  ce->store_parameter(_lock_reg->as_register(), 0);
  // note: non-blocking leaf routine => no call info needed
  Runtime1::StubID exit_id;
  if (ce->compilation()->has_fpu_code()) {
    exit_id = Runtime1::monitorexit_id;
  } else {
    exit_id = Runtime1::monitorexit_nofpu_id;
  }
  __ adr(lr, _continuation);
  __ b(RuntimeAddress(Runtime1::entry_for(exit_id)));
}


// Implementation of patching:
// - Copy the code at given offset to an inlined buffer (first the bytes, then the number of bytes)
// - Replace original code with a call to the stub
// At Runtime:
// - call to stub, jump to runtime
// - in runtime: preserve all registers (rspecially objects, i.e., source and destination object)
// - in runtime: after initializing class, restore original code, reexecute instruction

int PatchingStub::_patch_info_offset = -NativeGeneralJump::instruction_size;

void PatchingStub::align_patch_site(MacroAssembler* masm) {
}

void PatchingStub::emit_code(LIR_Assembler* ce) {
  assert(NativeCall::instruction_size <= _bytes_to_copy && _bytes_to_copy <= 0xFF, "not enough room for call");

  Label call_patch;

  // static field accesses have special semantics while the class
  // initializer is being run so we emit a test which can be used to
  // check that this code is being executed by the initializing
  // thread.
  address being_initialized_entry = __ pc();
  if (CommentedAssembly) {
    __ block_comment(" patch template");
  }

  // make a copy the code which is going to be patched.
  for (int i = 0; i < _bytes_to_copy; i++) {
    address ptr = (address)(_pc_start + i);
    int a_byte = (*ptr) & 0xFF;
    __ emit_int8(a_byte);
  }

  address end_of_patch = __ pc();
  int bytes_to_skip = 0;
  if (_id == load_mirror_id) {
    int offset = __ offset();
    if (CommentedAssembly) {
      __ block_comment(" being_initialized check");
    }
    assert(_obj != noreg, "must be a valid register");
    Register tmp = r0;
    Register tmp2 = r19;
    __ stp(tmp, tmp2, Address(__ pre(sp, -2 * wordSize)));
    // Load without verification to keep code size small. We need it because
    // begin_initialized_entry_offset has to fit in a byte. Also, we know it's not null.
    __ ldr(tmp2, Address(_obj, java_lang_Class::klass_offset_in_bytes()));
    __ ldr(tmp, Address(tmp2, InstanceKlass::init_thread_offset()));
    __ cmp(rthread, tmp);
    __ ldp(tmp, tmp2, Address(__ post(sp, 2 * wordSize)));
    __ br(Assembler::NE, call_patch);

    // access_field patches may execute the patched code before it's
    // copied back into place so we need to jump back into the main
    // code of the nmethod to continue execution.
    __ b(_patch_site_continuation);

    // make sure this extra code gets skipped
    bytes_to_skip += __ offset() - offset;
  }
  if (CommentedAssembly) {
    __ block_comment("patch data");
  }
  // Now emit the patch record telling the runtime how to find the
  // pieces of the patch.
  int sizeof_patch_record = 8;
  bytes_to_skip += sizeof_patch_record;

  // emit the offsets needed to find the code to patch
  int being_initialized_entry_offset = __ pc() - being_initialized_entry + sizeof_patch_record;

  // If this is a field access, the offset is held in the constant
  // pool rather than embedded in the instruction, so we don't copy
  // any instructions: we set the value in the constant pool and
  // overwrite the NativeGeneralJump.
  {
    Label L;
    __ br(Assembler::AL, L);
    __ emit_int8(0);
    __ emit_int8(being_initialized_entry_offset);
    if (_id == access_field_id) {
      __ emit_int8(bytes_to_skip + _bytes_to_copy);
      __ emit_int8(0);
    } else {
      __ emit_int8(bytes_to_skip);
      __ emit_int8(_bytes_to_copy);
    }
    __ bind(L);
  }

  address patch_info_pc = __ pc();
  assert(patch_info_pc - end_of_patch == bytes_to_skip, "incorrect patch info");

  address entry = __ pc();
  NativeGeneralJump::insert_unconditional((address)_pc_start, entry);
  address target = NULL;
  relocInfo::relocType reloc_type = relocInfo::none;

  switch (_id) {
  case access_field_id:
    target = Runtime1::entry_for(Runtime1::access_field_patching_id);
    reloc_type = relocInfo::section_word_type;
    break;
  case load_klass_id:
    target = Runtime1::entry_for(Runtime1::load_klass_patching_id);
    reloc_type = relocInfo::metadata_type;
    break;
  case load_mirror_id:
    target = Runtime1::entry_for(Runtime1::load_mirror_patching_id);
    reloc_type = relocInfo::oop_type;
    break;
  default: ShouldNotReachHere();
  }

  __ bind(call_patch);

  if (CommentedAssembly) {
    __ block_comment("patch entry point");
  }
  __ bl(RuntimeAddress(target));
  assert(_patch_info_offset == (patch_info_pc - __ pc()), "must not change");
  ce->add_call_info_here(_info);
  int jmp_off = __ offset();
  __ b(_patch_site_entry);
  // Add enough nops so deoptimization can overwrite the jmp above with a call
  // and not destroy the world.
  // FIXME: AArch64 doesn't really need this
  // __ nop(); __ nop();
  // if (_id == load_klass_id 
  //     || _id == load_mirror_id
  //     || _id == access_field_id
  //     ) {
  //   CodeSection* cs = __ code_section();
  //   RelocIterator iter(cs, (address)_pc_start, (address)(_pc_start + 1));
  //   relocInfo::change_reloc_info_for_address(&iter, (address) _pc_start, reloc_type, relocInfo::none);
  // }
}


void DeoptimizeStub::emit_code(LIR_Assembler* ce) {
  __ bind(_entry);
  __ call(RuntimeAddress(Runtime1::entry_for(Runtime1::deoptimize_id)));
  ce->add_call_info_here(_info);
  DEBUG_ONLY(__ should_not_reach_here());
}


void ImplicitNullCheckStub::emit_code(LIR_Assembler* ce) {
  address a;
  if (_info->deoptimize_on_exception()) {
    // Deoptimize, do not throw the exception, because it is probably wrong to do it here.
    a = Runtime1::entry_for(Runtime1::predicate_failed_trap_id);
  } else {
    a = Runtime1::entry_for(Runtime1::throw_null_pointer_exception_id);
  }

  ce->compilation()->implicit_exception_table()->append(_offset, __ offset());
  __ bind(_entry);
  __ call(RuntimeAddress(a));
  ce->add_call_info_here(_info);
  ce->verify_oop_map(_info);
  debug_only(__ should_not_reach_here());
}


void SimpleExceptionStub::emit_code(LIR_Assembler* ce) {
  assert(__ rsp_offset() == 0, "frame size should be fixed");

  __ bind(_entry);
  // pass the object in a scratch register because all other registers
  // must be preserved
  if (_obj->is_cpu_register()) {
    __ mov(rscratch1, _obj->as_register());
  }
  __ call(RuntimeAddress(Runtime1::entry_for(_stub)));
  ce->add_call_info_here(_info);
  debug_only(__ should_not_reach_here());
}


void ArrayCopyStub::emit_code(LIR_Assembler* ce) {
  //---------------slow case: call to native-----------------
  __ bind(_entry);
  // Figure out where the args should go
  // This should really convert the IntrinsicID to the Method* and signature
  // but I don't know how to do that.
  //
  VMRegPair args[5];
  BasicType signature[5] = { T_OBJECT, T_INT, T_OBJECT, T_INT, T_INT};
  SharedRuntime::java_calling_convention(signature, args, 5, true);

  // push parameters
  // (src, src_pos, dest, destPos, length)
  Register r[5];
  r[0] = src()->as_register();
  r[1] = src_pos()->as_register();
  r[2] = dst()->as_register();
  r[3] = dst_pos()->as_register();
  r[4] = length()->as_register();

  // next registers will get stored on the stack
  for (int i = 0; i < 5 ; i++ ) {
    VMReg r_1 = args[i].first();
    if (r_1->is_stack()) {
      int st_off = r_1->reg2stack() * wordSize;
      __ str (r[i], Address(sp, st_off));
    } else {
      assert(r[i] == args[i].first()->as_Register(), "Wrong register for arg ");
    }
  }

  ce->align_call(lir_static_call);

  ce->emit_static_call_stub();
  Address resolve(SharedRuntime::get_resolve_static_call_stub(),
		  relocInfo::static_call_type);
  __ bl(resolve);
  ce->add_call_info_here(info());

#ifndef PRODUCT
  __ lea(rscratch2, ExternalAddress((address)&Runtime1::_arraycopy_slowcase_cnt));
  __ incrementw(Address(rscratch2));
#endif

  __ b(_continuation);
}


/////////////////////////////////////////////////////////////////////////////
#if INCLUDE_ALL_GCS

void G1PreBarrierStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

jbyte* G1PostBarrierStub::_byte_map_base = NULL;

jbyte* G1PostBarrierStub::byte_map_base_slow() { Unimplemented(); return 0; }


void G1PostBarrierStub::emit_code(LIR_Assembler* ce) { Unimplemented(); }

#endif // INCLUDE_ALL_GCS
/////////////////////////////////////////////////////////////////////////////

#undef __
