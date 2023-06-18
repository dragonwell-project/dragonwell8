/*
 * Copyright (c) 2003, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_UTILITIES_HASHTABLE_INLINE_HPP
#define SHARE_VM_UTILITIES_HASHTABLE_INLINE_HPP

#include "memory/allocation.inline.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "utilities/hashtable.hpp"
#include "utilities/dtrace.hpp"

// Inline function definitions for hashtable.hpp.

// --------------------------------------------------------------------------

// Initialize a table.

template <MEMFLAGS F> inline BasicHashtable<F>::BasicHashtable(int table_size, int entry_size) {
  // Called on startup, no locking needed
  initialize(table_size, entry_size, 0);
  _buckets = NEW_C_HEAP_ARRAY2(HashtableBucket<F>, table_size, F, CURRENT_PC);
  for (int index = 0; index < _table_size; index++) {
    _buckets[index].clear();
  }
}


template <MEMFLAGS F> inline BasicHashtable<F>::BasicHashtable(int table_size, int entry_size,
                                      HashtableBucket<F>* buckets,
                                      int number_of_entries) {
  // Called on startup, no locking needed
  initialize(table_size, entry_size, number_of_entries);
  _buckets = buckets;
}


template <MEMFLAGS F> inline void BasicHashtable<F>::initialize(int table_size, int entry_size,
                                       int number_of_entries) {
  // Called on startup, no locking needed
  _table_size = table_size;
  _entry_size = entry_size;
  _free_list = NULL;
  _first_free_entry = NULL;
  _end_block = NULL;
  _number_of_entries = number_of_entries;
  _memory_blocks = NULL;
#ifdef ASSERT
  _lookup_count = 0;
  _lookup_length = 0;
#endif
}


// The following method is MT-safe and may be used with caution.
template <MEMFLAGS F> inline BasicHashtableEntry<F>* BasicHashtable<F>::bucket(int i) const {
  return _buckets[i].get_entry();
}


template <MEMFLAGS F> inline void HashtableBucket<F>::set_entry(BasicHashtableEntry<F>* l) {
  // Warning: Preserve store ordering.  The SystemDictionary is read
  //          without locks.  The new SystemDictionaryEntry must be
  //          complete before other threads can be allowed to see it
  //          via a store to _buckets[index].
  OrderAccess::release_store_ptr(&_entry, l);
}


template <MEMFLAGS F> inline BasicHashtableEntry<F>* HashtableBucket<F>::get_entry() const {
  // Warning: Preserve load ordering.  The SystemDictionary is read
  //          without locks.  The new SystemDictionaryEntry must be
  //          complete before other threads can be allowed to see it
  //          via a store to _buckets[index].
  return (BasicHashtableEntry<F>*) OrderAccess::load_ptr_acquire(&_entry);
}


template <MEMFLAGS F> inline void BasicHashtable<F>::set_entry(int index, BasicHashtableEntry<F>* entry) {
  _buckets[index].set_entry(entry);
}


template <MEMFLAGS F> inline void BasicHashtable<F>::add_entry(int index, BasicHashtableEntry<F>* entry) {
  entry->set_next(bucket(index));
  _buckets[index].set_entry(entry);
  ++_number_of_entries;
}

template <MEMFLAGS F> inline void BasicHashtable<F>::free_entry(BasicHashtableEntry<F>* entry) {
  entry->set_next(_free_list);
  _free_list = entry;
  --_number_of_entries;
}

#endif // SHARE_VM_UTILITIES_HASHTABLE_INLINE_HPP
