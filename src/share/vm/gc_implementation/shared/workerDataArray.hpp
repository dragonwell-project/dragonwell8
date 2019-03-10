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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_HPP

#include "memory/allocation.hpp"
#include "utilities/debug.hpp"

// These classes (WorkerDataArray and LineBuffer) are moved here from the original gc_implementation/g1/g1GCPhaseTimes.cpp
// And can cache the data for the parallel gc workers' performance data in G1GC and ParNew + CMS GC

template <class T>
class WorkerDataArray  : public CHeapObj<mtGC> {
  friend class G1GCParPhasePrinter;
  friend class GenGCParPhasePrinter;
  T*          _data;
  uint        _length;
  const char* _title;
  bool        _print_sum;
  int         _log_level;
  uint        _indent_level;
  bool        _enabled;

  WorkerDataArray<size_t>* _thread_work_items;

  NOT_PRODUCT(T uninitialized();)

  // We are caching the sum and average to only have to calculate them once.
  // This is not done in an MT-safe way. It is intended to allow single
  // threaded code to call sum() and average() multiple times in any order
  // without having to worry about the cost.
  bool   _has_new_data;
  T      _sum;
  T      _min;
  T      _max;
  double _average;

 public:
  WorkerDataArray(uint length, const char* title, bool print_sum, int log_level, uint indent_level);

  ~WorkerDataArray();

  void link_thread_work_items(WorkerDataArray<size_t>* thread_work_items);

  WorkerDataArray<size_t>* thread_work_items() { return _thread_work_items; }

  void set(uint worker_i, T value);

  void set_thread_work_item(uint worker_i, size_t value);

  T get(uint worker_i);

  void add(uint worker_i, T value);

  double average(uint active_threads);

  T sum(uint active_threads);

  T minimum(uint active_threads);

  T maximum(uint active_threads);

  void reset() PRODUCT_RETURN;
  void verify(uint active_threads) PRODUCT_RETURN;

  void set_enabled(bool enabled) { _enabled = enabled; }

  int log_level() { return _log_level;  }

 private:

  void calculate_totals(uint active_threads);
};

//=========================================================================
// Helper class for avoiding interleaved logging
class LineBuffer: public StackObj {

private:
  static const int BUFFER_LEN = 1024;
  static const int INDENT_CHARS = 3;
  char _buffer[BUFFER_LEN];
  int _indent_level;
  int _cur;

  void vappend(const char* format, va_list ap)  ATTRIBUTE_PRINTF(2, 0) {
    int res = vsnprintf(&_buffer[_cur], BUFFER_LEN - _cur, format, ap);
    if (res != -1) {
      _cur += res;
    } else {
      DEBUG_ONLY(warning("buffer too small in LineBuffer");)
      _buffer[BUFFER_LEN -1] = 0;
      _cur = BUFFER_LEN; // vsnprintf above should not add to _buffer if we are called again
    }
  }

public:
  explicit LineBuffer(int indent_level): _indent_level(indent_level), _cur(0) {
    for (; (_cur < BUFFER_LEN && _cur < (_indent_level * INDENT_CHARS)); _cur++) {
      _buffer[_cur] = ' ';
    }
  }
 
#ifndef PRODUCT
  ~LineBuffer() {
    assert(_cur == _indent_level * INDENT_CHARS, "pending data in buffer - append_and_print_cr() not called?");
  }
#endif

  void append(const char* format, ...)  ATTRIBUTE_PRINTF(2, 3) {
    va_list ap;
    va_start(ap, format);
    vappend(format, ap);
    va_end(ap);
  }

  void print_cr() {
    gclog_or_tty->print_cr("%s", _buffer);
    _cur = _indent_level * INDENT_CHARS;
  }

  void append_and_print_cr(const char* format, ...)  ATTRIBUTE_PRINTF(2, 3) {
    va_list ap;
    va_start(ap, format);
    vappend(format, ap);
    va_end(ap);
    print_cr();
  }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_WORKERDATAARRAY_HPP
