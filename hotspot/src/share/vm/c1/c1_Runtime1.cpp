/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/codeBuffer.hpp"
#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_Defs.hpp"
#include "c1/c1_FrameMap.hpp"
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include "c1/c1_Runtime1.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/codeBlob.hpp"
#include "code/compiledIC.hpp"
#include "code/pcDesc.hpp"
#include "code/scopeDesc.hpp"
#include "code/vtableStubs.hpp"
#include "compiler/disassembler.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/bytecode.hpp"
#include "interpreter/interpreter.hpp"
#include "jfr/support/jfrIntrinsics.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/barrierSet.hpp"
#include "memory/oopFactory.hpp"
#include "memory/resourceArea.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/compilationPolicy.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/threadCritical.hpp"
#include "runtime/vframe.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/copy.hpp"
#include "utilities/events.hpp"
#include "utilities/exceptions.hpp"


// Implementation of StubAssembler

StubAssembler::StubAssembler(CodeBuffer* code, const char * name, int stub_id) : C1_MacroAssembler(code) {
  _name = name;
  _must_gc_arguments = false;
  _frame_size = no_frame_size;
  _num_rt_args = 0;
  _stub_id = stub_id;
}


void StubAssembler::set_info(const char* name, bool must_gc_arguments) {
  _name = name;
  _must_gc_arguments = must_gc_arguments;
}


void StubAssembler::set_frame_size(int size) {
  if (_frame_size == no_frame_size) {
    _frame_size = size;
  }
  assert(_frame_size == size, "can't change the frame size");
}


void StubAssembler::set_num_rt_args(int args) {
  if (_num_rt_args == 0) {
    _num_rt_args = args;
  }
  assert(_num_rt_args == args, "can't change the number of args");
}

// Implementation of Runtime1

CodeBlob* Runtime1::_blobs[Runtime1::number_of_ids];
const char *Runtime1::_blob_names[] = {
  RUNTIME1_STUBS(STUB_NAME, LAST_STUB_NAME)
};

#ifndef PRODUCT
// statistics
int Runtime1::_generic_arraycopy_cnt = 0;
int Runtime1::_primitive_arraycopy_cnt = 0;
int Runtime1::_oop_arraycopy_cnt = 0;
int Runtime1::_generic_arraycopystub_cnt = 0;
int Runtime1::_arraycopy_slowcase_cnt = 0;
int Runtime1::_arraycopy_checkcast_cnt = 0;
int Runtime1::_arraycopy_checkcast_attempt_cnt = 0;
int Runtime1::_new_type_array_slowcase_cnt = 0;
int Runtime1::_new_object_array_slowcase_cnt = 0;
int Runtime1::_new_instance_slowcase_cnt = 0;
int Runtime1::_new_multi_array_slowcase_cnt = 0;
int Runtime1::_monitorenter_slowcase_cnt = 0;
int Runtime1::_monitorexit_slowcase_cnt = 0;
int Runtime1::_patch_code_slowcase_cnt = 0;
int Runtime1::_throw_range_check_exception_count = 0;
int Runtime1::_throw_index_exception_count = 0;
int Runtime1::_throw_div0_exception_count = 0;
int Runtime1::_throw_null_pointer_exception_count = 0;
int Runtime1::_throw_class_cast_exception_count = 0;
int Runtime1::_throw_incompatible_class_change_error_count = 0;
int Runtime1::_throw_array_store_exception_count = 0;
int Runtime1::_throw_count = 0;

static int _byte_arraycopy_cnt = 0;
static int _short_arraycopy_cnt = 0;
static int _int_arraycopy_cnt = 0;
static int _long_arraycopy_cnt = 0;
static int _oop_arraycopy_cnt = 0;

address Runtime1::arraycopy_count_address(BasicType type) {
  switch (type) {
  case T_BOOLEAN:
  case T_BYTE:   return (address)&_byte_arraycopy_cnt;
  case T_CHAR:
  case T_SHORT:  return (address)&_short_arraycopy_cnt;
  case T_FLOAT:
  case T_INT:    return (address)&_int_arraycopy_cnt;
  case T_DOUBLE:
  case T_LONG:   return (address)&_long_arraycopy_cnt;
  case T_ARRAY:
  case T_OBJECT: return (address)&_oop_arraycopy_cnt;
  default:
    ShouldNotReachHere();
    return NULL;
  }
}


#endif

// Simple helper to see if the caller of a runtime stub which
// entered the VM has been deoptimized

static bool caller_is_deopted() {
  JavaThread* thread = JavaThread::current();
  RegisterMap reg_map(thread, false);
  frame runtime_frame = thread->last_frame();
  frame caller_frame = runtime_frame.sender(&reg_map);
  assert(caller_frame.is_compiled_frame(), "must be compiled");
  return caller_frame.is_deoptimized_frame();
}

// Stress deoptimization
static void deopt_caller() {
  if ( !caller_is_deopted()) {
    JavaThread* thread = JavaThread::current();
    RegisterMap reg_map(thread, false);
    frame runtime_frame = thread->last_frame();
    frame caller_frame = runtime_frame.sender(&reg_map);
    Deoptimization::deoptimize_frame(thread, caller_frame.id());
    assert(caller_is_deopted(), "Must be deoptimized");
  }
}


void Runtime1::generate_blob_for(BufferBlob* buffer_blob, StubID id) {
  assert(0 <= id && id < number_of_ids, "illegal stub id");
  ResourceMark rm;
  // create code buffer for code storage
  CodeBuffer code(buffer_blob);

  Compilation::setup_code_buffer(&code, 0);

  // create assembler for code generation
  StubAssembler* sasm = new StubAssembler(&code, name_for(id), id);
  // generate code for runtime stub
  OopMapSet* oop_maps;
  oop_maps = generate_code_for(id, sasm);
  assert(oop_maps == NULL || sasm->frame_size() != no_frame_size,
         "if stub has an oop map it must have a valid frame size");

#ifdef ASSERT
  // Make sure that stubs that need oopmaps have them
  switch (id) {
    // These stubs don't need to have an oopmap
    case dtrace_object_alloc_id:
    case g1_pre_barrier_slow_id:
    case g1_post_barrier_slow_id:
    case slow_subtype_check_id:
    case fpu2long_stub_id:
    case unwind_exception_id:
    case counter_overflow_id:
#if defined(SPARC) || defined(PPC)
    case handle_exception_nofpu_id:  // Unused on sparc
#endif
      break;

    // All other stubs should have oopmaps
    default:
      assert(oop_maps != NULL, "must have an oopmap");
  }
#endif

  // align so printing shows nop's instead of random code at the end (SimpleStubs are aligned)
  sasm->align(BytesPerWord);
  // make sure all code is in code buffer
  sasm->flush();
  // create blob - distinguish a few special cases
  CodeBlob* blob = RuntimeStub::new_runtime_stub(name_for(id),
                                                 &code,
                                                 CodeOffsets::frame_never_safe,
                                                 sasm->frame_size(),
                                                 oop_maps,
                                                 sasm->must_gc_arguments());
  // install blob
  assert(blob != NULL, "blob must exist");
  _blobs[id] = blob;
}


void Runtime1::initialize(BufferBlob* blob) {
  // platform-dependent initialization
  initialize_pd();
  // generate stubs
  for (int id = 0; id < number_of_ids; id++) generate_blob_for(blob, (StubID)id);
  // printing
#ifndef PRODUCT
  if (PrintSimpleStubs) {
    ResourceMark rm;
    for (int id = 0; id < number_of_ids; id++) {
      _blobs[id]->print();
      if (_blobs[id]->oop_maps() != NULL) {
        _blobs[id]->oop_maps()->print();
      }
    }
  }
#endif
}


CodeBlob* Runtime1::blob_for(StubID id) {
  assert(0 <= id && id < number_of_ids, "illegal stub id");
  return _blobs[id];
}


