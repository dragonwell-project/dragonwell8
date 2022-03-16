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
#include "gc_implementation/g1/g1CollectedHeap.hpp"
#include "gc_implementation/g1/g1CollectorPolicy.hpp"
#include "gc_implementation/g1/elasticHeap.hpp"
#include "gc_implementation/g1/concurrentMarkThread.hpp"
#include "runtime/init.hpp"
#include "runtime/javaCalls.hpp"

template <bool commit, bool free>
class RegionMemoryClosure : public HeapRegionClosure {
private:
  ElasticHeap* _elastic_heap;
  bool _pretouch;
public:
  RegionMemoryClosure(ElasticHeap* elastic_heap, bool pretouch) :
    _elastic_heap(elastic_heap), _pretouch(pretouch)  {}

  virtual bool doHeapRegion(HeapRegion* hr) {
    if (commit) {
      _elastic_heap->commit_region_memory(hr, _pretouch);
    } else if (free) {
      _elastic_heap->free_region_memory(hr);
    } else {
      _elastic_heap->uncommit_region_memory(hr);
    }
    return false;
  }
};

ElasticHeapConcThread::ElasticHeapConcThread(ElasticHeap* elastic_heap) :
    _elastic_heap(elastic_heap),
    _uncommit_list("Free list for uncommitting regions", new MasterFreeRegionListMtSafeChecker()),
    _commit_list("Free list for committing regions", new MasterFreeRegionListMtSafeChecker()),
    _to_free_list("Free list for freeing old regions", new MasterFreeRegionListMtSafeChecker()),
    _working(false),
    _parallel_worker_threads(0),
    _parallel_workers(NULL),
    _should_terminate(false),
    _has_terminated(false) {
  set_name("ElasticHeapThread");
  _conc_lock = new Monitor(Mutex::nonleaf, "ElasticHeapThread::_conc_lock", Mutex::_allow_vm_block_flag);
  _working_lock = new Monitor(Mutex::nonleaf, "ElasticHeapThread::_working_lock", Mutex::_allow_vm_block_flag);

  if (FLAG_IS_DEFAULT(ElasticHeapParallelWorkers)) {
    ElasticHeapParallelWorkers = ConcGCThreads;
  }

  if (ElasticHeapParallelWorkers > 1) {
    _parallel_worker_threads = ElasticHeapParallelWorkers;
  }

  if (_parallel_worker_threads != 0) {
    _parallel_workers = new FlexibleWorkGang("Elastic Heap Parallel Worker Threads",
                                             _parallel_worker_threads, false, false);
    if (_parallel_workers == NULL) {
      vm_exit_during_initialization("Failed necessary allocation.");
    } else {
      _parallel_workers->initialize_workers();
    }
  }
}

void ElasticHeapConcThread::start() {
  if (!os::create_thread(this, os::cgc_thread)) {
    vm_exit_during_initialization("Cannot create ElasticHeapConcThread. Out of system resources.");
  }
  Thread::start(this);
}

void ElasticHeapConcThread::wait_for_universe_init() {
  MutexLockerEx x(_conc_lock, Mutex::_no_safepoint_check_flag);
  while (!is_init_completed()) {
    _conc_lock->wait(Mutex::_no_safepoint_check_flag, 100 /* ms */);
  }
}

void ElasticHeapConcThread::sleep_before_next_cycle() {
  MutexLockerEx x(_conc_lock, Mutex::_no_safepoint_check_flag);
  while (!working() && !_should_terminate) {
    // Wait for ElasticHeap::evaluate_elastic_work in safepoint to notify
    _conc_lock->wait(Mutex::_no_safepoint_check_flag);
  }
}

void ElasticHeapConcThread::sanity_check() {
  if (!_uncommit_list.is_empty()) {
    guarantee(_commit_list.is_empty(), "sanity");
  } else if (!_commit_list.is_empty()) {
    guarantee(_uncommit_list.is_empty(), "sanity");
  } else {
    guarantee(!_to_free_list.is_empty() || G1CollectedHeap::heap()->full_collection(), "sanity");
  }
}

void ElasticHeapConcThread::do_memory_job(bool pretouch) {
  double start = os::elapsedTime();
  uint commit_length = 0;
  uint uncommit_length = 0;

  RegionMemoryClosure<false, false> uncommit_cl(_elastic_heap, pretouch);
  RegionMemoryClosure<false, true> free_cl(_elastic_heap, pretouch);
  RegionMemoryClosure<true, false> commit_pretouch_cl(_elastic_heap, pretouch);

  sanity_check();

  if (!_uncommit_list.is_empty()) {
    uncommit_length += do_memory(&_uncommit_list, &uncommit_cl);
  }

  if (!_commit_list.is_empty()) {
    commit_length += do_memory(&_commit_list, &commit_pretouch_cl);
  }

  if (!_to_free_list.is_empty()) {
    uncommit_length += do_memory(&_to_free_list, &free_cl);
  }

  if (PrintGCDetails && PrintElasticHeapDetails) {
    print_work_summary(uncommit_length, commit_length, start);
  }
}

void ElasticHeapConcThread::do_memory_job_after_full_collection() {
  if (!_uncommit_list.is_empty() || !_commit_list.is_empty()) {
    do_memory_job(AlwaysPreTouch);
  }
}

void ElasticHeapConcThread::set_working() {
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(!_working, "sanity");
  _working = true;
}

void ElasticHeapConcThread::print_work_summary(uint uncommit_length, uint commit_length, double start) {
  assert(uncommit_length !=0 || commit_length != 0, "sanity");
  const char* op = NULL;
  uint list_length = 0;
  if (uncommit_length != 0 && commit_length != 0) {
    op = "commit/uncommit";
    list_length = uncommit_length + commit_length;
  } else if (uncommit_length != 0) {
    op = "uncommit";
    list_length = uncommit_length;
  } else if (commit_length != 0) {
    op = "commit";
    list_length = commit_length;
  } else {
    ShouldNotReachHere();
  }
  size_t bytes = list_length * HeapRegion::GrainBytes;
  gclog_or_tty->print_cr("[Elastic Heap%s: %s " SIZE_FORMAT "%s spent %.3fs(concurrent workers: %u) ]",
    G1CollectedHeap::heap()->full_collection() ? "" : " concurrent thread",
    op,
    byte_size_in_proper_unit(bytes),
    proper_unit_for_byte_size(bytes),
    os::elapsedTime() - start,
    _parallel_worker_threads > 1 ? _parallel_worker_threads : 1);
}

