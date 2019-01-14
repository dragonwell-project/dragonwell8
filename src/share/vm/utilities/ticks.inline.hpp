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

#ifndef SHARE_VM_UTILITIES_TICKS_INLINE_HPP
#define SHARE_VM_UTILITIES_TICKS_INLINE_HPP

#include "utilities/ticks.hpp"

template <typename InstantType>
inline TimeInterval<InstantType>& TimeInterval<InstantType>::operator+=(const TimeInterval<InstantType>& rhs) {
  _interval += rhs._interval;
  return *this;
}

template <typename InstantType>
inline TimeInterval<InstantType>& TimeInterval<InstantType>::operator-=(const TimeInterval<InstantType>& rhs) {
  _interval -= rhs._interval;
  return *this;
}

template <typename InstantType>
inline bool operator==(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return lhs.value() == rhs.value();
}

template <typename InstantType>
inline bool operator!=(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename InstantType>
inline bool operator<(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return lhs.value() < rhs.value();
}

template <typename InstantType>
inline bool operator>(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return operator<(rhs, lhs);
}

template <typename InstantType>
inline bool operator<=(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return !operator>(lhs, rhs);
}

template <typename InstantType>
inline bool operator>=(const TimeInterval<InstantType>& lhs, const TimeInterval<InstantType>& rhs) {
  return !operator<(lhs, rhs);
}

template <typename InstantType>
inline TimeInterval<InstantType> operator-(const InstantType& end, const InstantType& start) {
  return TimeInterval<InstantType>(end, start);
}

inline TimeInstant& TimeInstant::operator+=(const TimeInterval<TimeInstant>& rhs) {
  _instant += rhs.value();
  return *this;
}

inline TimeInstant& TimeInstant::operator-=(const TimeInterval<TimeInstant>& rhs) {
  _instant -= rhs.value();
  return *this;
}

inline bool operator==(const TimeInstant& lhs, const TimeInstant& rhs) {
  return lhs.value() == rhs.value();
}

inline bool operator!=(const TimeInstant& lhs, const TimeInstant& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const TimeInstant& lhs, const TimeInstant& rhs) {
  return lhs.value() < rhs.value();
}

inline bool operator>(const TimeInstant& lhs, const TimeInstant& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const TimeInstant& lhs, const TimeInstant& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const TimeInstant& lhs, const TimeInstant& rhs) {
  return !operator<(lhs, rhs);
}

inline TimeInstant operator+(const TimeInstant& lhs, const TimeInterval<TimeInstant>& span) {
  TimeInstant t(lhs);
  t += span;
  return t;
}

inline TimeInstant operator-(const TimeInstant& lhs, const TimeInterval<TimeInstant>& span) {
  TimeInstant t(lhs);
  t -= span;
  return t;
}

// TraceElapsedInterval

inline bool operator==(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return lhs.value() == rhs.value();
}

inline bool operator!=(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return lhs.value() < rhs.value();
}

inline bool operator>(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const TraceElapsedInterval& lhs, const TraceElapsedInterval& rhs) {
  return !operator<(lhs, rhs);
}

inline TraceElapsedInterval operator-(const TraceElapsedCounter& end, const TraceElapsedCounter& start) {
  return TraceElapsedInterval(end, start);
}

// TraceElapsedCounter

inline TraceElapsedCounter operator+(const TraceElapsedCounter& lhs, const TraceElapsedInterval& span) {
  TraceElapsedCounter t(lhs);
  t += span;
  return t;
}

inline TraceElapsedCounter operator-(const TraceElapsedCounter& lhs, const TraceElapsedInterval& span) {
  TraceElapsedCounter t(lhs);
  t -= span;
  return t;
}

inline bool operator==(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return lhs.value() == rhs.value();
}

inline bool operator!=(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return lhs.value() < rhs.value();
}

inline bool operator>(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const TraceElapsedCounter& lhs, const TraceElapsedCounter& rhs) {
  return !operator<(lhs, rhs);
}

#endif // SHARE_VM_UTILITIES_TICKS_INLINE_HPP
