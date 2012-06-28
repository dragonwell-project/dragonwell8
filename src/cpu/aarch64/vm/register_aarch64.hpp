/*
 * Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_AARCH64_VM_REGISTER_AARCH64_HPP
#define CPU_AARCH64_VM_REGISTER_AARCH64_HPP

#include "asm/register.hpp"
#include "vm_version_aarch64.hpp"

class VMRegImpl;
typedef VMRegImpl* VMReg;

// Use Register as shortcut
class RegisterImpl;
typedef RegisterImpl* Register;

inline Register as_Register(int encoding) {
  return (Register)(intptr_t) encoding;
}

class RegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers      = 32,
    number_of_byte_registers = 32
  };

  // derived registers, offsets, and addresses
  Register successor() const                          { return as_Register(encoding() + 1); }

  // construction
  inline friend Register as_Register(int encoding);

  VMReg as_VMReg();

  // accessors
  int   encoding() const                         { assert(is_valid(), "invalid register"); return (intptr_t)this; }
  bool  is_valid() const                         { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  bool  has_byte_register() const                { return 0 <= (intptr_t)this && (intptr_t)this < number_of_byte_registers; }
  const char* name() const;
  int   encoding_nocheck() const                         { return (intptr_t)this; }
  
};

// The integer registers of the aarch64 architecture

CONSTANT_REGISTER_DECLARATION(Register, noreg, (-1));


extern const Register r1; // enum { r1_RegisterEnumValue = ((1)) };

CONSTANT_REGISTER_DECLARATION(Register, r0,    (0));
CONSTANT_REGISTER_DECLARATION(Register, r1,    (1));
CONSTANT_REGISTER_DECLARATION(Register, r2,    (2));
CONSTANT_REGISTER_DECLARATION(Register, r3,    (3));
CONSTANT_REGISTER_DECLARATION(Register, r4,    (4));
CONSTANT_REGISTER_DECLARATION(Register, r5,    (5));
CONSTANT_REGISTER_DECLARATION(Register, r6,    (6));
CONSTANT_REGISTER_DECLARATION(Register, r7,    (7));
CONSTANT_REGISTER_DECLARATION(Register, r8,    (8));
CONSTANT_REGISTER_DECLARATION(Register, r9,    (9));
CONSTANT_REGISTER_DECLARATION(Register, r10,  (10));
CONSTANT_REGISTER_DECLARATION(Register, r11,  (11));
CONSTANT_REGISTER_DECLARATION(Register, r12,  (12));
CONSTANT_REGISTER_DECLARATION(Register, r13,  (13));
CONSTANT_REGISTER_DECLARATION(Register, r14,  (14));
CONSTANT_REGISTER_DECLARATION(Register, r15,  (15));
CONSTANT_REGISTER_DECLARATION(Register, r16,  (16));
CONSTANT_REGISTER_DECLARATION(Register, r17,  (17));
CONSTANT_REGISTER_DECLARATION(Register, r18,  (18));
CONSTANT_REGISTER_DECLARATION(Register, r19,  (19));
CONSTANT_REGISTER_DECLARATION(Register, r20,  (20));
CONSTANT_REGISTER_DECLARATION(Register, r21,  (21));
CONSTANT_REGISTER_DECLARATION(Register, r22,  (22));
CONSTANT_REGISTER_DECLARATION(Register, r23,  (23));
CONSTANT_REGISTER_DECLARATION(Register, r24,  (24));
CONSTANT_REGISTER_DECLARATION(Register, r25,  (25));
CONSTANT_REGISTER_DECLARATION(Register, r26,  (26));
CONSTANT_REGISTER_DECLARATION(Register, r27,  (27));
CONSTANT_REGISTER_DECLARATION(Register, r28,  (28));
CONSTANT_REGISTER_DECLARATION(Register, r29,  (29));
CONSTANT_REGISTER_DECLARATION(Register, r30,  (30));
CONSTANT_REGISTER_DECLARATION(Register, sp,  (31));

// Use FloatRegister as shortcut
class FloatRegisterImpl;
typedef FloatRegisterImpl* FloatRegister;

inline FloatRegister as_FloatRegister(int encoding) {
  return (FloatRegister)(intptr_t) encoding;
}

// The implementation of floating point registers for the architecture
class FloatRegisterImpl: public AbstractRegisterImpl {
 public:
  enum {
    number_of_registers = 32
  };

  // construction
  inline friend FloatRegister as_FloatRegister(int encoding);

  VMReg as_VMReg();

  // derived registers, offsets, and addresses
  FloatRegister successor() const                          { return as_FloatRegister(encoding() + 1); }

  // accessors
  int   encoding() const                          { assert(is_valid(), "invalid register"); return (intptr_t)this; }
  bool  is_valid() const                          { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  const char* name() const;

};

// Need to know the total number of registers of all sorts for SharedInfo.
// Define a class that exports it.
class ConcreteRegisterImpl : public AbstractRegisterImpl {
 public:
  enum {
  // A big enough number for C2: all the registers plus flags
  // This number must be large enough to cover REG_COUNT (defined by c2) registers.
  // There is no requirement that any ordering here matches any ordering c2 gives
  // it's optoregs.

    number_of_registers =      RegisterImpl::number_of_registers +
                           1 // eflags
  };

};

#endif // CPU_AARCH64_VM_REGISTER_AARCH64_HPP
