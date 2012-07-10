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
#include "memory/resourceArea.hpp"
#include "oops/markOop.hpp"
#include "oops/methodOop.hpp"
#include "oops/oop.inline.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/monitorChunk.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/stubRoutines.hpp"
#include "vmreg_aarch64.inline.hpp"
#ifdef COMPILER1
#include "c1/c1_Runtime1.hpp"
#include "runtime/vframeArray.hpp"
#endif

#ifdef ASSERT
void RegisterMap::check_location_valid() {
}
#endif


// Profiling/safepoint support

bool frame::safe_for_sender(JavaThread *thread) { Unimplemented(); return false; }


void frame::patch_pc(Thread* thread, address pc) { Unimplemented(); }

bool frame::is_interpreted_frame() const  { Unimplemented(); return false; }

int frame::frame_size(RegisterMap* map) const { Unimplemented(); return 0; }

intptr_t* frame::entry_frame_argument_at(int offset) const { Unimplemented(); return 0; }

// sender_sp
#ifdef CC_INTERP
intptr_t* frame::interpreter_frame_sender_sp() const { Unimplemented(); return 0; }

// monitor elements

BasicObjectLock* frame::interpreter_frame_monitor_begin() const { Unimplemented(); return 0; }

BasicObjectLock* frame::interpreter_frame_monitor_end() const { Unimplemented(); return 0; }

#else // CC_INTERP

intptr_t* frame::interpreter_frame_sender_sp() const { Unimplemented(); return 0; }

void frame::set_interpreter_frame_sender_sp(intptr_t* sender_sp) { Unimplemented(); }


// monitor elements

BasicObjectLock* frame::interpreter_frame_monitor_begin() const { Unimplemented(); return 0; }

BasicObjectLock* frame::interpreter_frame_monitor_end() const { Unimplemented(); return 0; }

void frame::interpreter_frame_set_monitor_end(BasicObjectLock* value) { Unimplemented(); }

// Used by template based interpreter deoptimization
void frame::interpreter_frame_set_last_sp(intptr_t* sp) { Unimplemented(); }
#endif // CC_INTERP

frame frame::sender_for_entry_frame(RegisterMap* map) const {Unimplemented(); frame fr; return fr; }

//------------------------------------------------------------------------------
// frame::verify_deopt_original_pc
//
// Verifies the calculated original PC of a deoptimization PC for the
// given unextended SP.  The unextended SP might also be the saved SP
// for MethodHandle call sites.
#if ASSERT
void frame::verify_deopt_original_pc(nmethod* nm, intptr_t* unextended_sp, bool is_method_handle_return) { Unimplemented(); }
#endif

//------------------------------------------------------------------------------
// frame::adjust_unextended_sp
void frame::adjust_unextended_sp() { Unimplemented(); }

//------------------------------------------------------------------------------
// frame::update_map_with_saved_link
void frame::update_map_with_saved_link(RegisterMap* map, intptr_t** link_addr) { Unimplemented(); }


//------------------------------------------------------------------------------
// frame::sender_for_interpreter_frame
frame frame::sender_for_interpreter_frame(RegisterMap* map) const { Unimplemented(); frame fr; return fr; }


//------------------------------------------------------------------------------
// frame::sender_for_compiled_frame
frame frame::sender_for_compiled_frame(RegisterMap* map) const { Unimplemented(); frame fr; return fr; }


//------------------------------------------------------------------------------
// frame::sender
frame frame::sender(RegisterMap* map) const { Unimplemented(); frame fr; return fr; }


bool frame::interpreter_frame_equals_unpacked_fp(intptr_t* fp) { Unimplemented(); return false; }

void frame::pd_gc_epilog() { Unimplemented(); }

bool frame::is_interpreted_frame_valid(JavaThread* thread) const { Unimplemented(); return false; }

BasicType frame::interpreter_frame_result(oop* oop_result, jvalue* value_result) { Unimplemented(); return T_VOID; }


intptr_t* frame::interpreter_frame_tos_at(jint offset) const { Unimplemented(); return 0; }

#ifndef PRODUCT

#define DESCRIBE_FP_OFFSET(name) \
  values.describe(frame_no, fp() + frame::name##_offset, #name)

void frame::describe_pd(FrameValues& values, int frame_no) { Unimplemented(); }
#endif

intptr_t *frame::initial_deoptimization_info() { Unimplemented(); return 0; }

intptr_t* frame::real_fp() const { Unimplemented(); return 0; }
