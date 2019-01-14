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

#include "precompiled.hpp"
#include "classfile/javaClasses.hpp"
#include "jfr/recorder/access/jfrOptionSet.hpp"
#include "jfr/recorder/checkpoint/constant/traceid/jfrTraceIdEpoch.hpp"
#include "jfr/recorder/storage/jfrMemorySpace.inline.hpp"
#include "jfr/recorder/repository/jfrChunkWriter.hpp"
#include "jfr/recorder/storage/jfrStorageUtils.inline.hpp"
#include "jfr/recorder/stringpool/jfrStringPool.hpp"
#include "jfr/recorder/stringpool/jfrStringPoolWriter.hpp"
#include "jfr/utilities/jfrLog.hpp"
#include "runtime/atomic.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/thread.inline.hpp"
#include "tracefiles/traceTypes.hpp"

typedef JfrStringPool::Buffer* BufferPtr;

static JfrStringPool* _instance = NULL;

JfrStringPool& JfrStringPool::instance() {
  return *_instance;
}

JfrStringPool* JfrStringPool::create(JfrChunkWriter& cw) {
  assert(_instance == NULL, "invariant");
  _instance = new JfrStringPool(cw);
  return _instance;
}

void JfrStringPool::destroy() {
  assert(_instance != NULL, "invariant");
  delete _instance;
  _instance = NULL;
}

JfrStringPool::JfrStringPool(JfrChunkWriter& cw) : _free_list_mspace(NULL), _transient_mspace(NULL), _lock(NULL), _chunkwriter(cw) {}

JfrStringPool::~JfrStringPool() {
  if (_free_list_mspace != NULL) {
    delete _free_list_mspace;
  }
  if (_transient_mspace != NULL) {
    delete _transient_mspace;
  }
  if (_lock != NULL) {
    delete _lock;
  }
}

static const size_t unlimited_mspace_size = 0;
static const size_t string_pool_cache_count = 1;
static const size_t string_pool_buffer_size = M;

static JfrStringPoolMspace* create_mspace(size_t buffer_size, size_t limit, size_t cache_count, JfrStringPool* system) {
  JfrStringPoolMspace* mspace = new JfrStringPoolMspace(buffer_size, limit, cache_count, system);
  if (mspace != NULL) {
    mspace->initialize();
  }
  return mspace;
}

bool JfrStringPool::initialize() {
  assert(_free_list_mspace == NULL, "invariant");
  _free_list_mspace = create_mspace(string_pool_buffer_size, unlimited_mspace_size, string_pool_cache_count, this);
  if (_free_list_mspace == NULL) {
    return false;
  }
  _transient_mspace = create_mspace((size_t)JfrOptionSet::global_buffer_size(), unlimited_mspace_size, 0, this);
  if (_transient_mspace == NULL) {
    return false;
  }
  assert(_lock == NULL, "invariant");
  _lock = new Mutex(Monitor::leaf - 1, "Checkpoint mutex", Mutex::_allow_vm_block_flag);
  return _lock != NULL;
}

/*
* 1. If the buffer was a "lease" from the global system, release back.
* 2. If the buffer is transient (temporal dynamically allocated), retire and release.
*
* The buffer is effectively invalidated for the thread post-return,
* and the caller should take means to ensure that it is not referenced any longer.
*/
static void release(BufferPtr buffer, Thread* thread) {
  assert(buffer != NULL, "invariant");
  assert(buffer->lease(), "invariant");
  assert(buffer->acquired_by_self(), "invariant");
  buffer->clear_lease();
  if (buffer->transient()) {
    buffer->set_retired();
  }
  buffer->release();
}

BufferPtr JfrStringPool::flush(BufferPtr old, size_t used, size_t requested, Thread* thread) {
  assert(old != NULL, "invariant");
  assert(old->lease(), "invariant");
  if (0 == requested) {
    // indicates a lease is being returned
    release(old, thread);
    return NULL;
  }
  // migration of in-flight information
  BufferPtr const new_buffer = lease_buffer(thread, used + requested);
  if (new_buffer != NULL) {
    migrate_outstanding_writes(old, new_buffer, used, requested);
  }
  release(old, thread);
  return new_buffer; // might be NULL
}

static BufferPtr lease_transient(size_t size, JfrStringPoolMspace* transient_mspace, Thread* thread) {
  BufferPtr buffer = mspace_allocate_transient_lease_to_full(size, transient_mspace, thread);
  if (buffer == NULL) {
    log_warning(jfr)("Unable to allocate " SIZE_FORMAT " bytes of transient memory.", size);
    return NULL;
  }
  assert(buffer->acquired_by_self(), "invariant");
  assert(buffer->transient(), "invariant");
  assert(buffer->lease(), "invariant");
  return buffer;
}

static const size_t lease_retry = 10;

