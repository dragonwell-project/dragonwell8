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
#include "jfr/recorder/checkpoint/jfrCheckpointWriter.hpp"
#include "jfr/recorder/checkpoint/constant/jfrConstantManager.hpp"
#include "jfr/recorder/checkpoint/constant/jfrConstant.hpp"
#include "jfr/utilities/jfrIterator.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/exceptions.hpp"

JfrSerializerRegistration::JfrSerializerRegistration(JfrConstantTypeId id, bool permit_cache, JfrConstantSerializer* cs) :
  _next(NULL),
  _prev(NULL),
  _serializer(cs),
  _cache(),
  _id(id),
  _permit_cache(permit_cache) {}

JfrSerializerRegistration::~JfrSerializerRegistration() {
  delete _serializer;
}

JfrSerializerRegistration* JfrSerializerRegistration::next() const {
  return _next;
}

void JfrSerializerRegistration::set_next(JfrSerializerRegistration* next) {
  _next = next;
}

JfrSerializerRegistration* JfrSerializerRegistration::prev() const {
  return _prev;
}

void JfrSerializerRegistration::set_prev(JfrSerializerRegistration* prev) {
  _prev = prev;
}

JfrConstantTypeId JfrSerializerRegistration::id() const {
  return _id;
}

void JfrSerializerRegistration::invoke_serializer(JfrCheckpointWriter& writer) const {
  if (_cache.valid()) {
    writer.increment();
    _cache->write(writer);
    return;
  }
  const JfrCheckpointContext ctx = writer.context();
  writer.write_constant_type(_id);
  _serializer->write_constants(writer);
  if (_permit_cache) {
    _cache = writer.copy(&ctx);
  }
}

JfrConstantManager::~JfrConstantManager() {
  Iterator iter(_constants);
  JfrSerializerRegistration* registration;
  while (iter.has_next()) {
    registration = _constants.remove(iter.next());
    assert(registration != NULL, "invariant");
    delete registration;
  }
  Iterator sp_type_iter(_safepoint_constants);
  while (sp_type_iter.has_next()) {
    registration = _safepoint_constants.remove(sp_type_iter.next());
    assert(registration != NULL, "invariant");
    delete registration;
  }
}

size_t JfrConstantManager::number_of_registered_constant_types() const {
  size_t count = 0;
  const Iterator iter(_constants);
  while (iter.has_next()) {
    ++count;
    iter.next();
  }
  const Iterator sp_type_iter(_safepoint_constants);
  while (sp_type_iter.has_next()) {
    ++count;
    sp_type_iter.next();
  }
  return count;
}

void JfrConstantManager::write_constants(JfrCheckpointWriter& writer) const {
  const Iterator iter(_constants);
  while (iter.has_next()) {
    iter.next()->invoke_serializer(writer);
  }
}

void JfrConstantManager::write_safepoint_constants(JfrCheckpointWriter& writer) const {
  assert(SafepointSynchronize::is_at_safepoint(), "invariant");
  const Iterator iter(_safepoint_constants);
  while (iter.has_next()) {
    iter.next()->invoke_serializer(writer);
  }
}

void JfrConstantManager::write_constant_tag_set() const {
  assert(!SafepointSynchronize::is_at_safepoint(), "invariant");
  // can safepoint here because of PackageTable_lock
  MutexLockerEx lock(PackageTable_lock);
  JfrCheckpointWriter writer(true, true, Thread::current());
  ConstantSet constant_set;
  constant_set.write_constants(writer);
}

void JfrConstantManager::write_constant_tag_set_for_unloaded_classes() const {
  assert(SafepointSynchronize::is_at_safepoint(), "invariant");
  JfrCheckpointWriter writer(false, true, Thread::current());
  ClassUnloadConstantSet class_unload_constant_set;
  class_unload_constant_set.write_constants(writer);
}

void JfrConstantManager::create_thread_checkpoint(JavaThread* jt) const {
  assert(jt != NULL, "invariant");
  JfrThreadConstant constant_type_thread(jt);
  JfrCheckpointWriter writer(false, true, jt);
  writer.write_constant_type(CONSTANT_TYPE_THREAD);
  constant_type_thread.write_constants(writer);
  // create and install a checkpoint blob
  jt->trace_data()->set_thread_checkpoint(writer.checkpoint_blob());
  assert(jt->trace_data()->has_thread_checkpoint(), "invariant");
}

