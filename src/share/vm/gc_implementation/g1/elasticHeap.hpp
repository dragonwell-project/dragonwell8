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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_ELASTICHEAP_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_ELASTICHEAP_HPP

#include "gc_implementation/g1/heapRegionSet.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#include "runtime/mutex.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/exceptions.hpp"

#define GC_INTERVAL_SEQ_LENGTH 10

class ElasticHeap;

// ElasticHeapConcThread:
// Commit/uncommit memory (several GB) may cost significant time so we won't do it in STW.
// We use a concurrent thread for doing commit/uncommit/pretouch the memory of regions
// Heap regions will be removed from free list of _hrm and put into commit/uncommit list
// for doing the commit/uncommit job and moved back into _hrm in next GC stw
// after commit/uncommit finishes.

class ElasticHeapConcThread : public NamedThread {
friend class ElasticHeap;
private:
  ElasticHeap*        _elastic_heap;
  // List of regions to uncommit
  FreeRegionList      _uncommit_list;
  // List of regions to commit
  FreeRegionList      _commit_list;
  // List of old regions to free
  FreeRegionList      _to_free_list;

  // Indicate the commit/uncommit is working
  // Will be set when VM thread(GC) triggers ElasticHeapThread to do memory job
  // and be cleared when the memory job finishes
  //
  // Java threads(Jcmd listener or MXBean) will check this flag if the memory work is done
  // ElasticHeap::recover_for_full_gc will also check this flag
  volatile bool       _working;

  // ElasticHeapThread will wait on it for doing memory job
  // And VM trhead(GC) will notify ElasticHeapThread to do memory job via _conc_lock
  Monitor*            _conc_lock;
  // Java threads(Jcmd listener or MXBean) will wait on _working_lock for the finish of memory work
  // ElasticHeap::recover_for_full_gc will also wait on _working_lock for the finish of memory work
  Monitor*            _working_lock;

  // Number of parallel worker threads
  uint                _parallel_worker_threads;
  // Parallel workers
  FlexibleWorkGang*   _parallel_workers;
  // For terminating conc thread
  bool                _should_terminate;
  bool                _has_terminated;

  void                sanity_check();
  void                print_work_summary(uint uncommit_length, uint commit_length, double start);
  // Commit/Uncommit phyical pages of regions
  void                do_memory_job(bool pretouch);
  void                do_memory_job_after_full_collection();
  // Commit/Uncommit phyical pages of regions
  uint                do_memory(FreeRegionList* list, HeapRegionClosure* cl);
  // Parallel Commit/Uncommit phyical pages
  void                par_work_on_regions(FreeRegionList* list, HeapRegionClosure* cl);
  void                wait_for_universe_init();

public:
  ElasticHeapConcThread(ElasticHeap* elastic_heap);

  // Region list for commit/uncommit
  FreeRegionList*     uncommit_list()           { return &_uncommit_list; }
  FreeRegionList*     commit_list()             { return &_commit_list; }
  FreeRegionList*     to_free_list()            { return &_to_free_list; }

  void                start();
  bool                working() const           { return _working; }
  void                set_working();
  void                clear_working()           {
    assert(_working, "Sanity");
    _working = false;
  }

  // Lock for manipulating ElasticHeapThread data structures
  Monitor*            conc_lock()               { return _conc_lock; }
  Monitor*            working_lock()            { return _working_lock; }
  void                sleep_before_next_cycle();
  virtual void        run();
  void                stop();

  // Count of regions in FreeRegionList
  uint                total_unavaiable_regions();
};

// Data structure for elastic heap timer thread
// We will trigger young gc in this thread
// but we cannot do it in a regular native thread(a limitation in JDK1.8)
// so we have to make it a Java thread
class ElasticHeapTimer : AllStatic {
  friend class ElasticHeap;
private:
  volatile static bool _should_terminate;
  static JavaThread*  _thread;
  static Monitor*     _monitor;

public:
  // Timer thread entry
  static void         timer_thread_entry(JavaThread* thread, TRAPS);
  static void         start();
  static void         stop();
  static bool         has_error(TRAPS, const char* error);
};

class ElasticHeapEvaluator : public CHeapObj<mtGC> {
protected:
  ElasticHeap*        _elas;
  G1CollectedHeap*    _g1h;
  G1CollectorPolicy*  _g1_policy;
  HeapRegionManager*  _hrm;
public:
  ElasticHeapEvaluator(ElasticHeap* eh);

