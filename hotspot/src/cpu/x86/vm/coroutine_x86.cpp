/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "precompiled.hpp"
#include "prims/privilegedStack.hpp"
#include "runtime/coroutine.hpp"
#include "runtime/globals.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/objectMonitor.inline.hpp"
#include "services/threadService.hpp"

#include "coroutine_x86.hpp"
#include "vmreg_x86.hpp"

void Coroutine::set_coroutine_base(intptr_t **&base, JavaThread* thread, jobject obj, Coroutine *coro, oop coroutineObj, address coroutine_start) {
  // Make the 16 bytes(original is 8*5=40 bytes) alignation which is required by some instructions like movaps otherwise we will incur a crash.
  *(--base) = NULL;
  *(--base) = NULL;
  *(--base) = (intptr_t*)obj;
  *(--base) = (intptr_t*)coro;
  *(--base) = NULL;
  *(--base) = (intptr_t*)coroutine_start;
  *(--base) = NULL;
}

Register CoroutineStack::get_fp_reg() {
  return rbp;
}