void JfrConstantManager::write_thread_checkpoint(JavaThread* jt) const {
  assert(jt != NULL, "JavaThread is NULL!");
  ResourceMark rm(jt);
  if (jt->trace_data()->has_thread_checkpoint()) {
    JfrCheckpointWriter writer(false, false, jt);
    jt->trace_data()->thread_checkpoint()->write(writer);
  } else {
    JfrThreadConstant constant_type_thread(jt);
    JfrCheckpointWriter writer(false, true, jt);
    writer.write_constant_type(CONSTANT_TYPE_THREAD);
    constant_type_thread.write_constants(writer);
  }
}

#ifdef ASSERT
static void assert_not_registered_twice(JfrConstantTypeId id, JfrConstantManager::List& list) {
  const JfrConstantManager::Iterator iter(list);
  while (iter.has_next()) {
    assert(iter.next()->id() != id, "invariant");
  }
}
#endif

bool JfrConstantManager::register_serializer(JfrConstantTypeId id, bool require_safepoint, bool permit_cache, JfrConstantSerializer* cs) {
  assert(cs != NULL, "invariant");
  JfrSerializerRegistration* const registration = new JfrSerializerRegistration(id, permit_cache, cs);
  if (registration == NULL) {
    delete cs;
    return false;
  }
  if (require_safepoint) {
    assert(!_safepoint_constants.in_list(registration), "invariant");
    DEBUG_ONLY(assert_not_registered_twice(id, _safepoint_constants);)
      _safepoint_constants.prepend(registration);
  }
  else {
    assert(!_constants.in_list(registration), "invariant");
    DEBUG_ONLY(assert_not_registered_twice(id, _constants);)
      _constants.prepend(registration);
  }
  return true;
}

bool JfrConstantManager::initialize() {
  // non-safepointing serializers
  for (size_t i = 0; i < 16; ++i) {
    switch (i) {
    case 0: register_serializer(CONSTANT_TYPE_FLAGVALUEORIGIN, false, true, new FlagValueOriginConstant()); break;
    case 1: break;
    case 2: register_serializer(CONSTANT_TYPE_GCCAUSE, false, true, new GCCauseConstant()); break;
    case 3: register_serializer(CONSTANT_TYPE_GCNAME, false, true, new GCNameConstant()); break;
    case 4: register_serializer(CONSTANT_TYPE_GCWHEN, false, true, new GCWhenConstant()); break;
    case 5: register_serializer(CONSTANT_TYPE_G1HEAPREGIONTYPE, false, true, new G1HeapRegionTypeConstant()); break;
    case 6: register_serializer(CONSTANT_TYPE_GCTHRESHOLDUPDATER, false, true, new GCThresholdUpdaterConstant()); break;
    case 7: register_serializer(CONSTANT_TYPE_METADATATYPE, false, true, new MetadataTypeConstant()); break;
    case 8: register_serializer(CONSTANT_TYPE_METASPACEOBJTYPE, false, true, new MetaspaceObjTypeConstant()); break;
    case 9: register_serializer(CONSTANT_TYPE_G1YCTYPE, false, true, new G1YCTypeConstant()); break;
    case 10: register_serializer(CONSTANT_TYPE_REFERENCETYPE, false, true, new ReferenceTypeConstant()); break;
    case 11: register_serializer(CONSTANT_TYPE_NARROWOOPMODE, false, true, new NarrowOopModeConstant()); break;
    case 12: register_serializer(CONSTANT_TYPE_COMPILERPHASETYPE, false, true, new CompilerPhaseTypeConstant()); break;
    case 13: register_serializer(CONSTANT_TYPE_CODEBLOBTYPE, false, true, new CodeBlobTypeConstant()); break;
    case 14: register_serializer(CONSTANT_TYPE_VMOPERATIONTYPE, false, true, new VMOperationTypeConstant()); break;
    case 15: register_serializer(CONSTANT_TYPE_THREADSTATE, false, true, new ThreadStateConstant()); break;
    default:
      guarantee(false, "invariant");
    }
  }

  // safepointing serializers
  for (size_t i = 0; i < 2; ++i) {
    switch (i) {
    case 0: register_serializer(CONSTANT_TYPE_THREADGROUP, true, false, new JfrThreadGroupConstant()); break;
    case 1: register_serializer(CONSTANT_TYPE_THREAD, true, false, new JfrThreadConstantSet()); break;
    default:
      guarantee(false, "invariant");
    }
  }
  return true;
}


