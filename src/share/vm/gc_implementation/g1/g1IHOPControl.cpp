/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/g1IHOPControl.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "jfr/utilities/jfrLog.hpp"

G1IHOPControl::G1IHOPControl(double initial_ihop_percent) :
  _initial_ihop_percent(initial_ihop_percent),
  _target_occupancy(0),
  _last_allocated_bytes(0),
  _last_allocation_time_s(0.0)
{
  assert(_initial_ihop_percent >= 0.0 && _initial_ihop_percent <= 100.0, "Initial IHOP value must be between 0 and 100.");
}

void G1IHOPControl::update_target_occupancy(size_t new_target_occupancy) {
  log_debug(gc, ihop)("Target occupancy update: old: " SIZE_FORMAT "B, new: " SIZE_FORMAT "B",
                      _target_occupancy, new_target_occupancy);
  _target_occupancy = new_target_occupancy;
}

void G1IHOPControl::update_allocation_info(double allocation_time_s, size_t allocated_bytes, size_t additional_buffer_size) {
  assert(allocation_time_s >= 0.0, "Allocation time must be positive.");

  _last_allocation_time_s = allocation_time_s;
  _last_allocated_bytes = allocated_bytes;
}

// should be defined in globalDefinitions.hpp
template<typename T>
inline double percent_of(T numerator, T denominator) {
  return denominator != 0 ? (double)numerator / denominator * 100.0 : 0.0;
}

void G1IHOPControl::print() {
  assert(_target_occupancy > 0, "Target occupancy still not updated yet.");
  size_t cur_conc_mark_start_threshold = get_conc_mark_start_threshold();
  log_debug(gc, ihop)("Basic information (value update), threshold: " SIZE_FORMAT "B (%1.2f), target occupancy: " SIZE_FORMAT "B, current occupancy: " SIZE_FORMAT "B, "
                      "recent allocation size: " SIZE_FORMAT "B, recent allocation duration: %1.2fms, recent old gen allocation rate: %1.2fB/s, recent marking phase length: %1.2fms",
                      cur_conc_mark_start_threshold,
                      percent_of(cur_conc_mark_start_threshold, _target_occupancy),
                      _target_occupancy,
                      G1CollectedHeap::heap()->used(),
                      _last_allocated_bytes,
                      _last_allocation_time_s * 1000.0,
                      _last_allocation_time_s > 0.0 ? _last_allocated_bytes / _last_allocation_time_s : 0.0,
                      last_marking_length_s() * 1000.0);
}

void G1IHOPControl::send_trace_event(G1NewTracer* tracer) {
  assert(_target_occupancy > 0, "Target occupancy still not updated yet.");
  tracer->report_basic_ihop_statistics(get_conc_mark_start_threshold(),
                                       _target_occupancy,
                                       G1CollectedHeap::heap()->used(),
                                       _last_allocated_bytes,
                                       _last_allocation_time_s,
                                       last_marking_length_s());
}

G1StaticIHOPControl::G1StaticIHOPControl(double ihop_percent) :
  G1IHOPControl(ihop_percent),
  _last_marking_length_s(0.0) {
}