const char* Runtime1::name_for(StubID id) {
  assert(0 <= id && id < number_of_ids, "illegal stub id");
  return _blob_names[id];
}

const char* Runtime1::name_for_address(address entry) {
  for (int id = 0; id < number_of_ids; id++) {
    if (entry == entry_for((StubID)id)) return name_for((StubID)id);
  }

#define FUNCTION_CASE(a, f) \
  if ((intptr_t)a == CAST_FROM_FN_PTR(intptr_t, f))  return #f

  FUNCTION_CASE(entry, os::javaTimeMillis);
  FUNCTION_CASE(entry, os::javaTimeNanos);
  FUNCTION_CASE(entry, SharedRuntime::OSR_migration_end);
  FUNCTION_CASE(entry, SharedRuntime::d2f);
  FUNCTION_CASE(entry, SharedRuntime::d2i);
  FUNCTION_CASE(entry, SharedRuntime::d2l);
  FUNCTION_CASE(entry, SharedRuntime::dcos);
  FUNCTION_CASE(entry, SharedRuntime::dexp);
  FUNCTION_CASE(entry, SharedRuntime::dlog);
  FUNCTION_CASE(entry, SharedRuntime::dlog10);
  FUNCTION_CASE(entry, SharedRuntime::dpow);
  FUNCTION_CASE(entry, SharedRuntime::drem);
  FUNCTION_CASE(entry, SharedRuntime::dsin);
  FUNCTION_CASE(entry, SharedRuntime::dtan);
  FUNCTION_CASE(entry, SharedRuntime::f2i);
  FUNCTION_CASE(entry, SharedRuntime::f2l);
  FUNCTION_CASE(entry, SharedRuntime::frem);
  FUNCTION_CASE(entry, SharedRuntime::l2d);
  FUNCTION_CASE(entry, SharedRuntime::l2f);
  FUNCTION_CASE(entry, SharedRuntime::ldiv);
  FUNCTION_CASE(entry, SharedRuntime::lmul);
  FUNCTION_CASE(entry, SharedRuntime::lrem);
  FUNCTION_CASE(entry, SharedRuntime::lrem);
  FUNCTION_CASE(entry, SharedRuntime::dtrace_method_entry);
  FUNCTION_CASE(entry, SharedRuntime::dtrace_method_exit);
  FUNCTION_CASE(entry, is_instance_of);
  FUNCTION_CASE(entry, trace_block_entry);
#ifdef JFR_HAVE_INTRINSICS
  FUNCTION_CASE(entry, JFR_TIME_FUNCTION);
#endif
  FUNCTION_CASE(entry, StubRoutines::updateBytesCRC32());

#undef FUNCTION_CASE

  // Soft float adds more runtime names.
  return pd_name_for_address(entry);
}


JRT_ENTRY(void, Runtime1::new_instance(JavaThread* thread, Klass* klass))
  NOT_PRODUCT(_new_instance_slowcase_cnt++;)

  assert(klass->is_klass(), "not a class");
  Handle holder(THREAD, klass->klass_holder()); // keep the klass alive
  instanceKlassHandle h(thread, klass);
  h->check_valid_for_instantiation(true, CHECK);
  // make sure klass is initialized
  h->initialize(CHECK);
  // allocate instance and return via TLS
  oop obj = h->allocate_instance(CHECK);
  thread->set_vm_result(obj);
JRT_END


JRT_ENTRY(void, Runtime1::new_type_array(JavaThread* thread, Klass* klass, jint length))
  NOT_PRODUCT(_new_type_array_slowcase_cnt++;)
  // Note: no handle for klass needed since they are not used
  //       anymore after new_typeArray() and no GC can happen before.
  //       (This may have to change if this code changes!)
  assert(klass->is_klass(), "not a class");
  BasicType elt_type = TypeArrayKlass::cast(klass)->element_type();
  oop obj = oopFactory::new_typeArray(elt_type, length, CHECK);
  thread->set_vm_result(obj);
  // This is pretty rare but this runtime patch is stressful to deoptimization
  // if we deoptimize here so force a deopt to stress the path.
  if (DeoptimizeALot) {
    deopt_caller();
  }

JRT_END


JRT_ENTRY(void, Runtime1::new_object_array(JavaThread* thread, Klass* array_klass, jint length))
  NOT_PRODUCT(_new_object_array_slowcase_cnt++;)

  // Note: no handle for klass needed since they are not used
  //       anymore after new_objArray() and no GC can happen before.
  //       (This may have to change if this code changes!)
  assert(array_klass->is_klass(), "not a class");
  Handle holder(THREAD, array_klass->klass_holder()); // keep the klass alive
  Klass* elem_klass = ObjArrayKlass::cast(array_klass)->element_klass();
  objArrayOop obj = oopFactory::new_objArray(elem_klass, length, CHECK);
  thread->set_vm_result(obj);
  // This is pretty rare but this runtime patch is stressful to deoptimization
  // if we deoptimize here so force a deopt to stress the path.
  if (DeoptimizeALot) {
    deopt_caller();
  }
JRT_END


JRT_ENTRY(void, Runtime1::new_multi_array(JavaThread* thread, Klass* klass, int rank, jint* dims))
  NOT_PRODUCT(_new_multi_array_slowcase_cnt++;)

  assert(klass->is_klass(), "not a class");
  assert(rank >= 1, "rank must be nonzero");
  Handle holder(THREAD, klass->klass_holder()); // keep the klass alive
  oop obj = ArrayKlass::cast(klass)->multi_allocate(rank, dims, CHECK);
  thread->set_vm_result(obj);
JRT_END


JRT_ENTRY(void, Runtime1::unimplemented_entry(JavaThread* thread, StubID id))
  tty->print_cr("Runtime1::entry_for(%d) returned unimplemented entry point", id);
JRT_END


JRT_ENTRY(void, Runtime1::throw_array_store_exception(JavaThread* thread, oopDesc* obj))
  ResourceMark rm(thread);
  const char* klass_name = obj->klass()->external_name();
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_ArrayStoreException(), klass_name);
JRT_END


// counter_overflow() is called from within C1-compiled methods. The enclosing method is the method
// associated with the top activation record. The inlinee (that is possibly included in the enclosing
// method) method oop is passed as an argument. In order to do that it is embedded in the code as
// a constant.
static nmethod* counter_overflow_helper(JavaThread* THREAD, int branch_bci, Method* m) {
  nmethod* osr_nm = NULL;
  methodHandle method(THREAD, m);

  RegisterMap map(THREAD, false);
  frame fr =  THREAD->last_frame().sender(&map);
  nmethod* nm = (nmethod*) fr.cb();
  assert(nm!= NULL && nm->is_nmethod(), "Sanity check");
  methodHandle enclosing_method(THREAD, nm->method());

  CompLevel level = (CompLevel)nm->comp_level();
  int bci = InvocationEntryBci;
  if (branch_bci != InvocationEntryBci) {
    // Compute desination bci
    address pc = method()->code_base() + branch_bci;
    Bytecodes::Code branch = Bytecodes::code_at(method(), pc);
    int offset = 0;
    switch (branch) {
      case Bytecodes::_if_icmplt: case Bytecodes::_iflt:
      case Bytecodes::_if_icmpgt: case Bytecodes::_ifgt:
      case Bytecodes::_if_icmple: case Bytecodes::_ifle:
      case Bytecodes::_if_icmpge: case Bytecodes::_ifge:
      case Bytecodes::_if_icmpeq: case Bytecodes::_if_acmpeq: case Bytecodes::_ifeq:
      case Bytecodes::_if_icmpne: case Bytecodes::_if_acmpne: case Bytecodes::_ifne:
      case Bytecodes::_ifnull: case Bytecodes::_ifnonnull: case Bytecodes::_goto:
        offset = (int16_t)Bytes::get_Java_u2(pc + 1);
        break;
      case Bytecodes::_goto_w:
        offset = Bytes::get_Java_u4(pc + 1);
        break;
      default: ;
    }
    bci = branch_bci + offset;
  }
  assert(!HAS_PENDING_EXCEPTION, "Should not have any exceptions pending");
  osr_nm = CompilationPolicy::policy()->event(enclosing_method, method, branch_bci, bci, level, nm, THREAD);
  assert(!HAS_PENDING_EXCEPTION, "Event handler should not throw any exceptions");
  return osr_nm;
}

