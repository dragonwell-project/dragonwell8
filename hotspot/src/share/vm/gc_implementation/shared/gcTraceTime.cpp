/*
 * Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_implementation/shared/workerDataArray.inline.hpp"
#include "runtime/globals.hpp"
#include "runtime/os.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/timer.hpp"
#include "utilities/ostream.hpp"
#include "utilities/ticks.hpp"

GCTraceTime::GCTraceTime(const char* title, bool doit, bool print_cr, GCTimer* timer, GCId gc_id) :
    _title(title), _doit(doit), _print_cr(print_cr), _timer(timer), _start_counter() {
  if (_doit || _timer != NULL) {
    _start_counter.stamp();
  }

  if (_timer != NULL) {
    assert(SafepointSynchronize::is_at_safepoint(), "Tracing currently only supported at safepoints");
    assert(Thread::current()->is_VM_thread(), "Tracing currently only supported from the VM thread");

    _timer->register_gc_phase_start(title, _start_counter);
  }

  if (_doit) {
    gclog_or_tty->date_stamp(PrintGCDateStamps);
    gclog_or_tty->stamp(PrintGCTimeStamps);
    if (PrintGCID) {
      gclog_or_tty->print("#%u: ", gc_id.id());
    }
    gclog_or_tty->print("[%s", title);
    gclog_or_tty->flush();
  }
}

GCTraceTime::~GCTraceTime() {
  Ticks stop_counter;

  if (_doit || _timer != NULL) {
    stop_counter.stamp();
  }

  if (_timer != NULL) {
    _timer->register_gc_phase_end(stop_counter);
  }

  if (_doit) {
    const Tickspan duration = stop_counter - _start_counter;
    double duration_in_seconds = duration.seconds();
    if (_print_cr) {
      gclog_or_tty->print_cr(", %3.7f secs]", duration_in_seconds);
    } else {
      gclog_or_tty->print(", %3.7f secs]", duration_in_seconds);
    }
    gclog_or_tty->flush();
  }
}

//========================================================================
// GenGC Log level class
class GenGCLog : public AllStatic {
 public:
  typedef enum {
    LevelNone,
    LevelFine,
    LevelFinest,
  } LogLevel;

 private:
  static LogLevel _level;
 public:
  inline static bool fine() {
    return _level >= LevelFine;
  }

  inline static bool finest() {
    return _level == LevelFinest;
  }

  static LogLevel level() {
    return _level;
  }
};

GenGCLog::LogLevel GenGCLog::_level = GenGCLog::LevelNone;

GenGCPhaseTimes::GenGCPhaseTimes(uint max_gc_threads) :
  _max_gc_threads(max_gc_threads)
{
  assert(max_gc_threads > 0, "Must have some GC threads");

  _gc_par_phases[GCWorkerStart] = new WorkerDataArray<double>(max_gc_threads, "GC Worker Start (ms)", false, GenGCLog::LevelFine, 2);
  _gc_par_phases[RootProcess] = new WorkerDataArray<double>(max_gc_threads, "Root Processing (ms)", true, GenGCLog::LevelFine, 2);

  // Root scanning phases
  _gc_par_phases[ThreadRoots] = new WorkerDataArray<double>(max_gc_threads, "Thread Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[StringTableRoots] = new WorkerDataArray<double>(max_gc_threads, "StringTable Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[UniverseRoots] = new WorkerDataArray<double>(max_gc_threads, "Universe Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[JNIRoots] = new WorkerDataArray<double>(max_gc_threads, "JNI Handles Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[ObjectSynchronizerRoots] = new WorkerDataArray<double>(max_gc_threads, "ObjectSynchronizer Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[FlatProfilerRoots] = new WorkerDataArray<double>(max_gc_threads, "FlatProfiler Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[ManagementRoots] = new WorkerDataArray<double>(max_gc_threads, "Management Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[SystemDictionaryRoots] = new WorkerDataArray<double>(max_gc_threads, "SystemDictionary Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[CLDGRoots] = new WorkerDataArray<double>(max_gc_threads, "CLDG Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[JVMTIRoots] = new WorkerDataArray<double>(max_gc_threads, "JVMTI Roots (ms)", true, GenGCLog::LevelFinest, 3);
  _gc_par_phases[CodeCacheRoots] = new WorkerDataArray<double>(max_gc_threads, "CodeCache Roots (ms)", true, GenGCLog::LevelFinest, 3);

  _gc_par_phases[OldGenScan] = new WorkerDataArray<double>(max_gc_threads, "older-gen scanning (ms)", true, GenGCLog::LevelFine, 3);

  _gc_par_phases[Other] = new WorkerDataArray<double>(max_gc_threads, "GC Worker Other (ms)", true, GenGCLog::LevelFine,     2);
  _gc_par_phases[GCWorkerTotal] = new WorkerDataArray<double>(max_gc_threads, "GC Worker Total (ms)", true, GenGCLog::LevelFine, 2);
  _gc_par_phases[GCWorkerEnd] = new WorkerDataArray<double>(max_gc_threads, "GC Worker End (ms)", false, GenGCLog::LevelFine, 2);
}

void GenGCPhaseTimes::note_gc_start(uint active_gc_threads) {
  assert(active_gc_threads > 0, "The number of threads must be > 0");
  assert(active_gc_threads <= _max_gc_threads, "The number of active threads must be <= the max number of threads");
  _active_gc_threads = active_gc_threads;

  for (int i = 0; i < GCParPhasesSentinel; i++) {
    _gc_par_phases[i]->reset();
  }
}

void GenGCPhaseTimes::note_gc_end() {
  for (uint i = 0; i < _active_gc_threads; i++) {
    double worker_time = _gc_par_phases[GCWorkerEnd]->get(i) - _gc_par_phases[GCWorkerStart]->get(i);
    record_time_secs(GCWorkerTotal, i , worker_time);

    double worker_known_time = _gc_par_phases[RootProcess]->get(i);
    record_time_secs(Other, i, worker_time - worker_known_time);
  }

  for (int i = 0; i < GCParPhasesSentinel; i++) {
    _gc_par_phases[i]->verify(_active_gc_threads);
  }
}

void GenGCPhaseTimes::print_stats(int level, const char* str, double value) {
  LineBuffer(level).append_and_print_cr("[%s: %.1lf ms]", str, value);
}

void GenGCPhaseTimes::print_stats(int level, const char* str, double value, uint workers) {
  LineBuffer(level).append_and_print_cr("[%s: %.1lf ms, GC Workers: %u]", str, value, workers);
}

// record the time a phase took in seconds
void GenGCPhaseTimes::record_time_secs(GCParPhases phase, uint worker_i, double secs) {
  _gc_par_phases[phase]->set(worker_i, secs);
}

// add a number of seconds to a phase
void GenGCPhaseTimes::add_time_secs(GCParPhases phase, uint worker_i, double secs) {
  _gc_par_phases[phase]->add(worker_i, secs);
}

// return the average time for a phase in milliseconds
double GenGCPhaseTimes::average_time_ms(GCParPhases phase) {
  return _gc_par_phases[phase]->average(_active_gc_threads) * 1000.0;
}

double GenGCPhaseTimes::get_time_ms(GCParPhases phase, uint worker_i) {
  return _gc_par_phases[phase]->get(worker_i) * 1000.0;
}

double GenGCPhaseTimes::sum_time_ms(GCParPhases phase) {
  return _gc_par_phases[phase]->sum(_active_gc_threads) * 1000.0;
}

double GenGCPhaseTimes::min_time_ms(GCParPhases phase) {
  return _gc_par_phases[phase]->minimum(_active_gc_threads) * 1000.0;
}

double GenGCPhaseTimes::max_time_ms(GCParPhases phase) {
  return _gc_par_phases[phase]->maximum(_active_gc_threads) * 1000.0;
}

double GenGCPhaseTimes::average_thread_work_items(GCParPhases phase) {
  assert(_gc_par_phases[phase]->thread_work_items() != NULL, "No sub count");
  return _gc_par_phases[phase]->thread_work_items()->average(_active_gc_threads);
}

class GenGCParPhasePrinter : public StackObj {
  GenGCPhaseTimes* _phase_times;
 public:
  GenGCParPhasePrinter(GenGCPhaseTimes* phase_times) : _phase_times(phase_times) {}

  void print(GenGCPhaseTimes::GCParPhases phase_id) {
    WorkerDataArray<double>* phase = _phase_times->_gc_par_phases[phase_id];

    if (phase->_length == 1) {
      print_single_length(phase_id, phase);
    } else {
      print_multi_length(phase_id, phase);
    }
  }

 private:

  void print_single_length(GenGCPhaseTimes::GCParPhases phase_id, WorkerDataArray<double>* phase) {
    // No need for min, max, average and sum for only one worker
    LineBuffer buf(phase->_indent_level);
    buf.append_and_print_cr("[%s:  %.1lf]", phase->_title, _phase_times->get_time_ms(phase_id, 0));
  }

  void print_time_values(LineBuffer& buf, GenGCPhaseTimes::GCParPhases phase_id, WorkerDataArray<double>* phase) {
    uint active_length = _phase_times->_active_gc_threads;
    for (uint i = 0; i < active_length; ++i) {
      buf.append("  %.1lf", _phase_times->get_time_ms(phase_id, i));
    }
    buf.print_cr();
  }

  void print_multi_length(GenGCPhaseTimes::GCParPhases phase_id, WorkerDataArray<double>* phase) {
    LineBuffer buf(phase->_indent_level);
    buf.append("[%s:", phase->_title);

    print_time_values(buf, phase_id, phase);

    buf.append(" Min: %.1lf, Avg: %.1lf, Max: %.1lf, Diff: %.1lf",
        _phase_times->min_time_ms(phase_id), _phase_times->average_time_ms(phase_id), _phase_times->max_time_ms(phase_id),
        _phase_times->max_time_ms(phase_id) - _phase_times->min_time_ms(phase_id));

    if (phase->_print_sum) {
      // for things like the start and end times the sum is not
      // that relevant
      buf.append(", Sum: %.1lf", _phase_times->sum_time_ms(phase_id));
    }

    buf.append_and_print_cr("]");
  }
};

void GenGCPhaseTimes::print() {
  GenGCParPhasePrinter par_phase_printer(this);

  for (int i = 0; i <= GCMainParPhasesLast; i++) {
    par_phase_printer.print((GCParPhases) i);
  }
}

void GenGCPhaseTimes::log_gc_details() {
  this->print();
  gclog_or_tty->flush();
}

GenGCParPhaseTimesTracker::GenGCParPhaseTimesTracker(GenGCPhaseTimes* phase_times, GenGCPhaseTimes::GCParPhases phase, uint worker_id) :
    _phase_times(phase_times), _phase(phase), _worker_id(worker_id) {
  if (_phase_times != NULL) {
    _start_time = os::elapsedTime();
  }
}

GenGCParPhaseTimesTracker::~GenGCParPhaseTimesTracker() {
  if (_phase_times != NULL) {
    _phase_times->record_time_secs(_phase, _worker_id, os::elapsedTime() - _start_time);
  }
}