  void                sanity_check();
  virtual void        evaluate() = 0;
  virtual void        evaluate_young()                  { ShouldNotReachHere(); }
  virtual void        evaluate_old()                    { ShouldNotReachHere(); }
  virtual void        evaluate_old_common();

  virtual bool        ready_to_initial_mark()       { ShouldNotReachHere(); return false; }
};

class RecoverEvaluator : public ElasticHeapEvaluator {
public:
  RecoverEvaluator(ElasticHeap* eh)
    : ElasticHeapEvaluator(eh) {}
  virtual void        evaluate();
};

class PeriodicEvaluator : public ElasticHeapEvaluator {
public:
  PeriodicEvaluator(ElasticHeap* eh)
    : ElasticHeapEvaluator(eh) {}
  virtual void        evaluate();
  virtual void        evaluate_young();
  virtual void        evaluate_old()          { evaluate_old_common(); }
  virtual bool        ready_to_initial_mark();
};

class GenerationLimitEvaluator : public ElasticHeapEvaluator {
public:
  GenerationLimitEvaluator(ElasticHeap* eh)
    : ElasticHeapEvaluator(eh) {}
  virtual void        evaluate();
  virtual void        evaluate_young();
  virtual void        evaluate_old()          { evaluate_old_common(); }
  virtual bool        ready_to_initial_mark();
};

class SoftmxEvaluator : public ElasticHeapEvaluator {
private:
  static  uint        _min_free_region_count;
  uint                accommodate_heap_regions(uint regions);
public:
  SoftmxEvaluator(ElasticHeap* eh)
    : ElasticHeapEvaluator(eh) {}
  virtual void        evaluate();
};

class ElasticHeapGCStats : public CHeapObj<mtGC> {
friend class ElasticHeap;
public:
  ElasticHeapGCStats(ElasticHeap*, G1CollectedHeap*);

  double              avg_normalized_interval() const {
    return _recent_ygc_normalized_interval_ms->avg();
  }
  double              last_normalized_interval() const {
    return _recent_ygc_normalized_interval_ms->last();
  }
  int                 num_normalized_interval() const {
    return _recent_ygc_normalized_interval_ms->num();
  }
  uint                last_normalized_eden_consumed_length() const {
    return _last_normalized_eden_consumed_length;
  }
  double              last_initial_mark_timestamp_s() const {
    return _last_initial_mark_timestamp_s;
  }
  double              last_initial_mark_interval_s() const {
    return _last_initial_mark_interval_s;
  }
  size_t              last_initial_mark_non_young_bytes() const {
    return _last_initial_mark_non_young_bytes;
  }
  // Reset the tracking info of mixed gc
  void                reset_mixed_gc_info() {
    _last_gc_mixed = false;
    _mixed_gc_finished = false;;
    _need_mixed_gc = false;
  }

  bool                last_gc_mixed() const          { return _last_gc_mixed; }

  bool                need_mixed_gc() const          { return _need_mixed_gc; }
  void                set_need_mixed_gc(bool f)      { _need_mixed_gc = f; }

  double              last_gc_end_timestamp_s() const  { return _last_gc_end_timestamp_s; }
  void                set_last_gc_end_timestamp_s(double s) { _last_gc_end_timestamp_s = s; }

  void                track_gc_start(bool full_gc);

  bool                check_mixed_gc_finished();
private:
  ElasticHeap*        _elas;
  G1CollectedHeap*    _g1h;
  // G1 will use non-fixed size of young generation
  // We scale the gc interval to a normalized interval as max size of young generation
  // e.g. we make interval X 2 if last GC only used 1/2 of max size of young gen
  TruncatedSeq*       _recent_ygc_normalized_interval_ms;
  // Time of last GC in seccond
  volatile double     _last_gc_end_timestamp_s;
  // Time of initial mark in seccond
  volatile double     _last_initial_mark_timestamp_s;
  // If last gc is mixed gc
  volatile bool       _last_gc_mixed;
  // If mixed gc has finished
  volatile bool       _mixed_gc_finished;
  // If mixed gc needs to happen
  volatile bool       _need_mixed_gc;
  // The interval in second between last 2 intial-marks
  volatile double     _last_initial_mark_interval_s;
  // Bytes of non-young part in last initial mark
  size_t              _last_initial_mark_non_young_bytes;
  // Number of eden regions consumed in last gc interval(normalized)
  uint                _last_normalized_eden_consumed_length;
};

