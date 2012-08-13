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

	.data
        .globl setup_arm_sim, 
	.type  setup_arm_sim,@function
        .globl aarch64_stub_prolog
        .p2align  4
aarch64_stub_prolog:
4:	lea 1f(%rip), %r11 // entry point
	mov 2f(%rip), %r10
	jmp *%r10
2:	.quad aarch64_prolog
	.p2align 4
1:
	// 64 bit word used to hold the sim setup routine
	.set entry_offset, . - 1b
	.quad 0
	// 64 bit int used to idenitfy called fn arg/return types
	.set calltype_offset, . - 1b
	.quad 0
	// arm JIT code follows the stub
	.set arm_code_offset, . - 1b
	.size aarch64_stub_prolog, .-aarch64_stub_prolog
aarch64_stub_prolog_end:
	
	.text
aarch64_prolog:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
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

	// push start of arm code
	lea (arm_code_offset)(%r11), %rsi
	push %rsi
	// load address of sim setup routine
	lea (entry_offset)(%r11), %rdx
	// load call type code in arg reg 1
	mov (calltype_offset)(%r11), %rsi
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
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc


        .p2align  4
get_pc:
	// get return pc in rdi and then push it back
	pop %rdi
	push %rdi
	ret

	.p2align 4
	.long
	.globl aarch64_stub_prolog_size
	.type  aarch64_stub_prolog_size,@function
aarch64_stub_prolog_size:
	leaq  aarch64_stub_prolog_end - aarch64_stub_prolog, %rax
	ret