static BufferPtr lease_free(size_t size, JfrStringPoolMspace* mspace, Thread* thread) {
  return mspace_get_free_lease_with_retry(size, mspace, lease_retry, thread);
}

BufferPtr JfrStringPool::lease_buffer(Thread* thread, size_t size /* 0 */) {
  static const size_t max_elem_size = instance()._free_list_mspace->min_elem_size(); // min is max
  if (size <= max_elem_size) {
    BufferPtr const buffer = lease_free(size, instance()._free_list_mspace, thread);
    if (buffer != NULL) {
      assert(buffer->acquired_by_self(), "invariant");
      assert(!buffer->transient(), "invariant");
      assert(buffer->lease(), "invariant");
      return buffer;
    }
  }
  return lease_transient(size, instance()._transient_mspace, thread);
}

bool JfrStringPool::add(bool epoch, jlong id, jstring string, JavaThread* jt) {
  assert(jt != NULL, "invariant");
  const bool current_epoch = JfrTraceIdEpoch::epoch();
  if (current_epoch == epoch) {
    JfrStringPoolWriter writer(jt);
    writer.write(id);
    writer.write(string);
    writer.inc_nof_strings();
  }
  return current_epoch;
}

class StringPoolWriteOp  {
 public:
  typedef JfrStringPoolBuffer Type;
 private:
  UnBufferedWriteToChunk<Type> _writer;
  Thread* _thread;
  size_t _strings_processed;
 public:
  StringPoolWriteOp(JfrChunkWriter& writer, Thread* thread) : _writer(writer), _thread(thread), _strings_processed(0) {}
  bool write(Type* buffer, const u1* data, size_t size) {
    buffer->acquire(_thread); // blocking
    const u4 nof_strings_used = (const u4)buffer->string_count();
    assert(nof_strings_used > 0, "invariant");
    buffer->set_string_top(buffer->string_top() + nof_strings_used);
    // "size processed" for string pool buffers is the number of processed string elements
    _strings_processed += nof_strings_used;
    const bool ret = _writer.write(buffer, data, size);
    buffer->release();
    return ret;
  }
  size_t processed() { return _strings_processed; }
};

typedef StringPoolWriteOp WriteOperation;
typedef ConcurrentWriteOp<WriteOperation> ConcurrentWriteOperation;
typedef MutexedReleaseOp<JfrStringPoolMspace> StringPoolReleaseOperation;
typedef CompositeOperation<ConcurrentWriteOperation, StringPoolReleaseOperation> StringPoolWriteOperation;

size_t JfrStringPool::write() {
  Thread* const thread = Thread::current();
  WriteOperation wo(_chunkwriter, thread);
  ConcurrentWriteOperation cwo(wo);
  StringPoolReleaseOperation spro(_free_list_mspace, thread);
  StringPoolWriteOperation spwo(&cwo, &spro);
  assert(_free_list_mspace->is_full_empty(), "invariant");
  process_free_list(spwo, _free_list_mspace);
  StringPoolReleaseOperation transient_release(_transient_mspace, thread);
  StringPoolWriteOperation transient_writer(&cwo, &transient_release);
  assert(_transient_mspace->is_free_empty(), "invariant");
  process_full_list(transient_writer, _transient_mspace);
  return wo.processed();
}

class StringPoolBufferDiscarder {
 private:
  Thread* _thread;
  size_t _processed;
 public:
  typedef JfrStringPoolBuffer Type;
  StringPoolBufferDiscarder() : _thread(Thread::current()), _processed(0) {}
  bool discard(Type* buffer, const u1* data, size_t size) {
    buffer->acquire(_thread); // serialized access
    const u4 nof_strings_used = buffer->string_count();
    buffer->set_string_top(buffer->string_top() + nof_strings_used);
    // "size processed" for string pool buffers is the number of string elements
    _processed += nof_strings_used;
    buffer->release();
    return true;
  }
  size_t processed() const { return _processed; }
};

typedef DiscardOp<StringPoolBufferDiscarder> DiscardOperation;
size_t JfrStringPool::clear() {
  DiscardOperation discard_operation(concurrent); // concurrent discard mode
  assert(_free_list_mspace->is_full_empty(), "invariant");
  assert(_transient_mspace->is_free_empty(), "invariant");
  process_full_list(discard_operation, _transient_mspace);
  process_free_list(discard_operation, _free_list_mspace);
  return discard_operation.processed();
}

void JfrStringPool::register_full(BufferPtr t, Thread* thread) {
  // nothing here at the moment
  assert(t->retired(), "invariant");
}

void JfrStringPool::lock() {
  assert(!_lock->owned_by_self(), "invariant");
  _lock->lock_without_safepoint_check();
}

void JfrStringPool::unlock() {
  _lock->unlock();
}
