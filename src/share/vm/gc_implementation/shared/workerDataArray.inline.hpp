/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_INLINE_HPP

#include "gc_implementation/shared/workerDataArray.hpp"
#include "memory/allocation.hpp"
#include "utilities/debug.hpp"
#include "runtime/os.hpp"
#include "utilities/ostream.hpp"

template <class T>
WorkerDataArray<T>::WorkerDataArray(uint length, const char* title, bool print_sum, int log_level, uint indent_level):
  _title(title), _length(0), _print_sum(print_sum), _log_level(log_level), _indent_level(indent_level),
  _has_new_data(true), _thread_work_items(NULL), _enabled(true) {
  assert(length > 0, "Must have some workers to store data for");
  _length = length;
  _data = NEW_C_HEAP_ARRAY(T, _length, mtGC);
}

template <class T>
WorkerDataArray<T>::~WorkerDataArray() {
  FREE_C_HEAP_ARRAY(T, _data, mtGC);
}

template <class T>
void WorkerDataArray<T>::link_thread_work_items(WorkerDataArray<size_t>* thread_work_items) {
  _thread_work_items = thread_work_items;
}

template <class T>
void WorkerDataArray<T>::set(uint worker_i, T value) {
  assert(worker_i < _length, err_msg("Worker %d is greater than max: %d", worker_i, _length));
  assert(_data[worker_i] == WorkerDataArray<T>::uninitialized(), err_msg("Overwriting data for worker %d in %s", worker_i, _title));
  _data[worker_i] = value;
  _has_new_data = true;
}

template <class T>
void WorkerDataArray<T>::set_thread_work_item(uint worker_i, size_t value) {
  assert(_thread_work_items != NULL, "No sub count");
  _thread_work_items->set(worker_i, value);
}

template <class T>
T WorkerDataArray<T>::get(uint worker_i) {
  assert(worker_i < _length, err_msg("Worker %d is greater than max: %d", worker_i, _length));
  assert(_data[worker_i] != WorkerDataArray<T>::uninitialized(), err_msg("No data added for worker %d", worker_i));
  return _data[worker_i];
}

template <class T>
void WorkerDataArray<T>::add(uint worker_i, T value) {
  assert(worker_i < _length, err_msg("Worker %d is greater than max: %d", worker_i, _length));
  assert(_data[worker_i] != WorkerDataArray<T>::uninitialized(), err_msg("No data to add to for worker %d", worker_i));
  _data[worker_i] += value;
  _has_new_data = true;
}

template <class T>
double WorkerDataArray<T>::average(uint active_threads){
  calculate_totals(active_threads);
  return _average;
}

template <class T>
T WorkerDataArray<T>::sum(uint active_threads) {
  calculate_totals(active_threads);
  return _sum;
}

template <class T>
T WorkerDataArray<T>::minimum(uint active_threads) {
  calculate_totals(active_threads);
  return _min;
}

template <class T>
T WorkerDataArray<T>::maximum(uint active_threads) {
  calculate_totals(active_threads);
  return _max;
}

template <class T>
void WorkerDataArray<T>::calculate_totals(uint active_threads){
  if (!_has_new_data) {
    return;
  }

  _sum = (T)0;
  _min = _data[0];
  _max = _min;
  assert(active_threads <= _length, "Wrong number of active threads");
  for (uint i = 0; i < active_threads; ++i) {
    T val = _data[i];
    _sum += val;
    _min = MIN2(_min, val);
    _max = MAX2(_max, val);
  }
  _average = (double)_sum / (double)active_threads;
  _has_new_data = false;
}

#ifndef PRODUCT

template <class T>
void WorkerDataArray<T>::reset() {
  for (uint i = 0; i < _length; i++) {
    _data[i] = WorkerDataArray<T>::uninitialized();
  }
  if (_thread_work_items != NULL) {
    _thread_work_items->reset();
  }
}

template <class T>
void WorkerDataArray<T>::verify(uint active_threads) {
  if (!_enabled) {
    return;
  }

  assert(active_threads <= _length, "Wrong number of active threads");
  for (uint i = 0; i < active_threads; i++) {
    assert(_data[i] != WorkerDataArray<T>::uninitialized(),
        err_msg("Invalid data for worker %u in '%s'", i, _title));
  }
  if (_thread_work_items != NULL) {
    _thread_work_items->verify(active_threads);
  }
}
#endif // PRODUCT
#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_INLINE_HPP
