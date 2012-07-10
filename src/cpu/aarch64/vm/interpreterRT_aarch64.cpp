/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/universe.inline.hpp"
#include "oops/methodOop.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/signature.hpp"

#define __ _masm->

// Implementation of SignatureHandlerGenerator

Register InterpreterRuntime::SignatureHandlerGenerator::from() { Unimplemented(); return r0; }
Register InterpreterRuntime::SignatureHandlerGenerator::to()   { Unimplemented(); return r0; }
Register InterpreterRuntime::SignatureHandlerGenerator::temp() { Unimplemented(); return r0; }

void InterpreterRuntime::SignatureHandlerGenerator::pass_int() { Unimplemented(); }

void InterpreterRuntime::SignatureHandlerGenerator::pass_long() { Unimplemented(); }

void InterpreterRuntime::SignatureHandlerGenerator::pass_float() { Unimplemented(); }

void InterpreterRuntime::SignatureHandlerGenerator::pass_double() { Unimplemented(); }

void InterpreterRuntime::SignatureHandlerGenerator::pass_object() { Unimplemented(); }

void InterpreterRuntime::SignatureHandlerGenerator::generate(uint64_t fingerprint) { Unimplemented(); }


// Implementation of SignatureHandlerLibrary

void SignatureHandlerLibrary::pd_set_handler(address handler) { Unimplemented(); }


#ifdef _WIN64
class SlowSignatureHandler
  : public NativeSignatureIterator {
 private:
  address   _from;
  intptr_t* _to;
  intptr_t* _reg_args;
  intptr_t* _fp_identifiers;
  unsigned int _num_args;

  virtual void pass_int() { Unimplemented(); }

  virtual void pass_long() { Unimplemented(); }

  virtual void pass_object() { Unimplemented(); }

  virtual void pass_float() { Unimplemented(); }

  virtual void pass_double() { Unimplemented(); }

 public:
  SlowSignatureHandler(methodHandle method, address from, intptr_t* to)
    : NativeSignatureIterator(method) { Unimplemented(); }
};
#else
class SlowSignatureHandler
  : public NativeSignatureIterator {
 private:
  address   _from;
  intptr_t* _to;
  intptr_t* _int_args;
  intptr_t* _fp_args;
  intptr_t* _fp_identifiers;
  unsigned int _num_int_args;
  unsigned int _num_fp_args;

  virtual void pass_int() { Unimplemented(); }

  virtual void pass_long() { Unimplemented(); }

  virtual void pass_object() { Unimplemented(); }

  virtual void pass_float() { Unimplemented(); }

  virtual void pass_double() { Unimplemented(); }

 public:
  SlowSignatureHandler(methodHandle method, address from, intptr_t* to)
    : NativeSignatureIterator(method) { Unimplemented(); }
};
#endif


IRT_ENTRY(address,
          InterpreterRuntime::slow_signature_handler(JavaThread* thread,
                                                     methodOopDesc* method,
                                                     intptr_t* from,
                                                     intptr_t* to))
  Unimplemented();
  return 0;
IRT_END
