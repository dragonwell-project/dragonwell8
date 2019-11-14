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
 *
 */
#ifndef SHARE_VM_JFR_OBJECTROFILER_OBJECTPROFILER_HPP
#define SHARE_VM_JFR_OBJECTROFILER_OBJECTPROFILER_HPP

#include "prims/jni.h"

class ObjectProfiler : public AllStatic {
 private:
  static volatile jint _enabled;
  static bool _sample_instance_obj_alloc;
  static bool _sample_array_obj_alloc;
#ifndef PRODUCT
  static volatile int _try_lock;
#endif

 public:
  static void start(jlong event_id);
  static void stop(jlong event_id);
  static jint enabled();
  static void* enabled_flag_address();
};

#endif // SHARE_VM_JFR_OBJECTROFILER_OBJECTPROFILER_HPP
