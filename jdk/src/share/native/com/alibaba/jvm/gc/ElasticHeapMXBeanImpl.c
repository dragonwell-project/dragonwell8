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

#include "jni.h"
#include "jvm.h"
#include "com_alibaba_jvm_gc_ElasticHeapMXBeanImpl.h"

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

static JNINativeMethod methods[] = {
  {"getEvaluationModeImpl",             "()I",  (void *)&JVM_ElasticHeapGetEvaluationMode},
  {"setYoungGenCommitPercentImpl",      "(I)V", (void *)&JVM_ElasticHeapSetYoungGenCommitPercent},
  {"getYoungGenCommitPercentImpl",      "()I",  (void *)&JVM_ElasticHeapGetYoungGenCommitPercent},
  {"setUncommitIHOPImpl",               "(I)V", (void *)&JVM_ElasticHeapSetUncommitIHOP},
  {"getUncommitIHOPImpl",               "()I",  (void *)&JVM_ElasticHeapGetUncommitIHOP},
  {"getTotalYoungUncommittedBytesImpl", "()J",  (void *)&JVM_ElasticHeapGetTotalYoungUncommittedBytes},
  {"setSoftmxPercentImpl",              "(I)V", (void *)&JVM_ElasticHeapSetSoftmxPercent},
  {"getSoftmxPercentImpl",              "()I",  (void *)&JVM_ElasticHeapGetSoftmxPercent},
  {"getTotalUncommittedBytesImpl",      "()J",  (void *)&JVM_ElasticHeapGetTotalUncommittedBytes},
};

JNIEXPORT void JNICALL
Java_com_alibaba_jvm_gc_ElasticHeapMXBeanImpl_registerNatives(JNIEnv *env, jclass cls)
{
  (*env)->RegisterNatives(env, cls, methods, ARRAY_LENGTH(methods));
}
