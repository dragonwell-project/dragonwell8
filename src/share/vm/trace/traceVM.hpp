/*
 * Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_TRACE_TRACEVM_HPP
#define SHARE_VM_TRACE_TRACEVM_HPP

#include "utilities/macros.hpp"
#if INCLUDE_TRACE
#include "jvmtifiles/jvmti.h"

/** The possible java.lang.Thread states a thread can have "in java". */

#define MAKE_JAVATHREAD_STATES(state)  \
   state(STATE_NEW, (u8)0) /* Only implicitely defined in JVMTI */ \
   state(STATE_RUNNABLE, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_RUNNABLE)) \
   state(STATE_SLEEPING, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_SLEEPING)) \
   state(STATE_IN_OBJECT_WAIT, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_WAITING_INDEFINITELY | JVMTI_THREAD_STATE_IN_OBJECT_WAIT)) \
   state(STATE_IN_OBJECT_WAIT_TIMED, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_IN_OBJECT_WAIT)) \
   state(STATE_PARKED, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_WAITING_INDEFINITELY | JVMTI_THREAD_STATE_PARKED)) \
   state(STATE_PARKED_TIMED, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_WAITING | JVMTI_THREAD_STATE_WAITING_WITH_TIMEOUT | JVMTI_THREAD_STATE_PARKED)) \
   state(STATE_BLOCKED_ON_MONITOR_ENTER, (u8)(JVMTI_THREAD_STATE_ALIVE | JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER)) \
   state(STATE_TERMINATED, (u8)(JVMTI_THREAD_STATE_TERMINATED))

typedef enum {
#define _ts_enum(n, v)  THREAD_##n = v,
   MAKE_JAVATHREAD_STATES(_ts_enum)
   LAST_JavaLangThreadState
} JavaLangThreadState;

#endif // INCLUDE_TRACE
#endif // SHARE_VM_TRACE_TRACEVM_HPP
