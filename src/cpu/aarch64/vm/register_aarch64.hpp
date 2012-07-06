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
  int   encoding_nocheck() const                         { return (intptr_t)this; }
  bool  is_valid() const                          { return 0 <= (intptr_t)this && (intptr_t)this < number_of_registers; }
  const char* name() const;

};

// The float registers of the AARCH64 architecture

CONSTANT_REGISTER_DECLARATION(FloatRegister, fnoreg , (-1));

CONSTANT_REGISTER_DECLARATION(FloatRegister, F0     , ( 0));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F1     , ( 1));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F2     , ( 2));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F3     , ( 3));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F4     , ( 4));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F5     , ( 5));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F6     , ( 6));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F7     , ( 7));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F8     , ( 8));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F9     , ( 9));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F10    , (10));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F11    , (11));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F12    , (12));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F13    , (13));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F14    , (14));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F15    , (15));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F16    , (16));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F17    , (17));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F18    , (18));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F19    , (19));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F20    , (20));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F21    , (21));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F22    , (22));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F23    , (23));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F24    , (24));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F25    , (25));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F26    , (26));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F27    , (27));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F28    , (28));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F29    , (29));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F30    , (30));
CONSTANT_REGISTER_DECLARATION(FloatRegister, F31    , (31));

#ifndef DONT_USE_REGISTER_DEFINES
#define fnoreg ((FloatRegister)(fnoreg_FloatRegisterEnumValue))
#define F0     ((FloatRegister)(    F0_FloatRegisterEnumValue))
#define F1     ((FloatRegister)(    F1_FloatRegisterEnumValue))
#define F2     ((FloatRegister)(    F2_FloatRegisterEnumValue))
#define F3     ((FloatRegister)(    F3_FloatRegisterEnumValue))
#define F4     ((FloatRegister)(    F4_FloatRegisterEnumValue))
#define F5     ((FloatRegister)(    F5_FloatRegisterEnumValue))
#define F6     ((FloatRegister)(    F6_FloatRegisterEnumValue))
#define F7     ((FloatRegister)(    F7_FloatRegisterEnumValue))
#define F8     ((FloatRegister)(    F8_FloatRegisterEnumValue))
#define F9     ((FloatRegister)(    F9_FloatRegisterEnumValue))
#define F10    ((FloatRegister)(   F10_FloatRegisterEnumValue))
#define F11    ((FloatRegister)(   F11_FloatRegisterEnumValue))
#define F12    ((FloatRegister)(   F12_FloatRegisterEnumValue))
#define F13    ((FloatRegister)(   F13_FloatRegisterEnumValue))
#define F14    ((FloatRegister)(   F14_FloatRegisterEnumValue))
#define F15    ((FloatRegister)(   F15_FloatRegisterEnumValue))
#define F16    ((FloatRegister)(   F16_FloatRegisterEnumValue))
#define F17    ((FloatRegister)(   F17_FloatRegisterEnumValue))
#define F18    ((FloatRegister)(   F18_FloatRegisterEnumValue))
#define F19    ((FloatRegister)(   F19_FloatRegisterEnumValue))
#define F20    ((FloatRegister)(   F20_FloatRegisterEnumValue))
#define F21    ((FloatRegister)(   F21_FloatRegisterEnumValue))
#define F22    ((FloatRegister)(   F22_FloatRegisterEnumValue))
#define F23    ((FloatRegister)(   F23_FloatRegisterEnumValue))
#define F24    ((FloatRegister)(   F24_FloatRegisterEnumValue))
#define F25    ((FloatRegister)(   F25_FloatRegisterEnumValue))
#define F26    ((FloatRegister)(   F26_FloatRegisterEnumValue))
#define F27    ((FloatRegister)(   F27_FloatRegisterEnumValue))
#define F28    ((FloatRegister)(   F28_FloatRegisterEnumValue))
#define F29    ((FloatRegister)(   F29_FloatRegisterEnumValue))
#define F30    ((FloatRegister)(   F30_FloatRegisterEnumValue))
#define F31    ((FloatRegister)(   F31_FloatRegisterEnumValue))
#endif // DONT_USE_REGISTER_DEFINES

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
                               FloatRegisterImpl::number_of_registers +
                               1 // flags
  };

};

#endif // CPU_AARCH64_VM_REGISTER_AARCH64_HPP
