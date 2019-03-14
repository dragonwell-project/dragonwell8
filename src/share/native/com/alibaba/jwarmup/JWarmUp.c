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
#include "com_alibaba_jwarmup_JWarmUp.h"

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

static JNINativeMethod jwarmup_methods[] = {
  {"notifyApplicationStartUpIsDone0","()V", (void *)&JVM_NotifyApplicationStartUpIsDone},
  {"checkIfCompilationIsComplete0","()Z", (void *)&JVM_CheckJWarmUpCompilationIsComplete},
  {"notifyJVMDeoptWarmUpMethods0","()V", (void *)&JVM_NotifyJVMDeoptWarmUpMethods}
};

JNIEXPORT void JNICALL
Java_com_alibaba_jwarmup_JWarmUp_registerNatives(JNIEnv *env, jclass cls)
{
    (*env)->RegisterNatives(env, cls, jwarmup_methods, ARRAY_LENGTH(jwarmup_methods));
}