class ElasticHeapSetting;

// ElasticHeap:
// Main class of elastic heap feature
class ElasticHeap : public CHeapObj<mtGC> {
friend class ElasticHeapEvaluator;
friend class RecoverEvaluator;
friend class PeriodicEvaluator;
friend class GenerationLimitEvaluator;
friend class SoftmxEvaluator;
public:
  ElasticHeap(G1CollectedHeap* g1h);
  ~ElasticHeap();

  enum ErrorType {
    NoError = 0,
    HeapAdjustInProgress,
    GCTooFrequent,
    IllegalYoungPercent,
    IllegalMode
  };

  // Modes when elastic heap is activated
  enum EvaluationMode {
    InactiveMode = 0,
    PeriodicUncommitMode,
    GenerationLimitMode,
    SoftmxMode,
    EvaluationModeNum
  };

  static uint         ignore_arg();

  ElasticHeapGCStats* stats()         { return _stats; }
  ElasticHeapSetting* setting()       { return _setting; }
private:
  G1CollectedHeap*    _g1h;
  ElasticHeapGCStats* _stats;
  ElasticHeapSetting* _setting;
  // Initial value of InitiatingHeapOccupancyPercent
  uint                _orig_ihop;
  // Initial value of desired young length
  uint                _orig_min_desired_young_length;
  uint                _orig_max_desired_young_length;

  // Indicates in the concurrent working cycle of memory job
  // 1. A concurrent cycle starts (start_conc_cycle) in VM Thread(GC stw) when triggering
  // ElasticHeapConcThread to do memory job
  // 2. A concurrent cycle ends (end_conc_cycle) in the next gc pause after region
  // commit/uncommit finishes and move regions in ElasticHeapConcThread back into _hrm
  volatile bool       _in_conc_cycle;

  // Concurrent thread for doing memory job
  ElasticHeapConcThread*  _conc_thread;

  volatile int        _configure_setting_lock;

  ElasticHeapEvaluator* _evaluators[EvaluationModeNum];

  bool _heap_capacity_changed;

  void                start_conc_cycle();
  void                end_conc_cycle();

  void                move_regions_back_to_hrm();
  // Move the regions in _thread back to _hrm and clear in_cycle
  void                finish_conc_cycle();
  // Check completion of previous ElasticHeapConcThread work
  void                check_end_of_previous_conc_cycle();
  // Update heap size information after expanding completes
  void                update_expanded_heap_size();
  // Do the elastic heap evaluation and operation
  void                evaluate_elastic_work();

  void                try_starting_conc_cycle();

  void                wait_for_conc_thread_working_done(bool safepointCheck);
  bool                enough_free_regions_to_uncommit(uint num_regions_to_uncommit);

  // Update min/max desired young length after change young length
  void                update_desired_young_length(uint unavailable_young_length);

public:
  void                uncommit_region_memory(HeapRegion* hr);
  void                commit_region_memory(HeapRegion* hr, bool pretouch);
  void                free_region_memory(HeapRegion* hr);

  bool                in_conc_cycle()              { return _in_conc_cycle; }

  // Record gc information
  void                record_gc_start(bool full_gc = false);
  void                record_gc_end();

  void                prepare_in_gc_start();

  // Main entry in GC pause for elastic heap
  void                perform_after_young_collection();
  void                perform_after_full_collection();
  // Commit/uncommit regions
  void                uncommit_regions(uint num);
  void                commit_regions(uint num);

  void                resize_young_length(uint target_length);
  void                change_heap_capacity(uint target_heap_regions);

  uint                max_young_length() const      { return _orig_max_desired_young_length; }

  // Number of regions that old gen overlap with young gen
  // So young gen cannot use as many as MaxNewSize
  uint                overlapped_young_regions_with_old_gen();

  uint                num_unavailable_regions();

  // Main entry for processing elastic heap via JCMD/MXBean
  ErrorType           configure_setting(uint young_percent, uint uncommit_ihop, uint softmx_percent, bool fullgc = false);

  // Get the commit percent and freed bytes
  int                 young_commit_percent() const;
  jlong               young_uncommitted_bytes() const;
  int                 softmx_percent() const;
  jlong               uncommitted_bytes() const;
  int                 uncommit_ihop() const;

  // Wait for the conc thread working done and finish the current cycle
  void                wait_for_conc_cycle_end();
  // If Full GC happens in auto mode, elastic heap will wait to recover the uncommitted regions
  void                wait_to_recover();

