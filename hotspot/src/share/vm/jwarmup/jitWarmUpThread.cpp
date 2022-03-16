/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

#include "code/codeCache.hpp"
#include "jwarmup/jitWarmUp.hpp"
#include "jwarmup/jitWarmUpThread.hpp"
#include "runtime/java.hpp"
#include "runtime/mutex.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/orderAccess.hpp"

JitWarmUpFlushThread* JitWarmUpFlushThread::_jwp_thread = NULL;

JitWarmUpFlushThread::JitWarmUpFlushThread(unsigned int sec) : NamedThread() {
  set_name("JitWarmUp Flush Thread");
  set_sleep_sec(sec);
  if (os::create_thread(this, os::vm_thread)) {
    os::set_priority(this, MaxPriority);
  } else {
    tty->print_cr("[JitWarmUp] ERROR : failed to create JitWarmUpFlushThread");
    vm_exit(-1);
  }
}

JitWarmUpFlushThread::~JitWarmUpFlushThread() {
  // do nothing
}

void JitWarmUpFlushThread::run() {
  assert(_jwp_thread == this, "sanity check");
  this->record_stack_base_and_size();
  this->_has_started = true;
  os::sleep(this, 1000 * sleep_sec(), false);
  JitWarmUp::instance()->flush_logfile();
  {
    MutexLockerEx mu(JitWarmUpPrint_lock);
    _jwp_thread = NULL;
  }
}

void JitWarmUpFlushThread::spawn_wait_for_flush(unsigned int sec) {
  JitWarmUpFlushThread* t = new JitWarmUpFlushThread(sec);
  _jwp_thread = t;
  Thread::start(t);
}

void JitWarmUpFlushThread::print_jwarmup_threads_on(outputStream* st) {
  MutexLockerEx mu(JitWarmUpPrint_lock);
  if (_jwp_thread == NULL || !_jwp_thread->has_started()) {
    return;
  }
  st->print("\"%s\" ", _jwp_thread->name());
  _jwp_thread->print_on(st);
  st->cr();
}
