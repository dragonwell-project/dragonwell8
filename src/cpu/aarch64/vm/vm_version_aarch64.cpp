/*
 * Copyright (c) 2013, Red Hat Inc.
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates.
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
#include "asm/macroAssembler.inline.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/java.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "vm_version_aarch64.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif

int VM_Version::_cpu;
int VM_Version::_model;
int VM_Version::_stepping;
int VM_Version::_cpuFeatures;
const char*           VM_Version::_features_str = "";

static BufferBlob* stub_blob;
static const int stub_size = 550;

extern "C" {
  typedef void (*getPsrInfo_stub_t)(void*);
}
static getPsrInfo_stub_t getPsrInfo_stub = NULL;


class VM_Version_StubGenerator: public StubCodeGenerator {
 public:

  VM_Version_StubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

  address generate_getPsrInfo() {
    StubCodeMark mark(this, "VM_Version", "getPsrInfo_stub");
#   define __ _masm->
    address start = __ pc();

#ifdef BUILTIN_SIM
    __ c_stub_prolog(1, 0, MacroAssembler::ret_type_void);
#endif

    // void getPsrInfo(VM_Version::CpuidInfo* cpuid_info);

    address entry = __ pc();

    // TODO : redefine fields in CpuidInfo and generate
    // code to fill them in

    __ ret(lr);

#   undef __


  }
};


void VM_Version::get_processor_features() {
  _supports_cx8 = true;
  _supports_atomic_getset4 = true;
  _supports_atomic_getadd4 = true;
  _supports_atomic_getset8 = true;
  _supports_atomic_getadd8 = true;

  FLAG_SET_DEFAULT(AllocatePrefetchDistance, 256);
}

void VM_Version::initialize() {
  ResourceMark rm;

  stub_blob = BufferBlob::create("getPsrInfo_stub", stub_size);
  if (stub_blob == NULL) {
    vm_exit_during_initialization("Unable to allocate getPsrInfo_stub");
  }

  CodeBuffer c(stub_blob);
  VM_Version_StubGenerator g(&c);
  getPsrInfo_stub = CAST_TO_FN_PTR(getPsrInfo_stub_t,
                                   g.generate_getPsrInfo());

  get_processor_features();
}