void ElasticHeapConcThread::run() {
  wait_for_universe_init();

  while (!_should_terminate) {
    // wait until working is set.
    sleep_before_next_cycle();
    if (_should_terminate) {
      break;
    }

    assert(working(), "sanity");

    // We don't want a lot of page faults while accessing a lot of memory(several GB)
    // in a very short period of time(few seconds) which may cause Java threads slow down
    // significantly. So we will pretouch the regions after commit them.
    do_memory_job(true);

    // Thread work is done by now
    {
      MutexLockerEx x(_working_lock, Mutex::_no_safepoint_check_flag);
      clear_working();
      _working_lock->notify_all();
    }
  }

  {
    MutexLockerEx mu(Terminator_lock,
      Mutex::_no_safepoint_check_flag);
    _has_terminated = true;
    Terminator_lock->notify();
  }
}

void ElasticHeapConcThread::stop() {
  {
    MutexLockerEx ml(Terminator_lock);
    _should_terminate = true;
  }

  {
    MutexLockerEx ml(_conc_lock, Mutex::_no_safepoint_check_flag);
    _conc_lock->notify();
  }
  {
    MutexLockerEx ml(Terminator_lock);
    while (!_has_terminated) {
      Terminator_lock->wait();
    }
  }
}

uint ElasticHeapConcThread::do_memory(FreeRegionList* list, HeapRegionClosure* cl) {
  if (_parallel_worker_threads != 0) {
    par_work_on_regions(list, cl);
  } else {
    FreeRegionListIterator iter(list);
    while (iter.more_available()) {
      HeapRegion* hr = iter.get_next();
      cl->doHeapRegion(hr);
    }
  }
  return list->length();
}

class ElasticHeapParTask : public AbstractGangTask {
private:
  FreeRegionList* _list;
  HeapRegionClosure* _cl;
  uint _n_workers;
  uint _num_regions_per_worker;
public:
  ElasticHeapParTask(FreeRegionList* list, HeapRegionClosure* cl, uint n_workers)
    : AbstractGangTask("Elastic Heap memory commit/uncommit task"),
      _list(list), _cl(cl), _n_workers(n_workers) {
    if ((list->length() % n_workers) == 0 ) {
      _num_regions_per_worker = list->length() / _n_workers;
    } else {
      _num_regions_per_worker = list->length() / _n_workers + 1;
    }
  }

  void work(uint worker_id) {
    uint skipped = 0;
    FreeRegionListIterator iter(_list);
    while (iter.more_available()) {
      if (skipped == _num_regions_per_worker * worker_id) {
        break;
      }
      iter.get_next();
      skipped++;
    }
    uint index = 0;
    while (iter.more_available()) {
      if (index == _num_regions_per_worker) {
        break;
      }
      HeapRegion* hr = iter.get_next();
      _cl->doHeapRegion(hr);
      index++;
    }
  }
};

void ElasticHeapConcThread::par_work_on_regions(FreeRegionList* list, HeapRegionClosure* cl) {
  _parallel_workers->set_active_workers((int)_parallel_worker_threads);
  ElasticHeapParTask task(list, cl, _parallel_worker_threads);
  _parallel_workers->run_task(&task);
}

uint ElasticHeapConcThread::total_unavaiable_regions() {
  return _uncommit_list.length() + _commit_list.length() + _to_free_list.length();
}

volatile bool ElasticHeapTimer::_should_terminate = false;
JavaThread* ElasticHeapTimer::_thread = NULL;
Monitor*    ElasticHeapTimer::_monitor = NULL;

bool ElasticHeapTimer::has_error(TRAPS, const char* error) {
  if (HAS_PENDING_EXCEPTION) {
    tty->print_cr("%s", error);
    java_lang_Throwable::print(PENDING_EXCEPTION, tty);
    tty->cr();
    CLEAR_PENDING_EXCEPTION;
    return true;
  } else {
    return false;
  }
}

void ElasticHeapTimer::start() {
  _monitor = new Monitor(Mutex::nonleaf, "ElasticHeapTimer::_monitor", Mutex::_allow_vm_block_flag);

  EXCEPTION_MARK;
  Klass* k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(), true, CHECK);
  instanceKlassHandle klass (THREAD, k);
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK);

  const char thread_name[] = "Elastic Heap timer";
  Handle string = java_lang_String::create_from_str(thread_name, CHECK);

  // Initialize thread_oop to put it into the system threadGroup
  Handle thread_group (THREAD, Universe::system_thread_group());
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, thread_oop,
                       klass,
                       vmSymbols::object_initializer_name(),
                       vmSymbols::threadgroup_string_void_signature(),
                       thread_group,
                       string,
                       THREAD);
  if (has_error(THREAD, "Exception in VM (ElasticHeapTimer::start) : ")) {
    vm_exit_during_initialization("Cannot create ElasticHeap timer thread.");
    return;
  }

  KlassHandle group(THREAD, SystemDictionary::ThreadGroup_klass());
  JavaCalls::call_special(&result,
                        thread_group,
                        group,
                        vmSymbols::add_method_name(),
                        vmSymbols::thread_void_signature(),
                        thread_oop,             // ARG 1
                        THREAD);
  if (has_error(THREAD, "Exception in VM (ElasticHeapTimer::start) : ")) {
    vm_exit_during_initialization("Cannot create ElasticHeap timer thread.");
    return;
  }

  {
    MutexLocker mu(Threads_lock);
    _thread = new JavaThread(&ElasticHeapTimer::timer_thread_entry);
    if (_thread == NULL || _thread->osthread() == NULL) {
      vm_exit_during_initialization("Cannot create ElasticHeap timer thread. Out of system resources.");
    }

    java_lang_Thread::set_thread(thread_oop(), _thread);
    java_lang_Thread::set_daemon(thread_oop());
    _thread->set_threadObj(thread_oop());
    Threads::add(_thread);
    Thread::start(_thread);
  }
}

void ElasticHeapTimer::timer_thread_entry(JavaThread* thread, TRAPS) {
  while(!_should_terminate) {
    assert(!SafepointSynchronize::is_at_safepoint(), "Elastic Heap timer thread is a JavaThread");
    G1CollectedHeap::heap()->elastic_heap()->check_to_trigger_ygc();

    MutexLockerEx x(_monitor);
    if (_should_terminate) {
      break;
    }
    _monitor->wait(false /* no_safepoint_check */, 200);
  }
}

void ElasticHeapTimer::stop() {
  _should_terminate = true;
  {
    MutexLockerEx ml(_monitor, Mutex::_no_safepoint_check_flag);
    _monitor->notify();
  }
}

ElasticHeapGCStats::ElasticHeapGCStats(ElasticHeap* elas, G1CollectedHeap* g1h) :
    _elas(elas),
    _g1h(g1h),
    _recent_ygc_normalized_interval_ms(new TruncatedSeq(GC_INTERVAL_SEQ_LENGTH)),
    _last_gc_end_timestamp_s(0.0),
    _last_initial_mark_timestamp_s(0.0),
    _last_gc_mixed(false),
    _mixed_gc_finished(false),
    _need_mixed_gc(false),
    _last_initial_mark_interval_s(0.0),
    _last_initial_mark_non_young_bytes(0),
    _last_normalized_eden_consumed_length(0) {
}

