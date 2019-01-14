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

#ifndef SHARE_VM_JFR_CHECKPOINT_CONSTANT_JFRCONSTANT_HPP
#define SHARE_VM_JFR_CHECKPOINT_CONSTANT_JFRCONSTANT_HPP

#include "jfr/metadata/jfrConstantSerializer.hpp"

class JfrThreadConstantSet : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class JfrThreadGroupConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class ClassUnloadConstantSet : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class FlagValueOriginConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class GCCauseConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class GCNameConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class GCWhenConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class G1HeapRegionTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class GCThresholdUpdaterConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class MetadataTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class MetaspaceObjTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class G1YCTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class ReferenceTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class NarrowOopModeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class CompilerPhaseTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class CodeBlobTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class VMOperationTypeConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class ConstantSet : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class ThreadStateConstant : public JfrConstantSerializer {
 public:
  void write_constants(JfrCheckpointWriter& writer);
};

class JfrThreadConstant : public JfrConstantSerializer {
 private:
  JavaThread* _thread;
 public:
  JfrThreadConstant(JavaThread* jt) : _thread(jt) {}
  void write_constants(JfrCheckpointWriter& writer);
};

#endif // SHARE_VM_JFR_CHECKPOINT_CONSTANT_JFRCONSTANT_HPP
