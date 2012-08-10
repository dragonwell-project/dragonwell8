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
  // Unimplemented();
  return 0;
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

void Deoptimization::unwind_callee_save_values(frame* f, vframeArray* vframe_array) { Unimplemented(); }
