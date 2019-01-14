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

#ifndef SHARE_VM_UTILITIES_TICKS_HPP
#define SHARE_VM_UTILITIES_TICKS_HPP

#include "jni.h"
#include "memory/allocation.hpp"
#include "utilities/macros.hpp"

template <typename InstantType>
class TimeInterval {
  template <typename, typename>
  friend TimeInterval operator-(const InstantType& end, const InstantType& start);
 private:
  jlong _interval;
  TimeInterval(const InstantType& end, const InstantType& start) : _interval(end - start) {}
 public:
  TimeInterval(jlong interval = 0) : _interval(interval) {}
  TimeInterval& operator+=(const TimeInterval& rhs);
  TimeInterval& operator-=(const TimeInterval& rhs);
  jlong value() const { return _interval; }
};

class TimeInstant {
 protected:
  jlong _instant;
 public:
  TimeInstant(jlong stamp = 0) : _instant(stamp) {}
  TimeInstant& operator+=(const TimeInterval<TimeInstant>& rhs);
  TimeInstant& operator-=(const TimeInterval<TimeInstant>& rhs);
  jlong value() const { return _instant; }
};

class ElapsedCounter : public TimeInstant {
 public:
  ElapsedCounter(jlong stamp = 0) : TimeInstant(stamp) {}
  static ElapsedCounter now();
  void stamp();
};

class ElapsedCounterStamped : public ElapsedCounter {
 public:
  ElapsedCounterStamped();
};

class FastElapsedCounter : public TimeInstant {
 public:
  FastElapsedCounter(jlong stamp = 0) : TimeInstant(stamp) {}
  static FastElapsedCounter now();
  void stamp();
};

class FastElapsedCounterStamped : public FastElapsedCounter {
 public:
  FastElapsedCounterStamped();
};

typedef TimeInterval<ElapsedCounter> ElapsedCounterInterval;
typedef TimeInterval<FastElapsedCounter> FastElapsedCounterInterval;

class TraceElapsedCounter;

class TraceElapsedInterval {
  friend TraceElapsedInterval operator-(const TraceElapsedCounter& end, const TraceElapsedCounter& start);
 private:
  ElapsedCounterInterval _elapsed_interval;
  FastElapsedCounterInterval _ft_elapsed_interval;
  TraceElapsedInterval(const TraceElapsedCounter& end, const TraceElapsedCounter& start);
 public:
  TraceElapsedInterval(jlong interval = 0);
  TraceElapsedInterval& operator+=(const TraceElapsedInterval& rhs);
  TraceElapsedInterval& operator-=(const TraceElapsedInterval& rhs);
  jlong value() const { return _elapsed_interval.value(); }
  jlong ft_value() const;
};

class TraceElapsedCounter {
 protected:
  ElapsedCounter _elapsed;
  X86_ONLY(FastElapsedCounter _ft_elapsed;)
 public:
  TraceElapsedCounter(jlong stamp = 0);
  TraceElapsedCounter& operator+=(const TraceElapsedInterval& rhs);
  TraceElapsedCounter& operator-=(const TraceElapsedInterval& rhs);

  static TraceElapsedCounter now();
  void stamp();
  jlong value() const { return _elapsed.value(); }
  jlong ft_value() const;
};

class TraceElapsedCounterStamped : public TraceElapsedCounter {
 public:
  TraceElapsedCounterStamped();
};

#if INCLUDE_TRACE
typedef TraceElapsedCounter  Ticks;
typedef TraceElapsedInterval Tickspan;
#else
typedef ElapsedCounter       Ticks;
typedef TicksInterval<Ticks> Tickspan;
#endif

#endif // SHARE_VM_UTILITIES_TICKS_HPP