void ElasticHeapGCStats::track_gc_start(bool full_gc) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  double now = os::elapsedTime();

  if (full_gc) {
    reset_mixed_gc_info();
  } else {
    if (_g1h->g1_policy()->last_young_gc()) {
      // The last young gc after conc-mark and before mixed gc(if needed)
      // We treat it as part of mixed GCs
      _last_gc_mixed = true;
    } else if (_g1h->g1_policy()->gcs_are_young()) {
      // Regular young GC
      if (_last_gc_mixed) {
        _mixed_gc_finished = true;
        _last_gc_mixed = false;
      }
    } else {
      // Mixed GC
      _last_gc_mixed = true;
    }
  }

  if (_g1h->g1_policy()->during_initial_mark_pause()) {
    _last_initial_mark_interval_s = now - _last_initial_mark_timestamp_s;
    _last_initial_mark_timestamp_s = now;
    _last_initial_mark_non_young_bytes = _g1h->non_young_capacity_bytes();
  }

  double gc_interval_s = now - _last_gc_end_timestamp_s;

  uint available_young_length = _elas->calculate_young_list_desired_max_length() -
                                _elas->overlapped_young_regions_with_old_gen();

  uint eden_consumed_length = _g1h->young_list()->length() - _g1h->young_list()->survivor_length();
  if (eden_consumed_length == 0) {
    // Java threads didn't consumed any eden regions before this gc
    if ((gc_interval_s * MILLIUNITS) > (double)ElasticHeapPeriodicYGCIntervalMillis) {
      // If the interval is long enough, we treat it a long normalized interval
      _recent_ygc_normalized_interval_ms->add(ElasticHeapPeriodicYGCIntervalMillis * GC_INTERVAL_SEQ_LENGTH);
      _last_normalized_eden_consumed_length = available_young_length;
    }
  } else {
    // This interval between 2 GCs will be normalized to full young length
    double normalized_interval = gc_interval_s * (double)available_young_length / (double)eden_consumed_length;
    _recent_ygc_normalized_interval_ms->add(normalized_interval * MILLIUNITS);
    _last_normalized_eden_consumed_length = available_young_length;
  }
}

bool ElasticHeapGCStats::check_mixed_gc_finished() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (!_mixed_gc_finished) {
    return false;
  }
  _mixed_gc_finished = false;
  _need_mixed_gc = false;
  return true;
}

ElasticHeapSetting::ElasticHeapSetting(ElasticHeap* elas, G1CollectedHeap* g1h) :
    _elas(elas),
    _g1h(g1h),
    _young_percent(0),
    _uncommit_ihop(0),
    _softmx_percent(0),
    _evaluating(false) {
}

ElasticHeap::ErrorType ElasticHeapSetting::change_young_percent(uint young_percent) {
  ElasticHeap::ErrorType error_type = ElasticHeap::NoError;
  if (ignore_arg(young_percent, _young_percent)) {
    // Do nothing
  } else if (!((young_percent >= ElasticHeapMinYoungCommitPercent && young_percent <= 100) ||
                young_percent == 0)) {
    error_type = ElasticHeap::IllegalYoungPercent;
  } else if (can_change_young_percent(young_percent)) {
    error_type = ElasticHeap::NoError;
    set_need_gc(true);
    _young_percent = young_percent;
  } else {
    error_type = ElasticHeap::GCTooFrequent;
  }
  return error_type;
}

ElasticHeap::ErrorType ElasticHeapSetting::change_uncommit_ihop(uint uncommit_ihop) {
  if (!ignore_arg(uncommit_ihop, _uncommit_ihop)) {
    _uncommit_ihop = uncommit_ihop;
    _elas->check_to_initate_conc_mark();
    set_need_gc(true);
  }
  return ElasticHeap::NoError;
}

ElasticHeap::ErrorType ElasticHeapSetting::change_softmx_percent(uint softmx_percent, bool fullgc) {
  if (!ignore_arg(softmx_percent, _softmx_percent)) {
    _softmx_percent = softmx_percent;
    // softmx_percent == 0 will recover uncommitted regions in RecoverEvaluator
    set_evaluating(softmx_percent != 0);
    if (!fullgc) {
      _elas->check_to_initate_conc_mark();
    }
    set_need_gc(true);
  }
  return ElasticHeap::NoError;
}

ElasticHeap::EvaluationMode ElasticHeapSetting::target_evaluation_mode(uint young_percent,
                                       uint uncommit_ihop,
                                       uint softmx_percent) {
  assert(!((young_percent != ignore_arg() || uncommit_ihop != ignore_arg()) && softmx_percent != ignore_arg()), "sanity");
  if (young_percent != ignore_arg() || uncommit_ihop != ignore_arg()) {
    return ElasticHeap::GenerationLimitMode;
  } else if (softmx_percent != ignore_arg()) {
    return ElasticHeap::SoftmxMode;
  } else {
    ShouldNotReachHere();
    return ElasticHeap::InactiveMode;
  }
}

ElasticHeap::ErrorType ElasticHeapSetting::process_arg(uint young_percent,
                                                       uint uncommit_ihop,
                                                       uint softmx_percent,
                                                       bool fullgc) {
  if (_elas->conflict_mode(target_evaluation_mode(young_percent, uncommit_ihop, softmx_percent))) {
    return ElasticHeap::IllegalMode;
  }

  ElasticHeap::ErrorType error_type = ElasticHeap::NoError;

  error_type = change_young_percent(young_percent);
  if (error_type != ElasticHeap::NoError) {
    return error_type;
  }

  error_type = change_uncommit_ihop(uncommit_ihop);
  assert(error_type == ElasticHeap::NoError, "sanity");

  error_type = change_softmx_percent(softmx_percent, fullgc);
  assert(error_type == ElasticHeap::NoError, "sanity");

  return ElasticHeap::NoError;
}

