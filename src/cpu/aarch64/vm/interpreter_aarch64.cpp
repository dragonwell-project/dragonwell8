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
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "oops/arrayOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/timer.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/debug.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#endif

#define __ _masm->


address AbstractInterpreterGenerator::generate_slow_signature_handler() {
  address entry = __ pc();

  __ andr(esp, esp, -16);
  __ mov(c_rarg3, esp);
  // rmethod
  // rlocals
  // c_rarg3: first stack arg - wordSize

  // adjust sp
  __ sub(sp, c_rarg3, 18 * wordSize);
  __ str(lr, Address(__ pre(sp, -2 * wordSize)));
  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::slow_signature_handler),
             rmethod, rlocals, c_rarg3);

  // r0: result handler

  // Stack layout:
  // rsp: return address           <- sp
  //      1 garbage
  //      8 integer args (if static first is unused)
  //      1 float/double identifiers
  //      8 double args
  //        stack args              <- esp
  //        garbage
  //        expression stack bottom
  //        bcp (NULL)
  //        ...

  // Restore LR
  __ ldr(lr, Address(__ post(sp, 2 * wordSize)));

  // Do FP first so we can use c_rarg3 as temp
  __ ldrw(c_rarg3, Address(sp, 9 * wordSize)); // float/double identifiers

  for (int i = 0; i < Argument::n_float_register_parameters_c; i++) {
    const FloatRegister r = as_FloatRegister(i);

    Label d, done;

    __ tbnz(c_rarg3, i, d);
    __ ldrs(r, Address(sp, (10 + i) * wordSize));
    __ b(done);
    __ bind(d);
    __ ldrd(r, Address(sp, (10 + i) * wordSize));
    __ bind(done);
  }

  // c_rarg0 contains the result from the call of
  // InterpreterRuntime::slow_signature_handler so we don't touch it
  // here.  It will be loaded with the JNIEnv* later.
  __ ldr(c_rarg1, Address(sp, 1 * wordSize));
  for (int i = c_rarg2->encoding(); i <= c_rarg7->encoding(); i += 2) {
    Register rm = as_Register(i), rn = as_Register(i+1);
    __ ldp(rm, rn, Address(sp, i * wordSize));
  }

  __ add(sp, sp, 18 * wordSize);
  __ ret(lr);

  return entry;
}


//
// Various method entries
//

address InterpreterGenerator::generate_math_entry(AbstractInterpreter::MethodKind kind) {
  return NULL;
}


// Abstract method entry
// Attempt to execute abstract method. Throw exception
address InterpreterGenerator::generate_abstract_entry(void) {
  address entry_point = __ pc();

  __ call_Unimplemented();

  return entry_point;
}

// Method handle invoker
// Dispatch a method of the form java.lang.invoke.MethodHandles::invoke(...)
address InterpreterGenerator::generate_method_handle_entry(void) {
  address entry_point = __ pc();

  __ call_Unimplemented();

  return entry_point;
}

// Empty method, generate a very fast return.

address InterpreterGenerator::generate_empty_entry(void) {
  return NULL;
}

void Deoptimization::unwind_callee_save_values(frame* f, vframeArray* vframe_array) {

  // This code is sort of the equivalent of C2IAdapter::setup_stack_frame back in
  // the days we had adapter frames. When we deoptimize a situation where a
  // compiled caller calls a compiled caller will have registers it expects
  // to survive the call to the callee. If we deoptimize the callee the only
  // way we can restore these registers is to have the oldest interpreter
  // frame that we create restore these values. That is what this routine
  // will accomplish.

  // At the moment we have modified c2 to not have any callee save registers
  // so this problem does not exist and this routine is just a place holder.

  assert(f->is_interpreted_frame(), "must be interpreted");
}