JRT_BLOCK_ENTRY(address, Runtime1::counter_overflow(JavaThread* thread, int bci, Method* method))
  nmethod* osr_nm;
  JRT_BLOCK
    osr_nm = counter_overflow_helper(thread, bci, method);
    if (osr_nm != NULL) {
      RegisterMap map(thread, false);
      frame fr =  thread->last_frame().sender(&map);
      Deoptimization::deoptimize_frame(thread, fr.id());
    }
  JRT_BLOCK_END
  return NULL;
JRT_END

extern void vm_exit(int code);

// Enter this method from compiled code handler below. This is where we transition
// to VM mode. This is done as a helper routine so that the method called directly
// from compiled code does not have to transition to VM. This allows the entry
// method to see if the nmethod that we have just looked up a handler for has
// been deoptimized while we were in the vm. This simplifies the assembly code
// cpu directories.
//
// We are entering here from exception stub (via the entry method below)
// If there is a compiled exception handler in this method, we will continue there;
// otherwise we will unwind the stack and continue at the caller of top frame method
// Note: we enter in Java using a special JRT wrapper. This wrapper allows us to
// control the area where we can allow a safepoint. After we exit the safepoint area we can
// check to see if the handler we are going to return is now in a nmethod that has
// been deoptimized. If that is the case we return the deopt blob
// unpack_with_exception entry instead. This makes life for the exception blob easier
// because making that same check and diverting is painful from assembly language.
JRT_ENTRY_NO_ASYNC(static address, exception_handler_for_pc_helper(JavaThread* thread, oopDesc* ex, address pc, nmethod*& nm))
  // Reset method handle flag.
  thread->set_is_method_handle_return(false);

  Handle exception(thread, ex);
  nm = CodeCache::find_nmethod(pc);
  assert(nm != NULL, "this is not an nmethod");
  // Adjust the pc as needed/
  if (nm->is_deopt_pc(pc)) {
    RegisterMap map(thread, false);
    frame exception_frame = thread->last_frame().sender(&map);
    // if the frame isn't deopted then pc must not correspond to the caller of last_frame
    assert(exception_frame.is_deoptimized_frame(), "must be deopted");
    pc = exception_frame.pc();
  }
#ifdef ASSERT
  assert(exception.not_null(), "NULL exceptions should be handled by throw_exception");
  assert(exception->is_oop(), "just checking");
  // Check that exception is a subclass of Throwable, otherwise we have a VerifyError
  if (!(exception->is_a(SystemDictionary::Throwable_klass()))) {
    if (ExitVMOnVerifyError) vm_exit(-1);
    ShouldNotReachHere();
  }
#endif

  // Check the stack guard pages and reenable them if necessary and there is
  // enough space on the stack to do so.  Use fast exceptions only if the guard
  // pages are enabled.
  bool guard_pages_enabled = thread->stack_yellow_zone_enabled();
  if (!guard_pages_enabled) guard_pages_enabled = thread->reguard_stack();

  if (JvmtiExport::can_post_on_exceptions()) {
    // To ensure correct notification of exception catches and throws
    // we have to deoptimize here.  If we attempted to notify the
    // catches and throws during this exception lookup it's possible
    // we could deoptimize on the way out of the VM and end back in
    // the interpreter at the throw site.  This would result in double
    // notifications since the interpreter would also notify about
    // these same catches and throws as it unwound the frame.

    RegisterMap reg_map(thread);
    frame stub_frame = thread->last_frame();
    frame caller_frame = stub_frame.sender(&reg_map);

    // We don't really want to deoptimize the nmethod itself since we
    // can actually continue in the exception handler ourselves but I
    // don't see an easy way to have the desired effect.
    Deoptimization::deoptimize_frame(thread, caller_frame.id());
    assert(caller_is_deopted(), "Must be deoptimized");

    return SharedRuntime::deopt_blob()->unpack_with_exception_in_tls();
  }

  // ExceptionCache is used only for exceptions at call sites and not for implicit exceptions
  if (guard_pages_enabled) {
    address fast_continuation = nm->handler_for_exception_and_pc(exception, pc);
    if (fast_continuation != NULL) {
      // Set flag if return address is a method handle call site.
      thread->set_is_method_handle_return(nm->is_method_handle_return(pc));
      return fast_continuation;
    }
  }

  // If the stack guard pages are enabled, check whether there is a handler in
  // the current method.  Otherwise (guard pages disabled), force an unwind and
  // skip the exception cache update (i.e., just leave continuation==NULL).
  address continuation = NULL;
  if (guard_pages_enabled) {

    // New exception handling mechanism can support inlined methods
    // with exception handlers since the mappings are from PC to PC

    // debugging support
    // tracing
    if (TraceExceptions) {
      ttyLocker ttyl;
      ResourceMark rm;
      tty->print_cr("Exception <%.*s> (" INTPTR_FORMAT ") thrown in compiled method <%s> at PC " INTPTR_FORMAT " for thread " INTPTR_FORMAT "",
                    MAX_LEN, exception->print_value_string(),
                    p2i((address)exception()), nm->method()->print_value_string(), p2i(pc), p2i(thread));
    }
    // for AbortVMOnException flag
    NOT_PRODUCT(Exceptions::debug_check_abort(exception));

    // Clear out the exception oop and pc since looking up an
    // exception handler can cause class loading, which might throw an
    // exception and those fields are expected to be clear during
    // normal bytecode execution.
    thread->clear_exception_oop_and_pc();

    bool recursive_exception = false;
    continuation = SharedRuntime::compute_compiled_exc_handler(nm, pc, exception, false, false, recursive_exception);
    // If an exception was thrown during exception dispatch, the exception oop may have changed
    thread->set_exception_oop(exception());
    thread->set_exception_pc(pc);

    // the exception cache is used only by non-implicit exceptions
    // Update the exception cache only when there didn't happen
    // another exception during the computation of the compiled
    // exception handler. Checking for exception oop equality is not
    // sufficient because some exceptions are pre-allocated and reused.
    if (continuation != NULL && !recursive_exception) {
      nm->add_handler_for_exception_and_pc(exception, pc, continuation);
    }
  }

  thread->set_vm_result(exception());
  // Set flag if return address is a method handle call site.
  thread->set_is_method_handle_return(nm->is_method_handle_return(pc));

  if (TraceExceptions) {
    ttyLocker ttyl;
    ResourceMark rm;
    tty->print_cr("Thread " PTR_FORMAT " continuing at PC " PTR_FORMAT " for exception thrown at PC " PTR_FORMAT,
                  p2i(thread), p2i(continuation), p2i(pc));
  }

  return continuation;
JRT_END

