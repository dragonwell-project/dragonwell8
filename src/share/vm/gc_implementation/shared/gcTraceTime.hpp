/*
 * Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP

#include "gc_implementation/shared/gcTrace.hpp"
#include "prims/jni_md.h"
#include "utilities/ticks.hpp"
#include "gc_implementation/shared/workerDataArray.hpp"

class GCTimer;
class LineBuffer;
template <class T> class WorkerDataArray;

class GCTraceTime {
  const char* _title;
  bool _doit;
  bool _print_cr;
  GCTimer* _timer;
  Ticks _start_counter;

 public:
  GCTraceTime(const char* title, bool doit, bool print_cr, GCTimer* timer, GCId gc_id);
  ~GCTraceTime();
};

class GenGCPhaseTimes : public CHeapObj<mtGC> {
  friend class GenGCParPhasePrinter;

  uint _active_gc_threads;
  uint _max_gc_threads;

 public:
  enum GCParPhases {
    GCWorkerStart,
    RootProcess,
    ThreadRoots,
    StringTableRoots,
    UniverseRoots,
    JNIRoots,
    ObjectSynchronizerRoots,
    FlatProfilerRoots,
    ManagementRoots,
    SystemDictionaryRoots,
    CLDGRoots,
    CodeCacheRoots,
    JVMTIRoots,
    OldGenScan,
    Other,
    GCWorkerTotal,
    GCWorkerEnd,
    GCParPhasesSentinel
  };
private:
  // Markers for grouping the phases in the GCPhases enum above
  static const int GCMainParPhasesLast = GCWorkerEnd;

  WorkerDataArray<double>* _gc_par_phases[GCParPhasesSentinel];

  double _cur_collection_par_time_ms;

  // Helper methods for detailed logging
  void print_stats(int level, const char* str, double value);
  void print_stats(int level, const char* str, double value, uint workers);

 public:
  GenGCPhaseTimes(uint max_gc_threads);
  void note_gc_start(uint active_gc_threads);
  void note_gc_end();
  void print();
  void log_gc_details();

  // record the time a phase took in seconds
  void record_time_secs(GCParPhases phase, uint worker_i, double secs);

  // add a number of seconds to a phase
  void add_time_secs(GCParPhases phase, uint worker_i, double secs);

  // return the average time for a phase in milliseconds
  double average_time_ms(GCParPhases phase);

 private:
  double get_time_ms(GCParPhases phase, uint worker_i);
  double sum_time_ms(GCParPhases phase);
  double min_time_ms(GCParPhases phase);
  double max_time_ms(GCParPhases phase);
  double average_thread_work_items(GCParPhases phase);

 public:

  void record_par_time(double ms) {
    _cur_collection_par_time_ms = ms;
  }
};

class GenGCParPhaseTimesTracker : public StackObj {
  double _start_time;
  GenGCPhaseTimes::GCParPhases _phase;
  GenGCPhaseTimes* _phase_times;
  uint _worker_id;
public:
  GenGCParPhaseTimesTracker(GenGCPhaseTimes* phase_times, GenGCPhaseTimes::GCParPhases phase, uint worker_id);
  ~GenGCParPhaseTimesTracker();
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_GCTRACETIME_HPP