bool ElasticHeapSetting::can_change_young_percent(uint percent) {
  if (percent == 0) {
    return true;
  }
  assert(percent >= ElasticHeapMinYoungCommitPercent && percent <= 100, "sanity");
  uint target_max_young_length = _elas->max_young_length() * percent / 100;

  // Check if ihop threashold will overlap young generation
  uint ihop_length = (uint)ceil((double)_g1h->num_regions() * InitiatingHeapOccupancyPercent / 100);
  int overlap_length = ihop_length + _elas->g1_reserve_regions() +
                       _elas->max_young_length() - _g1h->num_regions();
  if (overlap_length > 0) {
    if ((uint)overlap_length >= target_max_young_length) {
      // No available young length at all
      return false;
    }
    target_max_young_length -= (uint)overlap_length;
  }

  double last_interval = _elas->stats()->last_normalized_interval();
  double target_interval = last_interval *
                           (target_max_young_length * SurvivorRatio / (SurvivorRatio + 1)) /
                           _elas->stats()->last_normalized_eden_consumed_length();
  if ((target_max_young_length < _elas->calculate_young_list_desired_max_length()) &&
      (last_interval < ElasticHeapYGCIntervalMinMillis ||
      target_interval < ElasticHeapYGCIntervalMinMillis)) {
    // Last GC interval too small or target GC interval too small
    // We cannot do shrink
    // Expansion is always ok
    return false;
  }
  return true;
}

void ElasticHeapSetting::recover_setting(ElasticHeapSetting *setting) {
  _young_percent = setting->young_percent();
  _uncommit_ihop = setting->uncommit_ihop();
  _softmx_percent = setting->softmx_percent();
  _evaluating = false;
}

ElasticHeap::ElasticHeap(G1CollectedHeap* g1h) : _g1h(g1h),
    _stats(NULL),
    _setting(NULL),
    _orig_ihop(0),
    _orig_min_desired_young_length(0),
    _orig_max_desired_young_length(0),
    _in_conc_cycle(false),
    _conc_thread(NULL),
    _configure_setting_lock(0),
    _heap_capacity_changed(false) {

  _orig_max_desired_young_length = _g1h->g1_policy()->_young_gen_sizer->max_desired_young_length();
  _orig_min_desired_young_length = _g1h->g1_policy()->_young_gen_sizer->min_desired_young_length();
  _orig_ihop = InitiatingHeapOccupancyPercent;

  _conc_thread = new ElasticHeapConcThread(this);
  _conc_thread->start();

  _stats = new ElasticHeapGCStats(this, g1h);
  _setting = new ElasticHeapSetting(this, g1h);

  _evaluators[InactiveMode] = new RecoverEvaluator(this);
  _evaluators[PeriodicUncommitMode] = new PeriodicEvaluator(this);
  _evaluators[GenerationLimitMode] = new GenerationLimitEvaluator(this);
  _evaluators[SoftmxMode] = new SoftmxEvaluator(this);
}

void ElasticHeap::destroy() {
  _conc_thread->stop();
  ElasticHeapTimer::stop();
}

void ElasticHeap::check_to_initate_conc_mark() {
  if (_g1h->concurrent_mark()->cmThread()->during_cycle()) {
    return;
  }
  if (_stats->need_mixed_gc()) {
    // G1 conc-mark cycle already started
    return;
  }
  if (PrintGCDetails && PrintElasticHeapDetails) {
    gclog_or_tty->print_cr("(Elastic Heap triggers G1 concurrent mark)");
  }
  _g1h->g1_policy()->set_initiate_conc_mark_if_possible();
  _stats->set_need_mixed_gc(true);
}

void ElasticHeap::check_to_trigger_ygc() {
  if (evaluation_mode() == InactiveMode) {
    return;
  }

  double now = os::elapsedTime();
  double secs_since_last_gc = now - _stats->last_gc_end_timestamp_s();

  bool trigger_gc = false;
  if (_stats->need_mixed_gc() &&
      (secs_since_last_gc * MILLIUNITS) > ElasticHeapEagerMixedGCIntervalMillis) {
    // If already in conc-mark/mixed gc phase, make sure mixed gc will happen promptly
    trigger_gc = true;
  }
  if (evaluation_mode() == PeriodicUncommitMode) {
    if (ElasticHeapPeriodicYGCIntervalMillis != 0 &&
        ((secs_since_last_gc * MILLIUNITS) > ElasticHeapPeriodicYGCIntervalMillis * 5)) {
      // In PeriodicUncommitMode if GC interval is more than
      // 5 times of ElasticHeapPeriodicYGCIntervalMillis, we will trigger a GC
      trigger_gc = true;
    }
    uint millis_since_last_init_mark = (now - _stats->last_initial_mark_timestamp_s()) * MILLIUNITS;
    if (ElasticHeapPeriodicInitialMarkIntervalMillis != 0 &&
        (millis_since_last_init_mark > ElasticHeapPeriodicInitialMarkIntervalMillis)) {
      // Check if we need to start a conc-mark cycle
      check_to_initate_conc_mark();
      trigger_gc = true;
    }
  }

  if (trigger_gc) {
    // Invoke GC
    Universe::heap()->collect(GCCause::_g1_elastic_heap_gc);
  }
}

uint ElasticHeap::num_unavailable_regions() {
  assert_heap_locked_or_at_safepoint(true /* should_be_vm_thread */);
  return _g1h->_hrm.num_uncommitted_regions() + _conc_thread->total_unavaiable_regions();
}

int ElasticHeap::young_commit_percent() const {
  if (_setting->young_percent_set()) {
    // Specify by jcmd/mxbean
    return _setting->young_percent();
  } else {
    uint uncommitted_young_length = _g1h->_hrm.num_uncommitted_regions() + _conc_thread->uncommit_list()->length();
    return (max_young_length() - uncommitted_young_length) * 100 / max_young_length();
  }
}

jlong ElasticHeap::young_uncommitted_bytes() const {
  uint uncommitted_young_length = _g1h->_hrm.num_uncommitted_regions() + _conc_thread->uncommit_list()->length();
  return (jlong)uncommitted_young_length << HeapRegion::LogOfHRGrainBytes;
}

int ElasticHeap::softmx_percent() const {
  return _g1h->num_regions() * 100 / _g1h->max_regions();
}

jlong ElasticHeap::uncommitted_bytes() const {
  return (jlong)(_g1h->max_regions() - _g1h->num_regions()) * HeapRegion::GrainBytes;
}

int ElasticHeap::uncommit_ihop() const {
  return _setting->uncommit_ihop();
}

uint ElasticHeap::overlapped_young_regions_with_old_gen() {
  assert(Universe::heap()->is_gc_active(), "should only be called during gc");
  assert_at_safepoint(true /* should_be_vm_thread */);

  uint non_young_length = (uint)((double)_g1h->non_young_capacity_bytes() / HeapRegion::GrainBytes + 1);
  int regions = non_young_length + _g1h->g1_policy()->_reserve_regions +
                max_young_length() - _g1h->num_regions();
  if (regions < 0) {
    regions = 0;
  }
  return regions;
}

ElasticHeap::EvaluationMode ElasticHeap::evaluation_mode() const {
  if (_setting->softmx_percent_set()) {
    return SoftmxMode;
  } else if (_setting->generation_limit_set()) {
    return GenerationLimitMode;
  } else if (ElasticHeapPeriodicUncommit &&
             !(ElasticHeapPeriodicYGCIntervalMillis == 0 &&
               ElasticHeapPeriodicInitialMarkIntervalMillis == 0)) {
    return PeriodicUncommitMode;
  } else {
    return InactiveMode;
  }
}