// Enter this method from compiled code only if there is a Java exception handler
// in the method handling the exception.
// We are entering here from exception stub. We don't do a normal VM transition here.
// We do it in a helper. This is so we can check to see if the nmethod we have just
// searched for an exception handler has been deoptimized in the meantime.
address Runtime1::exception_handler_for_pc(JavaThread* thread) {
  oop exception = thread->exception_oop();
  address pc = thread->exception_pc();
  // Still in Java mode
  DEBUG_ONLY(ResetNoHandleMark rnhm);
  nmethod* nm = NULL;
  address continuation = NULL;
  {
    // Enter VM mode by calling the helper
    ResetNoHandleMark rnhm;
    continuation = exception_handler_for_pc_helper(thread, exception, pc, nm);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Now check to see if the nmethod we were called from is now deoptimized.
  // If so we must return to the deopt blob and deoptimize the nmethod
  if (nm != NULL && caller_is_deopted()) {
    continuation = SharedRuntime::deopt_blob()->unpack_with_exception_in_tls();
  }

  assert(continuation != NULL, "no handler found");
  return continuation;
}


JRT_ENTRY(void, Runtime1::throw_range_check_exception(JavaThread* thread, int index))
  NOT_PRODUCT(_throw_range_check_exception_count++;)
  char message[jintAsStringSize];
  sprintf(message, "%d", index);
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_ArrayIndexOutOfBoundsException(), message);
JRT_END


JRT_ENTRY(void, Runtime1::throw_index_exception(JavaThread* thread, int index))
  NOT_PRODUCT(_throw_index_exception_count++;)
  char message[16];
  sprintf(message, "%d", index);
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_IndexOutOfBoundsException(), message);
JRT_END


JRT_ENTRY(void, Runtime1::throw_div0_exception(JavaThread* thread))
  NOT_PRODUCT(_throw_div0_exception_count++;)
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_ArithmeticException(), "/ by zero");
JRT_END


JRT_ENTRY(void, Runtime1::throw_null_pointer_exception(JavaThread* thread))
  NOT_PRODUCT(_throw_null_pointer_exception_count++;)
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_NullPointerException());
JRT_END


JRT_ENTRY(void, Runtime1::throw_class_cast_exception(JavaThread* thread, oopDesc* object))
  NOT_PRODUCT(_throw_class_cast_exception_count++;)
  ResourceMark rm(thread);
  char* message = SharedRuntime::generate_class_cast_message(
    thread, object->klass()->external_name());
  SharedRuntime::throw_and_post_jvmti_exception(
    thread, vmSymbols::java_lang_ClassCastException(), message);
JRT_END


JRT_ENTRY(void, Runtime1::throw_incompatible_class_change_error(JavaThread* thread))
  NOT_PRODUCT(_throw_incompatible_class_change_error_count++;)
  ResourceMark rm(thread);
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_IncompatibleClassChangeError());
JRT_END


JRT_ENTRY_NO_ASYNC(void, Runtime1::monitorenter(JavaThread* thread, oopDesc* obj, BasicObjectLock* lock))
  NOT_PRODUCT(_monitorenter_slowcase_cnt++;)
  if (PrintBiasedLockingStatistics) {
    Atomic::inc(BiasedLocking::slow_path_entry_count_addr());
  }
  Handle h_obj(thread, obj);
  assert(h_obj()->is_oop(), "must be NULL or an object");
  if (UseBiasedLocking) {
    // Retry fast entry if bias is revoked to avoid unnecessary inflation
    ObjectSynchronizer::fast_enter(h_obj, lock->lock(), true, CHECK);
  } else {
    if (UseFastLocking) {
      // When using fast locking, the compiled code has already tried the fast case
      assert(obj == lock->obj(), "must match");
      ObjectSynchronizer::slow_enter(h_obj, lock->lock(), THREAD);
    } else {
      lock->set_obj(obj);
      ObjectSynchronizer::fast_enter(h_obj, lock->lock(), false, THREAD);
    }
  }
JRT_END


JRT_LEAF(void, Runtime1::monitorexit(JavaThread* thread, BasicObjectLock* lock))
  NOT_PRODUCT(_monitorexit_slowcase_cnt++;)
  assert(thread == JavaThread::current(), "threads must correspond");
  assert(thread->last_Java_sp(), "last_Java_sp must be set");
  // monitorexit is non-blocking (leaf routine) => no exceptions can be thrown
  EXCEPTION_MARK;

  oop obj = lock->obj();
  assert(obj->is_oop(), "must be NULL or an object");
  if (UseFastLocking) {
    // When using fast locking, the compiled code has already tried the fast case
    ObjectSynchronizer::slow_exit(obj, lock->lock(), THREAD);
  } else {
    ObjectSynchronizer::fast_exit(obj, lock->lock(), THREAD);
  }
JRT_END

// Cf. OptoRuntime::deoptimize_caller_frame
JRT_ENTRY(void, Runtime1::deoptimize(JavaThread* thread))
  // Called from within the owner thread, so no need for safepoint
  RegisterMap reg_map(thread, false);
  frame stub_frame = thread->last_frame();
  assert(stub_frame.is_runtime_frame(), "sanity check");
  frame caller_frame = stub_frame.sender(&reg_map);

  // We are coming from a compiled method; check this is true.
  assert(CodeCache::find_nmethod(caller_frame.pc()) != NULL, "sanity");

  // Deoptimize the caller frame.
  Deoptimization::deoptimize_frame(thread, caller_frame.id());

  // Return to the now deoptimized frame.
JRT_END


static Klass* resolve_field_return_klass(methodHandle caller, int bci, TRAPS) {
  Bytecode_field field_access(caller, bci);
  // This can be static or non-static field access
  Bytecodes::Code code       = field_access.code();

  // We must load class, initialize class and resolvethe field
  fieldDescriptor result; // initialize class if needed
  constantPoolHandle constants(THREAD, caller->constants());
  LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), CHECK_NULL);
  return result.field_holder();
}


//
// This routine patches sites where a class wasn't loaded or
// initialized at the time the code was generated.  It handles
// references to classes, fields and forcing of initialization.  Most
// of the cases are straightforward and involving simply forcing
// resolution of a class, rewriting the instruction stream with the
// needed constant and replacing the call in this function with the
// patched code.  The case for static field is more complicated since
// the thread which is in the process of initializing a class can
// access it's static fields but other threads can't so the code
// either has to deoptimize when this case is detected or execute a
// check that the current thread is the initializing thread.  The
// current
//
// Patches basically look like this:
//
//
// patch_site: jmp patch stub     ;; will be patched
// continue:   ...
//             ...
//             ...
//             ...
//
// They have a stub which looks like this:
//
//             ;; patch body
//             movl <const>, reg           (for class constants)
//        <or> movl [reg1 + <const>], reg  (for field offsets)
//        <or> movl reg, [reg1 + <const>]  (for field offsets)
//             <being_init offset> <bytes to copy> <bytes to skip>
// patch_stub: call Runtime1::patch_code (through a runtime stub)
//             jmp patch_site
//
//
// A normal patch is done by rewriting the patch body, usually a move,
// and then copying it into place over top of the jmp instruction
// being careful to flush caches and doing it in an MP-safe way.  The
// constants following the patch body are used to find various pieces
// of the patch relative to the call site for Runtime1::patch_code.
// The case for getstatic and putstatic is more complicated because
// getstatic and putstatic have special semantics when executing while
// the class is being initialized.  getstatic/putstatic on a class
// which is being_initialized may be executed by the initializing
// thread but other threads have to block when they execute it.  This
// is accomplished in compiled code by executing a test of the current
// thread against the initializing thread of the class.  It's emitted
// as boilerplate in their stub which allows the patched code to be
// executed before it's copied back into the main body of the nmethod.
//
// being_init: get_thread(<tmp reg>
//             cmpl [reg1 + <init_thread_offset>], <tmp reg>
//             jne patch_stub
//             movl [reg1 + <const>], reg  (for field offsets)  <or>
//             movl reg, [reg1 + <const>]  (for field offsets)
//             jmp continue
//             <being_init offset> <bytes to copy> <bytes to skip>
// patch_stub: jmp Runtim1::patch_code (through a runtime stub)
//             jmp patch_site
//
// If the class is being initialized the patch body is rewritten and
// the patch site is rewritten to jump to being_init, instead of
// patch_stub.  Whenever this code is executed it checks the current
// thread against the intializing thread so other threads will enter
// the runtime and end up blocked waiting the class to finish
// initializing inside the calls to resolve_field below.  The
// initializing class will continue on it's way.  Once the class is
// fully_initialized, the intializing_thread of the class becomes
// NULL, so the next thread to execute this code will fail the test,
// call into patch_code and complete the patching process by copying
// the patch body back into the main part of the nmethod and resume
// executing.
//
//

