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
 *
 */

#include "gc_implementation/g1/g1AllocationContext.hpp"
#include "runtime/thread.hpp"

// 0 will be returned if in ROOT tenant or memory isolation not enabled
AllocationContext_t AllocationContext::_root_context;

AllocationContext_t AllocationContext::current() {
  return Thread::current()->allocation_context();
}

AllocationContext_t AllocationContext::system() {
  return _root_context;
}

AllocationContextMark::AllocationContextMark(AllocationContext_t ctxt)
        : _saved_context(Thread::current()->allocation_context()) {
  Thread* thrd = Thread::current();
  thrd->set_allocation_context(ctxt);
}

AllocationContextMark::~AllocationContextMark() {
  Thread* thrd = Thread::current();
  thrd->set_allocation_context(_saved_context);
}
