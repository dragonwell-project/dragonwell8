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

#ifndef SHARE_VM_JFR_METADATA_JFRCONSTANTSERIALIZER_HPP
#define SHARE_VM_JFR_METADATA_JFRCONSTANTSERIALIZER_HPP

#include "memory/allocation.hpp"
#include "jfr/recorder/checkpoint/jfrCheckpointWriter.hpp"
#include "tracefiles/traceTypes.hpp"

/*
 * A JfrConstant (type) is a relation defined by enumerating a set of <key, value> ordered pairs:
 *
 * { <1, "my_first_constant">, <2, "my_second_constant">, ... }
 *
 * Write the key into event fields and the framework will maintain the mapping (if you register as below).
 *
 * In order to support mapping of constants, we use an interface called JfrConstantSerializer.
 * Inherit JfrConstantSerializer, create a CHeapObj instance and use JfrConstantSerializer::register_serializer(...) to register.
 * Once registered, the ownership of the serializer instance is transferred to Jfr.
 *
 * How to register:
 *
 * bool register_serializer(JfrConstantTypeId id, bool require_safepoint, bool permit_cache, JfrConstantSerializer* serializer)
 *
 * The constant types are machine generated into an enum located in tracefiles/traceTypes.hpp (included).
 *
 *  enum JfrConstantTypeId {
 *    ...
 *    CONSTANT_TYPE_THREADGROUP,
 *    CONSTANT_TYPE_CLASSLOADER,
 *    CONSTANT_TYPE_METHOD,
 *    CONSTANT_TYPE_SYMBOL,
 *    CONSTANT_TYPE_THREADSTATE,
 *    CONSTANT_TYPE_INFLATECAUSE,
 *    ...
 *
 * id                 this is the id of the constant type your are defining (see the enum above).
 * require_safepoint  indicate if your constants need to be evaluated and written under a safepoint.
 * permit_cache       indicate if your constants are stable to be cached.
 *                    (implies the callback is invoked only once and the contents will be cached. Set this to true for static information).
 * serializer         the serializer instance you define.
 *
 * See below for guidance about how to implement write_constants().
 *
 */
class JfrConstantSerializer : public CHeapObj<mtTracing> {
 public:
  virtual void write_constants(JfrCheckpointWriter& writer) = 0;
  virtual ~JfrConstantSerializer() {}
  static bool register_serializer(JfrConstantTypeId id, bool require_safepoint, bool permit_cache, JfrConstantSerializer* serializer);
};

/*
 *  Invoke writer.write_number_of_constants(num) to define the total number of constant mappings.
 *
 *  You then write the individual constants as ordered pairs, <key, value> ...
 *
 *  Here is an example:
 *
 *  void MyConstant::write_constants(JfrCheckpointWriter& writer) {
 *    const int nof_causes = ObjectSynchronizer::inflate_cause_nof;
 *    writer.write_number_of_constants(nof_causes);             // write number of constant (mappings) to follow
 *    for (int i = 0; i < nof_causes; i++) {
 *      writer.write_key(i);                                    // write key
 *      writer.write(ObjectSynchronizer::inflate_cause_name((ObjectSynchronizer::InflateCause)i)); // write value
 *    }
 *  }
 *
 * Please see jfr/recorder/checkpoint/constant/jfrConstant.cpp for reference.
 */

#endif // SHARE_VM_JFR_METADATA_JFRCONSTANTSERIALIZER_HPP
