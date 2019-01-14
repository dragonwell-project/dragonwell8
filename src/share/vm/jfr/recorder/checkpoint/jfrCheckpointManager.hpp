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

#ifndef SHARE_VM_JFR_RECORDER_CHECKPOINT_JFRCHECKPOINTMANAGER_HPP
#define SHARE_VM_JFR_RECORDER_CHECKPOINT_JFRCHECKPOINTMANAGER_HPP

#include "jfr/recorder/storage/jfrBuffer.hpp"
#include "jfr/recorder/storage/jfrMemorySpace.hpp"
#include "jfr/recorder/storage/jfrMemorySpaceRetrieval.hpp"
#include "tracefiles/traceTypes.hpp"

class JfrCheckpointManager;
class JfrChunkWriter;
class JfrConstantManager;
class JfrConstantSerializer;
class Mutex;
class Thread;

struct JfrCheckpointEntry {
  jlong size;
  jlong start_time;
  jlong duration;
  juint flushpoint;
  juint nof_segments;
};

typedef JfrMemorySpace<JfrBuffer, JfrMspaceSequentialRetrieval, JfrCheckpointManager> JfrCheckpointMspace;

//
// Responsible for maintaining checkpoints and by implication constants.
// A checkpoint is an event that has a payload consisting of constant types.
// A constant type is a relation, a set of key-value pairs.
//
class JfrCheckpointManager : public JfrCHeapObj {
 public:
  typedef JfrCheckpointMspace::Type Buffer;
  typedef JfrCheckpointMspace::List List;
 private:
  List _epoch_transition_list;
  JfrCheckpointMspace* _free_list_mspace;
  JfrCheckpointMspace* _transient_mspace;
  Mutex* _lock;
  JfrConstantManager* _constant_manager;
  const Thread* _service_thread;
  JfrChunkWriter& _chunkwriter;
  bool _checkpoint_epoch_state;

  // mspace callback
  void register_full(Buffer* t, Thread* thread);
  void lock();
  void unlock();

  static Buffer* lease_buffer(Thread* t, size_t size = 0);
  Buffer* lease_transient(size_t size, Thread* t);
  static Buffer* flush(Buffer* old, size_t used, size_t requested, Thread* t);

  size_t clear();
  size_t write();
  size_t write_epoch_transition_list();
  size_t write_constant_types();
  size_t write_safepoint_constant_types();
  void write_constant_tag_set();
  void shift_epoch();
  void synchronize_epoch();
  bool use_epoch_transition_list(const Thread* t) const;

  JfrCheckpointManager(JfrChunkWriter& cw);
  ~JfrCheckpointManager();

  static JfrCheckpointManager &instance();
  static JfrCheckpointManager* create(JfrChunkWriter& cw);
  bool initialize();
  static void destroy();

  static bool register_serializer(JfrConstantTypeId id, bool require_safepoint, bool permit_cache, JfrConstantSerializer* serializer);

 public:
  void register_service_thread(const Thread* t);
  static void write_constant_tag_set_for_unloaded_classes();
  static void create_thread_checkpoint(JavaThread* jt);
  static void write_thread_checkpoint(JavaThread* jt);

  friend class JfrRecorder;
  friend class JfrRecorderService;
  friend class JfrCheckpointFlush;
  friend class JfrCheckpointWriter;
  friend class JfrConstantSerializer;
  friend class JfrStackTraceRepository;
  template <typename, template <typename> class, typename>
  friend class JfrMemorySpace;
};

#endif //SHARE_VM_JFR_RECORDER_CHECKPOINT_JFRCHECKPOINTMANAGER_HPP