void ElasticHeap::uncommit_region_memory(HeapRegion* hr) {
  _g1h->_hrm.uncommit_region_memory(hr->hrm_index());
}

void ElasticHeap::commit_region_memory(HeapRegion* hr, bool pretouch) {
  _g1h->_hrm.commit_region_memory(hr->hrm_index());
  if (pretouch) {
    os::pretouch_memory((char*)hr->bottom(), (char*)hr->bottom() + HeapRegion::GrainBytes);
  }
}

void ElasticHeap::free_region_memory(HeapRegion* hr) {
  _g1h->_hrm.free_region_memory(hr->hrm_index());
}

bool ElasticHeap::conflict_mode(EvaluationMode target_mode) {
  assert (target_mode != InactiveMode, "sanity");
  EvaluationMode mode = evaluation_mode();
  if (mode == InactiveMode) {
    return false;
  } else {
    return (target_mode != mode);
  }
}

void ElasticHeap::prepare_in_gc_start() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  record_gc_start();
  check_end_of_previous_conc_cycle();
}

void ElasticHeap::record_gc_start(bool full_gc) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  _stats->track_gc_start(full_gc);
}

void ElasticHeap::record_gc_end() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  _stats->set_last_gc_end_timestamp_s(os::elapsedTime());
}

void ElasticHeap::start_conc_cycle() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  guarantee(!in_conc_cycle(), "sanity");
  guarantee(!_conc_thread->working(), "sanity");
  guarantee(_conc_thread->total_unavaiable_regions() != 0, "sanity");
  _in_conc_cycle = true;
}

void ElasticHeap::end_conc_cycle() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  guarantee(in_conc_cycle(), "sanity");
  guarantee(!_conc_thread->working(), "sanity");
  guarantee(_conc_thread->total_unavaiable_regions() == 0, "sanity");
  _in_conc_cycle = false;
}

void ElasticHeap::move_regions_back_to_hrm() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  _conc_thread->sanity_check();

  bool need_update_expanded_size = false;
  if (!_conc_thread->uncommit_list()->is_empty()) {
    _g1h->_hrm.move_to_uncommitted_list(_conc_thread->uncommit_list());
  }

  if (!_conc_thread->commit_list()->is_empty()) {
    _g1h->_hrm.move_to_free_list(_conc_thread->commit_list());
    need_update_expanded_size = true;
  }

  if (!_conc_thread->to_free_list()->is_empty()) {
    _g1h->_hrm.move_to_free_list(_conc_thread->to_free_list());
  }

  if (need_update_expanded_size) {
    update_expanded_heap_size();
  }
}

void ElasticHeap::finish_conc_cycle() {
  assert(!_conc_thread->working(), "Precondition");
  assert_at_safepoint(true /* should_be_vm_thread */);

  move_regions_back_to_hrm();
  end_conc_cycle();

  if (PrintGCDetails && PrintElasticHeapDetails) {
    gclog_or_tty->print("(Elastic Heap concurrent cycle ends)\n");
  }
}

void ElasticHeap::check_end_of_previous_conc_cycle() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (!in_conc_cycle()) {
    // Not in a concurrent cycle, do nothing
    return;
  }

  if (_conc_thread->working()) {
    // ElasticHeapThread is still working on commit/uncommit
    assert(in_conc_cycle(), "sanity");
    return;
  }

  finish_conc_cycle();
}

void ElasticHeap::wait_for_conc_cycle_end() {
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(_g1h->full_collection(), "Precondition");

  if (in_conc_cycle()) {
    wait_for_conc_thread_working_done(false);
    finish_conc_cycle();
  }
}

void ElasticHeap::wait_to_recover() {
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(_g1h->full_collection(), "Precondition");

  wait_for_conc_cycle_end();

  _g1h->_hrm.recover_uncommitted_regions();

  assert(ElasticHeapPeriodicUncommit, "sanity");
  update_desired_young_length(0);

  if (PrintGCDetails && PrintElasticHeapDetails) {
    gclog_or_tty->print_cr("[Elastic Heap recovers]");
  }
}

bool ElasticHeap::is_gc_to_resize_young_gen() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  return Universe::heap()->gc_cause() == GCCause::_g1_elastic_heap_gc && _setting->young_percent_set();
}

bool ElasticHeap::can_turn_on_periodic_uncommit() {
  return !in_conc_cycle() && !_setting->softmx_percent_set() && !_setting->generation_limit_set();
}

void ElasticHeap::update_expanded_heap_size() {
  assert(Universe::heap()->is_gc_active(), "should only be called during gc");
  assert_at_safepoint(true /* should_be_vm_thread */);
  assert(_conc_thread->total_unavaiable_regions() == 0, "sanity");

  if (heap_capacity_changed()) {
    // Complete heap expanding in softmx percent mode
    uint committed_num = _g1h->max_regions() -_g1h->_hrm.num_uncommitted_regions();
    assert(committed_num == _g1h->num_regions(), "sanity");
    change_heap_capacity(committed_num);
  } else {
    update_desired_young_length(_g1h->_hrm.num_uncommitted_regions());
  }
}

void ElasticHeap::perform_after_young_collection() {
  assert(Universe::heap()->is_gc_active(), "should only be called during gc");
  assert_at_safepoint(true /* should_be_vm_thread */);
  _setting->set_need_gc(false);

  if (!in_conc_cycle()) {
    // Do evaluation whether to resize or not, to start the new conc cycle
    evaluate_elastic_work();
  }

  record_gc_end();
}

void ElasticHeap::perform_after_full_collection() {
  assert(Universe::heap()->is_gc_active(), "should only be called during gc");
  assert_at_safepoint(true /* should_be_vm_thread */);
  _setting->set_need_gc(false);

  assert(!in_conc_cycle(), "sanity");
  if (evaluation_mode() == SoftmxMode || evaluation_mode() == InactiveMode) {
    _evaluators[evaluation_mode()]->evaluate();
    _conc_thread->do_memory_job_after_full_collection();
    move_regions_back_to_hrm();
  }

  record_gc_end();
}


void ElasticHeap::evaluate_elastic_work() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  _evaluators[evaluation_mode()]->evaluate();

  try_starting_conc_cycle();
}