JRT_ENTRY(void, Runtime1::patch_code(JavaThread* thread, Runtime1::StubID stub_id ))
  NOT_PRODUCT(_patch_code_slowcase_cnt++;)

#ifdef AARCH64
  // AArch64 does not patch C1-generated code.
  ShouldNotReachHere();
#endif

  ResourceMark rm(thread);
  RegisterMap reg_map(thread, false);
  frame runtime_frame = thread->last_frame();
  frame caller_frame = runtime_frame.sender(&reg_map);

  // last java frame on stack
  vframeStream vfst(thread, true);
  assert(!vfst.at_end(), "Java frame must exist");

  methodHandle caller_method(THREAD, vfst.method());
  // Note that caller_method->code() may not be same as caller_code because of OSR's
  // Note also that in the presence of inlining it is not guaranteed
  // that caller_method() == caller_code->method()

  int bci = vfst.bci();
  Bytecodes::Code code = caller_method()->java_code_at(bci);

#ifndef PRODUCT
  // this is used by assertions in the access_field_patching_id
  BasicType patch_field_type = T_ILLEGAL;
#endif // PRODUCT
  bool deoptimize_for_volatile = false;
  int patch_field_offset = -1;
  KlassHandle init_klass(THREAD, NULL); // klass needed by load_klass_patching code
  KlassHandle load_klass(THREAD, NULL); // klass needed by load_klass_patching code
  Handle mirror(THREAD, NULL);                    // oop needed by load_mirror_patching code
  Handle appendix(THREAD, NULL);                  // oop needed by appendix_patching code
  bool load_klass_or_mirror_patch_id =
    (stub_id == Runtime1::load_klass_patching_id || stub_id == Runtime1::load_mirror_patching_id);

  if (stub_id == Runtime1::access_field_patching_id) {

    Bytecode_field field_access(caller_method, bci);
    fieldDescriptor result; // initialize class if needed
    Bytecodes::Code code = field_access.code();
    constantPoolHandle constants(THREAD, caller_method->constants());
    LinkResolver::resolve_field_access(result, constants, field_access.index(), Bytecodes::java_code(code), CHECK);
    patch_field_offset = result.offset();

    // If we're patching a field which is volatile then at compile it
    // must not have been know to be volatile, so the generated code
    // isn't correct for a volatile reference.  The nmethod has to be
    // deoptimized so that the code can be regenerated correctly.
    // This check is only needed for access_field_patching since this
    // is the path for patching field offsets.  load_klass is only
    // used for patching references to oops which don't need special
    // handling in the volatile case.
    deoptimize_for_volatile = result.access_flags().is_volatile();

#ifndef PRODUCT
    patch_field_type = result.field_type();
#endif
  } else if (load_klass_or_mirror_patch_id) {
    Klass* k = NULL;
    switch (code) {
      case Bytecodes::_putstatic:
      case Bytecodes::_getstatic:
        { Klass* klass = resolve_field_return_klass(caller_method, bci, CHECK);
          init_klass = KlassHandle(THREAD, klass);
          mirror = Handle(THREAD, klass->java_mirror());
        }
        break;
      case Bytecodes::_new:
        { Bytecode_new bnew(caller_method(), caller_method->bcp_from(bci));
          k = caller_method->constants()->klass_at(bnew.index(), CHECK);
        }
        break;
      case Bytecodes::_multianewarray:
        { Bytecode_multianewarray mna(caller_method(), caller_method->bcp_from(bci));
          k = caller_method->constants()->klass_at(mna.index(), CHECK);
        }
        break;
      case Bytecodes::_instanceof:
        { Bytecode_instanceof io(caller_method(), caller_method->bcp_from(bci));
          k = caller_method->constants()->klass_at(io.index(), CHECK);
        }
        break;
      case Bytecodes::_checkcast:
        { Bytecode_checkcast cc(caller_method(), caller_method->bcp_from(bci));
          k = caller_method->constants()->klass_at(cc.index(), CHECK);
        }
        break;
      case Bytecodes::_anewarray:
        { Bytecode_anewarray anew(caller_method(), caller_method->bcp_from(bci));
          Klass* ek = caller_method->constants()->klass_at(anew.index(), CHECK);
          k = ek->array_klass(CHECK);
        }
        break;
      case Bytecodes::_ldc:
      case Bytecodes::_ldc_w:
        {
          Bytecode_loadconstant cc(caller_method, bci);
          oop m = cc.resolve_constant(CHECK);
          mirror = Handle(THREAD, m);
        }
        break;
      default: fatal("unexpected bytecode for load_klass_or_mirror_patch_id");
    }
    // convert to handle
    load_klass = KlassHandle(THREAD, k);
  } else if (stub_id == load_appendix_patching_id) {
    Bytecode_invoke bytecode(caller_method, bci);
    Bytecodes::Code bc = bytecode.invoke_code();

    CallInfo info;
    constantPoolHandle pool(thread, caller_method->constants());
    int index = bytecode.index();
    LinkResolver::resolve_invoke(info, Handle(), pool, index, bc, CHECK);
    appendix = info.resolved_appendix();
    switch (bc) {
      case Bytecodes::_invokehandle: {
        int cache_index = ConstantPool::decode_cpcache_index(index, true);
        assert(cache_index >= 0 && cache_index < pool->cache()->length(), "unexpected cache index");
        pool->cache()->entry_at(cache_index)->set_method_handle(pool, info);
        break;
      }
      case Bytecodes::_invokedynamic: {
        pool->invokedynamic_cp_cache_entry_at(index)->set_dynamic_call(pool, info);
        break;
      }
      default: fatal("unexpected bytecode for load_appendix_patching_id");
    }
  } else {
    ShouldNotReachHere();
  }

  if (deoptimize_for_volatile) {
    // At compile time we assumed the field wasn't volatile but after
    // loading it turns out it was volatile so we have to throw the
    // compiled code out and let it be regenerated.
    if (TracePatching) {
      tty->print_cr("Deoptimizing for patching volatile field reference");
    }
    // It's possible the nmethod was invalidated in the last
    // safepoint, but if it's still alive then make it not_entrant.
    nmethod* nm = CodeCache::find_nmethod(caller_frame.pc());
    if (nm != NULL) {
      nm->make_not_entrant();
    }

    Deoptimization::deoptimize_frame(thread, caller_frame.id());

    // Return to the now deoptimized frame.
  }

  // Now copy code back
  {
    MutexLockerEx ml_patch (Patching_lock, Mutex::_no_safepoint_check_flag);
    //
    // Deoptimization may have happened while we waited for the lock.
    // In that case we don't bother to do any patching we just return
    // and let the deopt happen
    if (!caller_is_deopted()) {
      NativeGeneralJump* jump = nativeGeneralJump_at(caller_frame.pc());
      address instr_pc = jump->jump_destination();
      NativeInstruction* ni = nativeInstruction_at(instr_pc);
      if (ni->is_jump() ) {
        // the jump has not been patched yet
        // The jump destination is slow case and therefore not part of the stubs
        // (stubs are only for StaticCalls)

        // format of buffer
        //    ....
        //    instr byte 0     <-- copy_buff
        //    instr byte 1
        //    ..
        //    instr byte n-1
        //      n
        //    ....             <-- call destination

        address stub_location = caller_frame.pc() + PatchingStub::patch_info_offset();
        unsigned char* byte_count = (unsigned char*) (stub_location - 1);
        unsigned char* byte_skip = (unsigned char*) (stub_location - 2);
        unsigned char* being_initialized_entry_offset = (unsigned char*) (stub_location - 3);
        address copy_buff = stub_location - *byte_skip - *byte_count;
        address being_initialized_entry = stub_location - *being_initialized_entry_offset;
        if (TracePatching) {
          ttyLocker ttyl;
          tty->print_cr(" Patching %s at bci %d at address " INTPTR_FORMAT "  (%s)", Bytecodes::name(code), bci,
                        p2i(instr_pc), (stub_id == Runtime1::access_field_patching_id) ? "field" : "klass");
          nmethod* caller_code = CodeCache::find_nmethod(caller_frame.pc());
          assert(caller_code != NULL, "nmethod not found");

          // NOTE we use pc() not original_pc() because we already know they are
          // identical otherwise we'd have never entered this block of code

          OopMap* map = caller_code->oop_map_for_return_address(caller_frame.pc());
          assert(map != NULL, "null check");
          map->print();
          tty->cr();

          Disassembler::decode(copy_buff, copy_buff + *byte_count, tty);
        }
        // depending on the code below, do_patch says whether to copy the patch body back into the nmethod
        bool do_patch = true;
        if (stub_id == Runtime1::access_field_patching_id) {
          // The offset may not be correct if the class was not loaded at code generation time.
          // Set it now.
          NativeMovRegMem* n_move = nativeMovRegMem_at(copy_buff);
          assert(n_move->offset() == 0 || (n_move->offset() == 4 && (patch_field_type == T_DOUBLE || patch_field_type == T_LONG)), "illegal offset for type");
          assert(patch_field_offset >= 0, "illegal offset");
          n_move->add_offset_in_bytes(patch_field_offset);
        } else if (load_klass_or_mirror_patch_id) {
          // If a getstatic or putstatic is referencing a klass which
          // isn't fully initialized, the patch body isn't copied into
          // place until initialization is complete.  In this case the
          // patch site is setup so that any threads besides the
          // initializing thread are forced to come into the VM and
          // block.
          do_patch = (code != Bytecodes::_getstatic && code != Bytecodes::_putstatic) ||
                     InstanceKlass::cast(init_klass())->is_initialized();
          NativeGeneralJump* jump = nativeGeneralJump_at(instr_pc);
          if (jump->jump_destination() == being_initialized_entry) {
            assert(do_patch == true, "initialization must be complete at this point");
          } else {
            // patch the instruction <move reg, klass>
            NativeMovConstReg* n_copy = nativeMovConstReg_at(copy_buff);

            assert(n_copy->data() == 0 ||
                   n_copy->data() == (intptr_t)Universe::non_oop_word(),
                   "illegal init value");
            if (stub_id == Runtime1::load_klass_patching_id) {
              assert(load_klass() != NULL, "klass not set");
              n_copy->set_data((intx) (load_klass()));
            } else {
              assert(mirror() != NULL, "klass not set");
              // Don't need a G1 pre-barrier here since we assert above that data isn't an oop.
              n_copy->set_data(cast_from_oop<intx>(mirror()));
            }

            if (TracePatching) {
              Disassembler::decode(copy_buff, copy_buff + *byte_count, tty);
            }
          }
        } else if (stub_id == Runtime1::load_appendix_patching_id) {
          NativeMovConstReg* n_copy = nativeMovConstReg_at(copy_buff);
          assert(n_copy->data() == 0 ||
                 n_copy->data() == (intptr_t)Universe::non_oop_word(),
                 "illegal init value");
          n_copy->set_data(cast_from_oop<intx>(appendix()));

          if (TracePatching) {
            Disassembler::decode(copy_buff, copy_buff + *byte_count, tty);
          }
        } else {
          ShouldNotReachHere();
        }

#if defined(SPARC) || defined(PPC)
        if (load_klass_or_mirror_patch_id ||
            stub_id == Runtime1::load_appendix_patching_id) {
          // Update the location in the nmethod with the proper
          // metadata.  When the code was generated, a NULL was stuffed
          // in the metadata table and that table needs to be update to
          // have the right value.  On intel the value is kept
          // directly in the instruction instead of in the metadata
          // table, so set_data above effectively updated the value.
          nmethod* nm = CodeCache::find_nmethod(instr_pc);
          assert(nm != NULL, "invalid nmethod_pc");
          RelocIterator mds(nm, copy_buff, copy_buff + 1);
          bool found = false;
          while (mds.next() && !found) {
            if (mds.type() == relocInfo::oop_type) {
              assert(stub_id == Runtime1::load_mirror_patching_id ||
                     stub_id == Runtime1::load_appendix_patching_id, "wrong stub id");
              oop_Relocation* r = mds.oop_reloc();
              oop* oop_adr = r->oop_addr();
              *oop_adr = stub_id == Runtime1::load_mirror_patching_id ? mirror() : appendix();
              r->fix_oop_relocation();
              found = true;
            } else if (mds.type() == relocInfo::metadata_type) {
              assert(stub_id == Runtime1::load_klass_patching_id, "wrong stub id");
              metadata_Relocation* r = mds.metadata_reloc();
              Metadata** metadata_adr = r->metadata_addr();
              *metadata_adr = load_klass();
              r->fix_metadata_relocation();
              found = true;
            }
          }
          assert(found, "the metadata must exist!");
        }
#endif
        if (do_patch) {
          // replace instructions
          // first replace the tail, then the call
#ifdef ARM
          if((load_klass_or_mirror_patch_id ||
              stub_id == Runtime1::load_appendix_patching_id) &&
              nativeMovConstReg_at(copy_buff)->is_pc_relative()) {
            nmethod* nm = CodeCache::find_nmethod(instr_pc);
            address addr = NULL;
            assert(nm != NULL, "invalid nmethod_pc");
            RelocIterator mds(nm, copy_buff, copy_buff + 1);
            while (mds.next()) {
              if (mds.type() == relocInfo::oop_type) {
                assert(stub_id == Runtime1::load_mirror_patching_id ||
                       stub_id == Runtime1::load_appendix_patching_id, "wrong stub id");
                oop_Relocation* r = mds.oop_reloc();
                addr = (address)r->oop_addr();
                break;
              } else if (mds.type() == relocInfo::metadata_type) {
                assert(stub_id == Runtime1::load_klass_patching_id, "wrong stub id");
                metadata_Relocation* r = mds.metadata_reloc();
                addr = (address)r->metadata_addr();
                break;
              }
            }
            assert(addr != NULL, "metadata relocation must exist");
            copy_buff -= *byte_count;
            NativeMovConstReg* n_copy2 = nativeMovConstReg_at(copy_buff);
            n_copy2->set_pc_relative_offset(addr, instr_pc);
          }
#endif

          for (int i = NativeCall::instruction_size; i < *byte_count; i++) {
            address ptr = copy_buff + i;
            int a_byte = (*ptr) & 0xFF;
            address dst = instr_pc + i;
            *(unsigned char*)dst = (unsigned char) a_byte;
          }
          ICache::invalidate_range(instr_pc, *byte_count);
          NativeGeneralJump::replace_mt_safe(instr_pc, copy_buff);

          if (load_klass_or_mirror_patch_id ||
              stub_id == Runtime1::load_appendix_patching_id) {
            relocInfo::relocType rtype =
              (stub_id == Runtime1::load_klass_patching_id) ?
                                   relocInfo::metadata_type :
                                   relocInfo::oop_type;
            // update relocInfo to metadata
            nmethod* nm = CodeCache::find_nmethod(instr_pc);
            assert(nm != NULL, "invalid nmethod_pc");

            // The old patch site is now a move instruction so update
            // the reloc info so that it will get updated during
            // future GCs.
            RelocIterator iter(nm, (address)instr_pc, (address)(instr_pc + 1));
            relocInfo::change_reloc_info_for_address(&iter, (address) instr_pc,
                                                     relocInfo::none, rtype);
#ifdef SPARC
            // Sparc takes two relocations for an metadata so update the second one.
            address instr_pc2 = instr_pc + NativeMovConstReg::add_offset;
            RelocIterator iter2(nm, instr_pc2, instr_pc2 + 1);
            relocInfo::change_reloc_info_for_address(&iter2, (address) instr_pc2,
                                                     relocInfo::none, rtype);
#endif
#ifdef PPC
          { address instr_pc2 = instr_pc + NativeMovConstReg::lo_offset;
            RelocIterator iter2(nm, instr_pc2, instr_pc2 + 1);
            relocInfo::change_reloc_info_for_address(&iter2, (address) instr_pc2,
                                                     relocInfo::none, rtype);
          }
#endif
          }

        } else {
          ICache::invalidate_range(copy_buff, *byte_count);
          NativeGeneralJump::insert_unconditional(instr_pc, being_initialized_entry);
        }
      }
    }
  }

  // If we are patching in a non-perm oop, make sure the nmethod
  // is on the right list.
  if (ScavengeRootsInCode && ((mirror.not_null() && mirror()->is_scavengable()) ||
                              (appendix.not_null() && appendix->is_scavengable()))) {
    MutexLockerEx ml_code (CodeCache_lock, Mutex::_no_safepoint_check_flag);
    nmethod* nm = CodeCache::find_nmethod(caller_frame.pc());
    guarantee(nm != NULL, "only nmethods can contain non-perm oops");
    if (!nm->on_scavenge_root_list()) {
      CodeCache::add_scavenge_root_nmethod(nm);
    }

    // Since we've patched some oops in the nmethod,
    // (re)register it with the heap.
    Universe::heap()->register_nmethod(nm);
  }
