/*
 * Copyright (c) 2013, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "runtime/os.hpp"
#include "utilities/ticks.inline.hpp"

#ifdef X86
#include "rdtsc_x86.hpp"
#endif

#ifdef TARGET_OS_ARCH_linux_x86
# include "os_linux_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_sparc
# include "os_linux_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_zero
# include "os_linux_zero.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_x86
# include "os_solaris_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_solaris_sparc
# include "os_solaris_sparc.hpp"
#endif
#ifdef TARGET_OS_ARCH_windows_x86
# include "os_windows_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_arm
# include "os_linux_arm.hpp"
#endif
#ifdef TARGET_OS_ARCH_linux_ppc
# include "os_linux_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_aix_ppc
# include "os_aix_ppc.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_x86
# include "os_bsd_x86.hpp"
#endif
#ifdef TARGET_OS_ARCH_bsd_zero
# include "os_bsd_zero.hpp"
#endif

ElapsedCounterStamped::ElapsedCounterStamped() : ElapsedCounter(os::elapsed_counter()) {}

void ElapsedCounter::stamp() {
  _instant = now().value();
}

ElapsedCounter ElapsedCounter::now() {
  return ElapsedCounterStamped();
}

#ifdef X86
FastElapsedCounterStamped::FastElapsedCounterStamped() : FastElapsedCounter(Rdtsc::elapsed_counter()) {}
#else
FastElapsedCounterStamped::FastElapsedCounterStamped() : FastElapsedCounter(os::elapsed_counter()) {}
#endif

void FastElapsedCounter::stamp() {
  _instant = now().value();
}

FastElapsedCounter FastElapsedCounter::now() {
  return FastElapsedCounterStamped();
}

TraceElapsedInterval::TraceElapsedInterval(const TraceElapsedCounter& end, const TraceElapsedCounter& start) :
  _elapsed_interval(end.value() - start.value())
#ifdef X86
  , _ft_elapsed_interval(end.ft_value() - start.ft_value())
#endif
  {}

TraceElapsedInterval::TraceElapsedInterval(jlong interval) :
  _elapsed_interval(interval)
#ifdef X86
  , _ft_elapsed_interval(interval)
#endif
  {}

TraceElapsedInterval& TraceElapsedInterval::operator+=(const TraceElapsedInterval& rhs) {
  _elapsed_interval += rhs._elapsed_interval;
  X86_ONLY(_ft_elapsed_interval += rhs._ft_elapsed_interval;)
  return *this;
}

TraceElapsedInterval& TraceElapsedInterval::operator-=(const TraceElapsedInterval& rhs) {
  _elapsed_interval -= rhs._elapsed_interval;
  X86_ONLY(_ft_elapsed_interval -= rhs._ft_elapsed_interval;)
  return *this;
}

jlong TraceElapsedInterval::ft_value() const {
#ifdef X86
  return _ft_elapsed_interval.value();
#else
  return _elapsed_interval.value();
#endif
}

TraceElapsedCounter::TraceElapsedCounter(jlong stamp) :
  _elapsed(stamp)
#ifdef X86
  , _ft_elapsed(stamp)
#endif
  {}

TraceElapsedCounter TraceElapsedCounter::now() {
  TraceElapsedCounterStamped dec;
  return dec;
}

TraceElapsedCounter& TraceElapsedCounter::operator+=(const TraceElapsedInterval& rhs) {
  _elapsed += rhs.value();
  X86_ONLY(_ft_elapsed += rhs.ft_value();)
  return *this;
}

TraceElapsedCounter& TraceElapsedCounter::operator-=(const TraceElapsedInterval& rhs) {
  _elapsed -= rhs.value();
  X86_ONLY(_ft_elapsed -= rhs.ft_value();)
  return *this;
}

void TraceElapsedCounter::stamp() {
  _elapsed.stamp();
  X86_ONLY(_ft_elapsed.stamp();)
}

jlong TraceElapsedCounter::ft_value() const {
#ifdef X86
  return _ft_elapsed.value();
#else
  return _elapsed.value();
#endif
}

TraceElapsedCounterStamped::TraceElapsedCounterStamped() : TraceElapsedCounter() {
  _elapsed.stamp();
  X86_ONLY(_ft_elapsed.stamp());
}