void ElasticHeap::update_desired_young_length(uint unavailable_young_length) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  assert(unavailable_young_length ==
         (_g1h->_hrm.num_uncommitted_regions() + _conc_thread->uncommit_list()->length()),
         "sanity");

  uint max_young_length = _orig_max_desired_young_length - unavailable_young_length;
  _g1h->g1_policy()->_young_gen_sizer->resize_max_desired_young_length(max_young_length);
  if (_orig_min_desired_young_length > max_young_length) {
    _g1h->g1_policy()->_young_gen_sizer->resize_min_desired_young_length(max_young_length);
  } else {
    _g1h->g1_policy()->_young_gen_sizer->resize_min_desired_young_length(_orig_min_desired_young_length);
  }
}

void ElasticHeap::set_heap_capacity_changed(uint num) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (num != _g1h->max_regions()) {
    _heap_capacity_changed = true;
    assert(evaluation_mode() == SoftmxMode, "Precondition");
  } else {
    _heap_capacity_changed = false;
  }
}

void ElasticHeap::change_heap_capacity(uint target_heap_regions) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  // Change the heap capacity
  set_heap_capacity_changed(target_heap_regions);
  guarantee(_g1h->num_regions() == target_heap_regions, "sanity");

  // Reset IHOP and young size
  InitiatingHeapOccupancyPercent = _orig_ihop;
  _g1h->g1_policy()->_young_gen_sizer->resize_min_desired_young_length(_orig_min_desired_young_length);
  _g1h->g1_policy()->_young_gen_sizer->resize_max_desired_young_length(_orig_max_desired_young_length);

  _g1h->g1_policy()->record_new_heap_size(target_heap_regions);
  assert(target_heap_regions == _g1h->num_regions(), "sanity");

  if (!heap_capacity_changed()) {
    return;
  }
  // We need to set correct IHOP and young size if heap capacity changed
  uint free_regions = _g1h->num_free_regions();
  // At least 1 reserve region
  uint reserve_regions = MAX2(1U, (uint)(_g1h->num_regions() * g1_reserve_factor()));

  if (free_regions <= reserve_regions) {
    // Actually no free regions
    return;
  }

  free_regions -= reserve_regions;

  // Half of the free bytes are for IHOP and half for young generation
  size_t target_ihop_bytes = _g1h->non_young_capacity_bytes() + free_regions * HeapRegion::GrainBytes / 2;
  assert(target_ihop_bytes < _g1h->capacity(), "sanity");
  InitiatingHeapOccupancyPercent = target_ihop_bytes * 100 / _g1h->capacity();

  assert(InitiatingHeapOccupancyPercent < 100 && InitiatingHeapOccupancyPercent > 0, "sanity");

  // Min young length will be half of max young length
  uint min_desired_young_length = (uint)ceil((double)free_regions / 4);
  uint max_desired_young_length = (uint)ceil((double)free_regions / 2);
  min_desired_young_length = MIN2(min_desired_young_length, _orig_min_desired_young_length);
  max_desired_young_length = MIN2(max_desired_young_length, _orig_max_desired_young_length);
  assert(min_desired_young_length > 0 && max_desired_young_length > 0, "sanity");
  _g1h->g1_policy()->_young_gen_sizer->resize_min_desired_young_length(min_desired_young_length);
  _g1h->g1_policy()->_young_gen_sizer->resize_max_desired_young_length(max_desired_young_length);
}

void ElasticHeap::try_starting_conc_cycle() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (_conc_thread->total_unavaiable_regions() == 0) {
    return;
  }

  {
    MutexLockerEx ml(_conc_thread->conc_lock(), Mutex::_no_safepoint_check_flag);
    start_conc_cycle();
    _conc_thread->set_working();
    // Notify thread to do the work
    _conc_thread->conc_lock()->notify();
  }

  if (PrintGCDetails && PrintElasticHeapDetails) {
    gclog_or_tty->print("(Elastic Heap concurrent cycle starts due to %s)", to_string(evaluation_mode()));
  }
}

bool ElasticHeap::enough_free_regions_to_uncommit(uint num_regions_to_uncommit) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (_g1h->g1_policy()->_free_regions_at_end_of_collection <= _g1h->g1_policy()->_reserve_regions) {
    // Free regions already less than G1 reserve
    return false;
  }
  if (num_regions_to_uncommit >=
      (_g1h->g1_policy()->_free_regions_at_end_of_collection -
       _g1h->g1_policy()->_reserve_regions)) {
    // No enough free regions excluding G1 reserve
    return false;
  }
  return true;
}

void ElasticHeap::resize_young_length(uint target_length) {
  assert(Universe::heap()->is_gc_active(), "should only be called during gc");
  assert_at_safepoint(true /* should_be_vm_thread */);

  uint target_uncommitted_length = max_young_length() - target_length;
  uint old_num_uncommitted = _g1h->_hrm.num_uncommitted_regions();

  if (old_num_uncommitted == target_uncommitted_length) {
    // No more regions need to be commit/uncommit
    return;
  }
  assert(_conc_thread->conc_lock()->owner() == NULL, "Precondition");

  uint new_uncommitted_length = 0;
  if (old_num_uncommitted < target_uncommitted_length) {
    // Shrink young gen
    new_uncommitted_length = target_uncommitted_length - old_num_uncommitted;

    if (!enough_free_regions_to_uncommit(new_uncommitted_length)) {
      if (_setting->young_percent_set()) {
        _setting->set_young_percent((max_young_length() - _g1h->_hrm.num_uncommitted_regions()) * 100 / max_young_length());
      }
      return;
    }
    // Move regions out of free list
    uncommit_regions(new_uncommitted_length);
    update_desired_young_length(target_uncommitted_length);
  } else {
    // Expand young gen
    assert(old_num_uncommitted > target_uncommitted_length, "sanity");
    // Move regions out of uncommitted young list
    commit_regions(old_num_uncommitted - target_uncommitted_length);
  }
}

void ElasticHeap::uncommit_regions(uint num) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  guarantee(_conc_thread->uncommit_list()->is_empty() && _conc_thread->commit_list()->is_empty(), "sanity");

  _g1h->_hrm.prepare_uncommit_regions(_conc_thread->uncommit_list(), num);
}

void ElasticHeap::commit_regions(uint num) {
  assert_at_safepoint(true /* should_be_vm_thread */);

  guarantee(_conc_thread->uncommit_list()->is_empty() && _conc_thread->commit_list()->is_empty(), "sanity");

  _g1h->_hrm.prepare_commit_regions(_conc_thread->commit_list(), num);
}

uint ElasticHeap::max_available_survivor_regions() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  // In elastic heap initiated gc, we need to make sure the survivor after gc less than the target max young length
  uint young_percent = _setting->young_percent();
  if (_setting->young_percent() == 0) {
    return max_young_length();
  } else {
    assert(young_percent >= ElasticHeapMinYoungCommitPercent && young_percent <= 100, "sanity");
    return (young_percent * max_young_length() / 100) - 1;
  }
}

