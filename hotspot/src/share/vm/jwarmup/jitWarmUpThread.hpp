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

#ifndef SHARE_VM_JWARMUP_JITWARMUPTHREAD_HPP
#define SHARE_VM_JWARMUP_JITWARMUPTHREAD_HPP

#include "runtime/thread.hpp"

class JitWarmUpFlushThread : public NamedThread {
protected:
  JitWarmUpFlushThread(unsigned int sec);
  virtual ~JitWarmUpFlushThread();

public:
  virtual void run();

  bool         has_started() { return _has_started; }
  unsigned int sleep_sec()   { return _sleep_sec; }

  void         set_sleep_sec(unsigned int sec) { _sleep_sec = sec; }

  static void  spawn_wait_for_flush(unsigned int sec);

  static void  print_jwarmup_threads_on(outputStream* st);

private:
  volatile bool   _has_started;
  unsigned int    _sleep_sec;

  static JitWarmUpFlushThread* _jwp_thread;
};

#endif //SHARE_VM_JWARMUP_JITWARMUPTHREAD_HPP
