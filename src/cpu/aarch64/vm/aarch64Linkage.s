# 
# Copyright (c) 2012, Red Hat. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

# Routines used to enable x86 VM C++ code to invoke JIT-compiled ARM code
# -- either Java methods or generated stub -- and to allow JIT-compiled
# ARM code to invoke x86 VM C++ code
#
# the code for aarch64_stub_prolog below can be copied into the start
# of the ARM code buffer and patched with a link to the
# C++ routine which starts execution on the simulator. the ARM
# code can be generated immediately following the copied code.

	.text
        .globl setup_arm_sim, 
	.type  setup_arm_sim,@function
        .globl aarch64_stub_prolog
	.type  aarch64_stub_prolog,@function
        .p2align  4
aarch64_stub_prolog:
	// save all registers used to pass args
	push %r9
	push %r8
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	// save rax -- this stack slot will be rewritten with a
	// return value if needed
	push %rax
	// get next pc in rdi
	call get_pc
aarch64_stub_prolog_here:
	// add offset to start of ARM code
	leaq (aarch64_stub_prolog_end - aarch64_stub_prolog_here), %rsi
	add %rdi, %rsi
	// push start of arm code
	push %rsi
	// load address of sim setup routine 
	leaq (-16)(%rsi), %rdx
	// load call type code in arg reg 1
	mov (-8)(%rsi), %rsi
	// if we need a return value push a stack slot for it
	// load current stack pointer in arg reg 0
	mov %rsp, %rdi
	// call sim setup routine
	call *(%rdx)
	// pop start of arm code
	pop %rdi
	// pop rax -- either restores old value or installs return value
	pop %rax
	// pop 6 arg registers
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %r8
	pop %r9
	ret

        .p2align  4
get_pc:
	// get return pc in rdi and then push it back
	pop %rdi
	push %rdi
	ret
	.p2align 4
	// 64 bit word used to hold the sim setup routine
	.long 0
	.long 0
	// 64 bit int used to idenitfy called fn arg/return types
	.long 0
	.long 0
	// arm JIT code follows the stub
aarch64_stub_prolog_end:

	.p2align 4
	.long
	.globl aarch64_stub_prolog_size
	.type  aarch64_stub_prolog_size,@function
aarch64_stub_prolog_size:
	leaq  aarch64_stub_prolog_end - aarch64_stub_prolog, %rax
	ret