void ElasticHeap::wait_for_conc_thread_working_done(bool safepointCheck) {
  if (_conc_thread->working()) {
    // Get the lock to wait for the finish of memory work
    if (safepointCheck) {
      assert(!Thread::current()->is_VM_thread(), "Should not be called in VM thread, either called from JCMD or MXBean");
      MutexLockerEx x(_conc_thread->working_lock());
      // Wait for conc thread to wake
      while (_conc_thread->working()) {
        _conc_thread->working_lock()->wait();
      }
    } else {
      assert(_g1h->full_collection(), "Precondition");
      assert_at_safepoint(true /* should_be_vm_thread */);
      MutexLockerEx x(_conc_thread->working_lock(), Mutex::_no_safepoint_check_flag);
      // Wait for conc thread to wake
      while (_conc_thread->working()) {
        _conc_thread->working_lock()->wait(Mutex::_no_safepoint_check_flag);
      }
    }
  }
}

uint ElasticHeap::ignore_arg() {
  return ElasticHeapSetting::ignore_arg();
}

class ElasticHeapConfigureSettingLock : public StackObj {
public:
  ElasticHeapConfigureSettingLock(volatile int* lock) : _lock(lock) {
    if (*lock == 1) {
      _locked = false;
    }
    _locked = ((int) Atomic::cmpxchg(1, lock, 0) == 0);
  }
  ~ElasticHeapConfigureSettingLock() {
    if (_locked) {
      *_lock = 0;
    }
  }
  bool locked() { return _locked; }
private:
  volatile int*   _lock;
  bool            _locked;
};

ElasticHeap::ErrorType ElasticHeap::configure_setting(uint young_percent, uint uncommit_ihop, uint softmx_percent, bool fullgc) {
  // Called from Java thread or listener thread
  assert(!Thread::current()->is_VM_thread(), "Should not be called in VM thread, either called from JCMD or MXBean");
  assert(JavaThread::current()->thread_state() == _thread_in_vm, "Should be in VM state");

  ElasticHeapConfigureSettingLock lock(&_configure_setting_lock);
  if (!lock.locked()) {
    return HeapAdjustInProgress;
  }

  if (_conc_thread->working()) {
    // If the memory work is not done, return error
    return HeapAdjustInProgress;
  }

  ElasticHeapSetting orig_setting = *_setting;
  ElasticHeap::ErrorType error_type = _setting->process_arg(young_percent, uncommit_ihop,
                                                            softmx_percent, fullgc);

  if (error_type != NoError) {
    return error_type;
  }

  if (fullgc) {
    assert(setting()->need_gc(), "sanity");
    Universe::heap()->collect(GCCause::_g1_elastic_heap_full_gc);
    if (setting()->need_gc()) {
      _setting->set_need_gc(false);
      _setting->recover_setting(&orig_setting);
      return GCFailure;
    }
    return NoError;
  }

  if (!setting()->need_gc()) {
    return NoError;
  }

  assert(!_conc_thread->working(), "sanity");

  // Invoke GC
  Universe::heap()->collect(GCCause::_g1_elastic_heap_gc);

  // Need to handle the scenario that gc is not successfully triggered
  if (setting()->need_gc()) {
    _setting->set_need_gc(false);
    _setting->recover_setting(&orig_setting);
    return GCFailure;
  }

  wait_for_conc_thread_working_done(true);
  guarantee(!_conc_thread->working(), "sanity");

  return NoError;
}

bool ElasticHeap::ready_to_uncommit_after_mixed_gc() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (!_stats->check_mixed_gc_finished()) {
    return false;
  }

  if ((_stats->last_initial_mark_interval_s() * MILLIUNITS) <
       ElasticHeapInitialMarkIntervalMinMillis) {
    // If last 2 initial marks are too close, do not uncommit memory
    return false;
  }
  return true;
}

void ElasticHeap::uncommit_old_gen() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  uint keep_committed_regions = (uint)ceil((double)_stats->last_initial_mark_non_young_bytes() / HeapRegion::GrainBytes);
  uint cur_non_young_regions = (uint)((double)_g1h->non_young_capacity_bytes() / HeapRegion::GrainBytes);

  if (_setting->uncommit_ihop_set()) {
    keep_committed_regions = (uint)ceil(((double)_g1h->capacity() * _setting->uncommit_ihop() / 100)
                                         / HeapRegion::GrainBytes);
  }

  uint reserve_regions = 0;
  if (keep_committed_regions > cur_non_young_regions) {
    reserve_regions = keep_committed_regions - cur_non_young_regions;
  }

  // Keep some regions committed for old gen to grow
  reserve_regions = MAX2(reserve_regions, (uint)ceil((double)_g1h->num_regions() * ElasticHeapOldGenReservePercent / 100));

  // Free memory of regions in free list than non young part regardless of reserve regions in free list
  _g1h->_hrm.prepare_old_region_list_to_free(_conc_thread->to_free_list(),
        reserve_regions, _g1h->g1_policy()->calculate_young_list_desired_max_length());
}

uint ElasticHeap::calculate_young_list_desired_max_length() {
  return _g1h->g1_policy()->calculate_young_list_desired_max_length();
}

double ElasticHeap::g1_reserve_factor() {
  return _g1h->g1_policy()->_reserve_factor;
}

uint ElasticHeap::g1_reserve_regions() {
  return _g1h->g1_policy()->_reserve_regions;
}

ElasticHeapEvaluator::ElasticHeapEvaluator(ElasticHeap* eh)
  : _elas(eh) {
  _g1h = _elas->_g1h;
  _g1_policy = _g1h->g1_policy();
  _hrm = &_g1h->_hrm;
}

void ElasticHeapEvaluator::sanity_check() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  assert(_g1h->num_regions() == _g1h->max_regions(), "sanity");
  assert(InitiatingHeapOccupancyPercent == _elas->_orig_ihop, "sanity");
  assert(_elas->_orig_max_desired_young_length ==
         _g1_policy->_young_gen_sizer->max_desired_young_length(), "sanity");
  assert(_elas->_orig_min_desired_young_length ==
         _g1_policy->_young_gen_sizer->min_desired_young_length(), "sanity");
}

void ElasticHeapEvaluator::evaluate_old_common() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (ready_to_initial_mark()) {
    _elas->check_to_initate_conc_mark();
  }

  if (!_elas->ready_to_uncommit_after_mixed_gc()) {
    return;
  }

  _elas->uncommit_old_gen();
}

void RecoverEvaluator::evaluate() {
  assert_at_safepoint(true /* should_be_vm_thread */);
  // Sanity check or recover any uncommitted regions
  if (_hrm->num_uncommitted_regions() == 0) {
    sanity_check();
  } else {
    _elas->commit_regions(_hrm->num_uncommitted_regions());
  }
}