  // If the gc is triggered by elastic heap to resize young gen
  bool                is_gc_to_resize_young_gen();
  // Max survivor regions in this gc.
  // We probably do a young gen resize in the end of gc
  // so we cannot leave a too large survior size if we will make young gen smaller
  uint                max_available_survivor_regions();

  // Check to initiate initial-mark
  void                check_to_initate_conc_mark();
  // Check to invoke a GC
  void                check_to_trigger_ygc();

  // With softmx mode, we will change the capacity of heap
  bool                heap_capacity_changed()     { return _heap_capacity_changed; }
  void                set_heap_capacity_changed(uint num);

  EvaluationMode      evaluation_mode() const;
  bool                conflict_mode(EvaluationMode target_mode);

  // Get string from error type
  static const char*  to_string(ErrorType type) {
    switch(type) {
      case HeapAdjustInProgress: return "last elastic-heap resize is still in progress";
      case GCTooFrequent: return "gc is too frequent";
      case IllegalYoungPercent: return "illegal percent";
      case IllegalMode: return "not in correct mode";
      default: ShouldNotReachHere(); return NULL;
    }
  }

  // Get string from EvaluationMode
  static const char*  to_string(EvaluationMode mode) {
    switch (mode) {
      case InactiveMode: return "inactive";
      case PeriodicUncommitMode: return "periodic uncommit";
      case GenerationLimitMode: return "generation limit";
      case SoftmxMode: return "softmx";
      default: ShouldNotReachHere(); return NULL;
    }
  }

  void                destroy();

  bool                can_turn_on_periodic_uncommit();

  bool                ready_to_uncommit_after_mixed_gc();
  void                uncommit_old_gen();


  uint                calculate_young_list_desired_max_length();
  double              g1_reserve_factor();
  uint                g1_reserve_regions();
};

class ElasticHeapSetting : public CHeapObj<mtGC> {
friend class ElasticHeap;
public:
  ElasticHeapSetting(ElasticHeap*, G1CollectedHeap*);
  // Whether do elastic heap resize by explicit command
  bool                generation_limit_set() const {
      return young_percent_set() || uncommit_ihop_set();
  }
  // Whether a jcmd/mxbean command set the young commit percent
  bool                young_percent_set() const {
    return _young_percent != 0;
  }
  uint                young_percent() const {
    return _young_percent;
  }
  void                set_young_percent(uint p) {
    _young_percent = p;
  }
  // Whether a jcmd/mxbean command set the uncommit ihop
  bool                uncommit_ihop_set() const {
    return _uncommit_ihop != 0;
  }
  uint                uncommit_ihop() const {
    return _uncommit_ihop;
  }
  bool                softmx_percent_set() const {
    return _softmx_percent != 0;
  }
  uint                softmx_percent() const {
    return _softmx_percent;
  }
  void                set_softmx_percent(uint p) {
    _softmx_percent = p;
  }

  bool                evaluating() const { return _evaluating; }
  void                set_evaluating(bool f) { _evaluating = f; }

  ElasticHeap::ErrorType change_young_percent(uint young_percent, bool& trigger_gc);
  ElasticHeap::ErrorType change_uncommit_ihop(uint uncommit_ihop, bool& trigger_gc);
  ElasticHeap::ErrorType change_softmx_percent(uint softmx_percent, bool& trigger_gc, bool fullgc);
  ElasticHeap::ErrorType process_arg(uint young_percent, uint uncommit_ihop, uint softmx_percent,
                                     bool& trigger_gc, bool fullgc);
  ElasticHeap::EvaluationMode target_evaluation_mode(uint young_percent,
                                             uint uncommit_ihop,
                                             uint softmx_percent);
private:
  ElasticHeap*        _elas;
  G1CollectedHeap*    _g1h;
  // Young list percentage set by command
  volatile uint       _young_percent;
  // Set the initiating heap occupancy for elastic heap and do uncommit after concurrent gc
  volatile uint       _uncommit_ihop;
  // Set the softmx percent
  volatile uint       _softmx_percent;
  volatile bool       _evaluating;

  // Check if we can change the young percent by jcmd/MXBean
  bool                can_change_young_percent(uint percent);

  static uint         ignore_arg()  { return (uint)-1; }
  static bool         ignore_arg(uint arg, uint cur_value)  {
    return arg == cur_value || arg == ignore_arg();
  }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_ELASTICHEAP_HPP
