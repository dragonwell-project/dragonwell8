/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef CPU_X86_VM_FRAME_X86_INLINE_HPP
#define CPU_X86_VM_FRAME_X86_INLINE_HPP

// Inline functions for Intel frames:

// Constructors:

inline frame::frame() {
  _pc = NULL;
  _sp = NULL;
  _unextended_sp = NULL;
  _fp = NULL;
  _cb = NULL;
  _deopt_state = unknown;
}

inline frame::frame(intptr_t* sp, intptr_t* fp, address pc) { Unimplemented(); }

inline frame::frame(intptr_t* sp, intptr_t* unextended_sp, intptr_t* fp, address pc) { Unimplemented(); }

inline frame::frame(intptr_t* sp, intptr_t* fp) { Unimplemented(); }

// Accessors

inline bool frame::equal(frame other) const { Unimplemented(); return false; }

// Return unique id for this frame. The id must have a value where we can distinguish
// identity and younger/older relationship. NULL represents an invalid (incomparable)
// frame.
inline intptr_t* frame::id(void) const { Unimplemented(); return 0; }

// Relationals on frames based
// Return true if the frame is younger (more recent activation) than the frame represented by id
inline bool frame::is_younger(intptr_t* id) const { Unimplemented(); return false; }

// Return true if the frame is older (less recent activation) than the frame represented by id
inline bool frame::is_older(intptr_t* id) const   { Unimplemented(); return false; }



inline intptr_t* frame::link() const              { Unimplemented(); return 0; }
inline void      frame::set_link(intptr_t* addr)  { Unimplemented(); }


inline intptr_t* frame::unextended_sp() const     { Unimplemented(); return 0; }

// Return address:

inline address* frame::sender_pc_addr()      const { Unimplemented(); return 0; }
inline address  frame::sender_pc()           const { Unimplemented(); return 0; }

// return address of param, zero origin index.
inline address* frame::native_param_addr(int idx) const { Unimplemented(); return 0; }

#ifdef CC_INTERP

inline interpreterState frame::get_interpreterState() const { Unimplemented(); return 0; }

inline intptr_t*    frame::sender_sp()        const { Unimplemented(); return 0; }

inline intptr_t** frame::interpreter_frame_locals_addr() const { Unimplemented(); return 0; }

inline intptr_t* frame::interpreter_frame_bcx_addr() const { Unimplemented(); return 0; }


// Constant pool cache

inline constantPoolCacheOop* frame::interpreter_frame_cache_addr() const { Unimplemented(); return 0; }

// Method

inline methodOop* frame::interpreter_frame_method_addr() const { Unimplemented(); return 0; }

inline intptr_t* frame::interpreter_frame_mdx_addr() const { Unimplemented(); return 0; }

// top of expression stack
inline intptr_t* frame::interpreter_frame_tos_address() const { Unimplemented(); return 0; }

#else /* asm interpreter */
inline intptr_t*    frame::sender_sp()        const { Unimplemented(); return 0; }

inline intptr_t** frame::interpreter_frame_locals_addr() const { Unimplemented(); return 0; }

inline intptr_t* frame::interpreter_frame_last_sp() const { Unimplemented(); return 0; }

inline intptr_t* frame::interpreter_frame_bcx_addr() const { Unimplemented(); return 0; }


inline intptr_t* frame::interpreter_frame_mdx_addr() const { Unimplemented(); return 0; }



// Constant pool cache

inline constantPoolCacheOop* frame::interpreter_frame_cache_addr() const { Unimplemented(); return 0; }

// Method

inline methodOop* frame::interpreter_frame_method_addr() const { Unimplemented(); return 0; }

// top of expression stack
inline intptr_t* frame::interpreter_frame_tos_address() const { Unimplemented(); return 0; }

#endif /* CC_INTERP */

inline int frame::pd_oop_map_offset_adjustment() const { Unimplemented(); return 0; }

inline int frame::interpreter_frame_monitor_size() { Unimplemented(); return 0; }


// expression stack
// (the max_stack arguments are used by the GC; see class FrameClosure)

inline intptr_t* frame::interpreter_frame_expression_stack() const { Unimplemented(); return 0; }


inline jint frame::interpreter_frame_expression_stack_direction() { Unimplemented(); return -1; }


// Entry frames

inline JavaCallWrapper* frame::entry_frame_call_wrapper() const { Unimplemented(); return 0; }


// Compiled frames

inline int frame::local_offset_for_compiler(int local_index, int nof_args, int max_nof_locals, int max_nof_monitors) { Unimplemented(); return 0; }

inline int frame::monitor_offset_for_compiler(int local_index, int nof_args, int max_nof_locals, int max_nof_monitors) { Unimplemented(); return 0; }

inline int frame::min_local_offset_for_compiler(int nof_args, int max_nof_locals, int max_nof_monitors) { Unimplemented(); return 0; }


inline bool frame::volatile_across_calls(Register reg) { Unimplemented(); return false; }



inline oop frame::saved_oop_result(RegisterMap* map) const { Unimplemented(); oop o(0); return o; }

inline void frame::set_saved_oop_result(RegisterMap* map, oop obj) { Unimplemented(); }

#endif // CPU_X86_VM_FRAME_X86_INLINE_HPP