void PeriodicEvaluator::evaluate() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (os::elapsedTime() < ElasticHeapPeriodicUncommitStartupDelay) {
    return;
  }

  if (ElasticHeapPeriodicInitialMarkIntervalMillis != 0) {
    evaluate_old();
  }
  evaluate_young();
}

void PeriodicEvaluator::evaluate_young() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  // Do tuning until enough gc info collected
  if (ElasticHeapPeriodicYGCIntervalMillis == 0 || _elas->stats()->num_normalized_interval() < GC_INTERVAL_SEQ_LENGTH) {
    return;
  }

  // Now we caculate the lower bound and higher bound of young gc interval
  uint low_interval = ElasticHeapPeriodicYGCIntervalMillis *
                      (100 - ElasticHeapPeriodicYGCIntervalFloorPercent) / 100;
  uint high_interval = ElasticHeapPeriodicYGCIntervalMillis *
                       (100 + ElasticHeapPeriodicYGCIntervalCeilingPercent) / 100;

  double last_normalized_interval = _elas->stats()->last_normalized_interval();
  double avg_normalized_interval = _elas->stats()->avg_normalized_interval();

  if (last_normalized_interval < high_interval && last_normalized_interval > low_interval) {
    // last interval in proper range so do nothing
    return;
  }

  // We have 3 conditions which will trigger young generation resize
  // 1. last normalized interval less than ElasticHeapYGCIntervalMinMillis
  // 2. average interval of last 10 gc is smaller than lower bound
  // 3. average interval of last 10 gc is larger than higher bound
  bool interval_too_small = last_normalized_interval < ElasticHeapYGCIntervalMinMillis;
  bool avg_interval_larger_than_high = avg_normalized_interval > high_interval;
  bool avg_interval_less_than_low = avg_normalized_interval < low_interval;

  // Calcuate the minimal young list length according to ElasticHeapPeriodicMinYoungCommitPercent
  uint min_young_list_length = _elas->max_young_length() *
                               ElasticHeapPeriodicMinYoungCommitPercent / 100;

  bool continue_resize = avg_interval_larger_than_high &&
                         (_elas->calculate_young_list_desired_max_length()
                           != min_young_list_length);

  continue_resize |= (interval_too_small || avg_interval_less_than_low);

  uint overlap_length = _elas->overlapped_young_regions_with_old_gen();
  if (overlap_length > 0) {
    continue_resize = true;
  }

  if (!continue_resize) {
    return;
  }

  // Calcuate the target max young list length according to last gc interval
  uint target_max_young_list_length = _elas->stats()->last_normalized_eden_consumed_length() *
                                  ElasticHeapPeriodicYGCIntervalMillis / last_normalized_interval
                                  + _g1h->young_list()->length() /* Survivor length after GC */;

  if (overlap_length > 0) {
    // Old region already overlapped young size, need more regions
    target_max_young_list_length += overlap_length;
    min_young_list_length += overlap_length;
  }

  // Minimal young list length should be larger than existent survivor
  min_young_list_length = MAX2(min_young_list_length, _g1h->g1_policy()->recorded_survivor_regions() + 1);

  // Constrain the target_max_young_list_length
  target_max_young_list_length = MAX2(target_max_young_list_length, min_young_list_length);
  target_max_young_list_length = MIN2(target_max_young_list_length, _elas->max_young_length());

  _elas->resize_young_length(target_max_young_list_length);
}

bool PeriodicEvaluator::ready_to_initial_mark() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  return ElasticHeapPeriodicInitialMarkIntervalMillis != 0 &&
         ((os::elapsedTime() - _elas->stats()->last_initial_mark_timestamp_s()) >
          (ElasticHeapPeriodicInitialMarkIntervalMillis / (double)MILLIUNITS));
}

void GenerationLimitEvaluator::evaluate() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  evaluate_old();
  evaluate_young();
}

void GenerationLimitEvaluator::evaluate_young() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  // Calcuate the new target max length according to _young_percent
  uint target_max_young_length;
  if (_elas->setting()->young_percent() == 0) {
    target_max_young_length = _elas->max_young_length();
  } else {
    target_max_young_length = _elas->max_young_length() * _elas->setting()->young_percent() / 100;
  }

  _elas->resize_young_length(target_max_young_length);
}

bool GenerationLimitEvaluator::ready_to_initial_mark() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  if (_elas->setting()->uncommit_ihop() == 0) {
    return false;
  }
  size_t threshold = _g1h->capacity()  * _elas->setting()->uncommit_ihop() / 100;

  if (_g1h->non_young_capacity_bytes() > threshold) {
    return true;
  } else {
    return false;
  }
}

void SoftmxEvaluator::evaluate() {
  assert_at_safepoint(true /* should_be_vm_thread */);

  assert(_elas->setting()->softmx_percent_set(), "Precondition");
  if(!_elas->setting()->evaluating()) {
    return;
  }

  uint target_heap_regions = (uint)ceil((double)_g1h->max_regions() * _elas->setting()->softmx_percent() / 100);

  if (target_heap_regions == _g1h->num_regions()) {
    _elas->setting()->set_evaluating(false);
    return;
  } else if (target_heap_regions > _g1h->num_regions()) {
    // Expand heap
    _elas->setting()->set_evaluating(false);
    uint regions_to_commit = target_heap_regions - _g1h->num_regions();
    _elas->commit_regions(regions_to_commit);
  } else {
    // Shrink heap
    if (!_elas->stats()->check_mixed_gc_finished() && !_g1h->full_collection()) {
      return;
    }
    _elas->setting()->set_evaluating(false);
    target_heap_regions = accommodate_heap_regions(target_heap_regions);
    if (target_heap_regions >= _g1h->num_regions()) {
      return;
    }
    uint regions_to_uncommit = _g1h->num_regions() - target_heap_regions;
    assert(_g1h->num_free_regions() >= regions_to_uncommit, "sanity");

    _elas->uncommit_regions(regions_to_uncommit);
    _elas->change_heap_capacity(target_heap_regions);
  }
}

// Minimal free regions after shrink
uint SoftmxEvaluator::_min_free_region_count = 5;

uint SoftmxEvaluator::accommodate_heap_regions(uint desired_heap_regions) {
  uint old_regions = (uint)ceil((double)_g1h->non_young_capacity_bytes() / HeapRegion::GrainBytes);

  const double min_free_percentage = (double) MinHeapFreeRatio / 100.0;
  const double max_used_percentage = 1.0 - min_free_percentage;
  uint min_regions = (uint)ceil((double)old_regions / max_used_percentage);
  uint target_heap_regions = MAX2(min_regions, old_regions + _min_free_region_count);
  target_heap_regions = MIN2(target_heap_regions, _g1h->max_regions());

  if (target_heap_regions > desired_heap_regions) {
    return target_heap_regions;
  } else {
    return desired_heap_regions;
  }
}