JRT_END

//
// Entry point for compiled code. We want to patch a nmethod.
// We don't do a normal VM transition here because we want to
// know after the patching is complete and any safepoint(s) are taken
// if the calling nmethod was deoptimized. We do this by calling a
// helper method which does the normal VM transition and when it
// completes we can check for deoptimization. This simplifies the
// assembly code in the cpu directories.
//
#ifndef TARGET_ARCH_aarch64
int Runtime1::move_klass_patching(JavaThread* thread) {
//
// NOTE: we are still in Java
//
  Thread* THREAD = thread;
  debug_only(NoHandleMark nhm;)
  {
    // Enter VM mode

    ResetNoHandleMark rnhm;
    patch_code(thread, load_klass_patching_id);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Return true if calling code is deoptimized

  return caller_is_deopted();
}

int Runtime1::move_mirror_patching(JavaThread* thread) {
//
// NOTE: we are still in Java
//
  Thread* THREAD = thread;
  debug_only(NoHandleMark nhm;)
  {
    // Enter VM mode

    ResetNoHandleMark rnhm;
    patch_code(thread, load_mirror_patching_id);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Return true if calling code is deoptimized

  return caller_is_deopted();
}

int Runtime1::move_appendix_patching(JavaThread* thread) {
//
// NOTE: we are still in Java
//
  Thread* THREAD = thread;
  debug_only(NoHandleMark nhm;)
  {
    // Enter VM mode

    ResetNoHandleMark rnhm;
    patch_code(thread, load_appendix_patching_id);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Return true if calling code is deoptimized

  return caller_is_deopted();
}
//
// Entry point for compiled code. We want to patch a nmethod.
// We don't do a normal VM transition here because we want to
// know after the patching is complete and any safepoint(s) are taken
// if the calling nmethod was deoptimized. We do this by calling a
// helper method which does the normal VM transition and when it
// completes we can check for deoptimization. This simplifies the
// assembly code in the cpu directories.
//

int Runtime1::access_field_patching(JavaThread* thread) {
//
// NOTE: we are still in Java
//
  Thread* THREAD = thread;
  debug_only(NoHandleMark nhm;)
  {
    // Enter VM mode

    ResetNoHandleMark rnhm;
    patch_code(thread, access_field_patching_id);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Return true if calling code is deoptimized

  return caller_is_deopted();
JRT_END
#endif

JRT_LEAF(void, Runtime1::trace_block_entry(jint block_id))
  // for now we just print out the block id
  tty->print("%d ", block_id);
JRT_END


// Array copy return codes.
enum {
  ac_failed = -1, // arraycopy failed
  ac_ok = 0       // arraycopy succeeded
};


// Below length is the # elements copied.
template <class T> int obj_arraycopy_work(oopDesc* src, T* src_addr,
                                          oopDesc* dst, T* dst_addr,
                                          int length) {

  // For performance reasons, we assume we are using a card marking write
  // barrier. The assert will fail if this is not the case.
  // Note that we use the non-virtual inlineable variant of write_ref_array.
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->has_write_ref_array_opt(), "Barrier set must have ref array opt");
  assert(bs->has_write_ref_array_pre_opt(), "For pre-barrier as well.");
  if (src == dst) {
    // same object, no check
    bs->write_ref_array_pre(dst_addr, length);
    Copy::conjoint_oops_atomic(src_addr, dst_addr, length);
    bs->write_ref_array((HeapWord*)dst_addr, length);
    return ac_ok;
  } else {
    Klass* bound = ObjArrayKlass::cast(dst->klass())->element_klass();
    Klass* stype = ObjArrayKlass::cast(src->klass())->element_klass();
    if (stype == bound || stype->is_subtype_of(bound)) {
      // Elements are guaranteed to be subtypes, so no check necessary
      bs->write_ref_array_pre(dst_addr, length);
      Copy::conjoint_oops_atomic(src_addr, dst_addr, length);
      bs->write_ref_array((HeapWord*)dst_addr, length);
      return ac_ok;
    }
  }
  return ac_failed;
}

// fast and direct copy of arrays; returning -1, means that an exception may be thrown
// and we did not copy anything
JRT_LEAF(int, Runtime1::arraycopy(oopDesc* src, int src_pos, oopDesc* dst, int dst_pos, int length))
#ifndef PRODUCT
  _generic_arraycopy_cnt++;        // Slow-path oop array copy
#endif

  if (src == NULL || dst == NULL || src_pos < 0 || dst_pos < 0 || length < 0) return ac_failed;
  if (!dst->is_array() || !src->is_array()) return ac_failed;
  if ((unsigned int) arrayOop(src)->length() < (unsigned int)src_pos + (unsigned int)length) return ac_failed;
  if ((unsigned int) arrayOop(dst)->length() < (unsigned int)dst_pos + (unsigned int)length) return ac_failed;

  if (length == 0) return ac_ok;
  if (src->is_typeArray()) {
    Klass* klass_oop = src->klass();
    if (klass_oop != dst->klass()) return ac_failed;
    TypeArrayKlass* klass = TypeArrayKlass::cast(klass_oop);
    const int l2es = klass->log2_element_size();
    const int ihs = klass->array_header_in_bytes() / wordSize;
    char* src_addr = (char*) ((oopDesc**)src + ihs) + (src_pos << l2es);
    char* dst_addr = (char*) ((oopDesc**)dst + ihs) + (dst_pos << l2es);
    // Potential problem: memmove is not guaranteed to be word atomic
    // Revisit in Merlin
    memmove(dst_addr, src_addr, length << l2es);
    return ac_ok;
  } else if (src->is_objArray() && dst->is_objArray()) {
    if (UseCompressedOops) {
      narrowOop *src_addr  = objArrayOop(src)->obj_at_addr<narrowOop>(src_pos);
      narrowOop *dst_addr  = objArrayOop(dst)->obj_at_addr<narrowOop>(dst_pos);
      return obj_arraycopy_work(src, src_addr, dst, dst_addr, length);
    } else {
      oop *src_addr  = objArrayOop(src)->obj_at_addr<oop>(src_pos);
      oop *dst_addr  = objArrayOop(dst)->obj_at_addr<oop>(dst_pos);
      return obj_arraycopy_work(src, src_addr, dst, dst_addr, length);
    }
  }
  return ac_failed;
JRT_END


JRT_LEAF(void, Runtime1::primitive_arraycopy(HeapWord* src, HeapWord* dst, int length))
#ifndef PRODUCT
  _primitive_arraycopy_cnt++;
#endif

  if (length == 0) return;
  // Not guaranteed to be word atomic, but that doesn't matter
  // for anything but an oop array, which is covered by oop_arraycopy.
  Copy::conjoint_jbytes(src, dst, length);
JRT_END

JRT_LEAF(void, Runtime1::oop_arraycopy(HeapWord* src, HeapWord* dst, int num))
#ifndef PRODUCT
  _oop_arraycopy_cnt++;
#endif

  if (num == 0) return;
  BarrierSet* bs = Universe::heap()->barrier_set();
  assert(bs->has_write_ref_array_opt(), "Barrier set must have ref array opt");
  assert(bs->has_write_ref_array_pre_opt(), "For pre-barrier as well.");
  if (UseCompressedOops) {
    bs->write_ref_array_pre((narrowOop*)dst, num);
    Copy::conjoint_oops_atomic((narrowOop*) src, (narrowOop*) dst, num);
  } else {
    bs->write_ref_array_pre((oop*)dst, num);
    Copy::conjoint_oops_atomic((oop*) src, (oop*) dst, num);
  }
  bs->write_ref_array(dst, num);
JRT_END


JRT_LEAF(int, Runtime1::is_instance_of(oopDesc* mirror, oopDesc* obj))
  // had to return int instead of bool, otherwise there may be a mismatch
  // between the C calling convention and the Java one.
  // e.g., on x86, GCC may clear only %al when returning a bool false, but
  // JVM takes the whole %eax as the return value, which may misinterpret
  // the return value as a boolean true.

  assert(mirror != NULL, "should null-check on mirror before calling");
  Klass* k = java_lang_Class::as_Klass(mirror);
  return (k != NULL && obj != NULL && obj->is_a(k)) ? 1 : 0;
JRT_END

JRT_ENTRY(void, Runtime1::predicate_failed_trap(JavaThread* thread))
  ResourceMark rm;

  assert(!TieredCompilation, "incompatible with tiered compilation");

  RegisterMap reg_map(thread, false);
  frame runtime_frame = thread->last_frame();
  frame caller_frame = runtime_frame.sender(&reg_map);

  nmethod* nm = CodeCache::find_nmethod(caller_frame.pc());
  assert (nm != NULL, "no more nmethod?");
  nm->make_not_entrant();

  methodHandle m(nm->method());
  MethodData* mdo = m->method_data();

  if (mdo == NULL && !HAS_PENDING_EXCEPTION) {
    // Build an MDO.  Ignore errors like OutOfMemory;
    // that simply means we won't have an MDO to update.
    Method::build_interpreter_method_data(m, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      assert((PENDING_EXCEPTION->is_a(SystemDictionary::OutOfMemoryError_klass())), "we expect only an OOM error here");
      CLEAR_PENDING_EXCEPTION;
    }
    mdo = m->method_data();
  }

  if (mdo != NULL) {
    mdo->inc_trap_count(Deoptimization::Reason_none);
  }

  if (TracePredicateFailedTraps) {
    stringStream ss1, ss2;
    vframeStream vfst(thread);
    methodHandle inlinee = methodHandle(vfst.method());
    inlinee->print_short_name(&ss1);
    m->print_short_name(&ss2);
    tty->print_cr("Predicate failed trap in method %s at bci %d inlined in %s at pc " INTPTR_FORMAT, ss1.as_string(), vfst.bci(), ss2.as_string(), p2i(caller_frame.pc()));
  }


  Deoptimization::deoptimize_frame(thread, caller_frame.id());

JRT_END

#ifndef PRODUCT
void Runtime1::print_statistics() {
  tty->print_cr("C1 Runtime statistics:");
  tty->print_cr(" _resolve_invoke_virtual_cnt:     %d", SharedRuntime::_resolve_virtual_ctr);
  tty->print_cr(" _resolve_invoke_opt_virtual_cnt: %d", SharedRuntime::_resolve_opt_virtual_ctr);
  tty->print_cr(" _resolve_invoke_static_cnt:      %d", SharedRuntime::_resolve_static_ctr);
  tty->print_cr(" _handle_wrong_method_cnt:        %d", SharedRuntime::_wrong_method_ctr);
  tty->print_cr(" _ic_miss_cnt:                    %d", SharedRuntime::_ic_miss_ctr);
  tty->print_cr(" _generic_arraycopy_cnt:          %d", _generic_arraycopy_cnt);
  tty->print_cr(" _generic_arraycopystub_cnt:      %d", _generic_arraycopystub_cnt);
  tty->print_cr(" _byte_arraycopy_cnt:             %d", _byte_arraycopy_cnt);
  tty->print_cr(" _short_arraycopy_cnt:            %d", _short_arraycopy_cnt);
  tty->print_cr(" _int_arraycopy_cnt:              %d", _int_arraycopy_cnt);
  tty->print_cr(" _long_arraycopy_cnt:             %d", _long_arraycopy_cnt);
  tty->print_cr(" _primitive_arraycopy_cnt:        %d", _primitive_arraycopy_cnt);
  tty->print_cr(" _oop_arraycopy_cnt (C):          %d", Runtime1::_oop_arraycopy_cnt);
  tty->print_cr(" _oop_arraycopy_cnt (stub):       %d", _oop_arraycopy_cnt);
  tty->print_cr(" _arraycopy_slowcase_cnt:         %d", _arraycopy_slowcase_cnt);
  tty->print_cr(" _arraycopy_checkcast_cnt:        %d", _arraycopy_checkcast_cnt);
  tty->print_cr(" _arraycopy_checkcast_attempt_cnt:%d", _arraycopy_checkcast_attempt_cnt);

  tty->print_cr(" _new_type_array_slowcase_cnt:    %d", _new_type_array_slowcase_cnt);
  tty->print_cr(" _new_object_array_slowcase_cnt:  %d", _new_object_array_slowcase_cnt);
  tty->print_cr(" _new_instance_slowcase_cnt:      %d", _new_instance_slowcase_cnt);
  tty->print_cr(" _new_multi_array_slowcase_cnt:   %d", _new_multi_array_slowcase_cnt);
  tty->print_cr(" _monitorenter_slowcase_cnt:      %d", _monitorenter_slowcase_cnt);
  tty->print_cr(" _monitorexit_slowcase_cnt:       %d", _monitorexit_slowcase_cnt);
  tty->print_cr(" _patch_code_slowcase_cnt:        %d", _patch_code_slowcase_cnt);

  tty->print_cr(" _throw_range_check_exception_count:            %d:", _throw_range_check_exception_count);
  tty->print_cr(" _throw_index_exception_count:                  %d:", _throw_index_exception_count);
  tty->print_cr(" _throw_div0_exception_count:                   %d:", _throw_div0_exception_count);
  tty->print_cr(" _throw_null_pointer_exception_count:           %d:", _throw_null_pointer_exception_count);
  tty->print_cr(" _throw_class_cast_exception_count:             %d:", _throw_class_cast_exception_count);
  tty->print_cr(" _throw_incompatible_class_change_error_count:  %d:", _throw_incompatible_class_change_error_count);
  tty->print_cr(" _throw_array_store_exception_count:            %d:", _throw_array_store_exception_count);
  tty->print_cr(" _throw_count:                                  %d:", _throw_count);

  SharedRuntime::print_ic_miss_histogram();
  tty->cr();
}
#endif // PRODUCT
