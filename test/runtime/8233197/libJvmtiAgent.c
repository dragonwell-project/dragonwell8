/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jvmti.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JNI_ENV_ARG

#ifdef __cplusplus
#define JNI_ENV_ARG(x, y) y
#define JNI_ENV_PTR(x) x
#else
#define JNI_ENV_ARG(x,y) x, y
#define JNI_ENV_PTR(x) (*x)
#endif

#endif

#define TranslateError(err) "JVMTI error"

static jvmtiEnv *jvmti = NULL;

static jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved);

JNIEXPORT
jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}

JNIEXPORT
jint JNICALL Agent_OnAttach(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}

JNIEXPORT
jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
    return JNI_VERSION_1_8;
}

static void JNICALL
Callback_VMStart(jvmtiEnv *jvmti_env, JNIEnv *env) {
    printf("Localizing jdk.jfr.FlightRecorder\n");
    // without fix for 8233197 the process will crash at the following line
    jclass cls = (*env)->FindClass(env, "jdk/jfr/FlightRecorder");
    jmethodID mid = (*env)->GetStaticMethodID(env, cls, "getFlightRecorder", "()Ljdk/jfr/FlightRecorder;");
    if (mid == 0) {
        printf("Unable to localize jdk.jfr.FlightRecorder#getFlightRecorder() method\n");
        // crash the tested JVM to make the test fail
        exit(-1);
    }
    printf("Going to initialize JFR subsystem ...\n");
    jobject jfr = (*env)->CallStaticObjectMethod(env, cls, mid);

    if (!(*env)->ExceptionCheck(env)) {
        // crash the tested JVM to make the test fail
        printf("JFR subsystem is wrongly initialized too early\n");
        exit(-2);
    }
    // exit VM
    exit(0);
}

static
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jint res, size;
    jvmtiCapabilities caps;
    jvmtiEventCallbacks callbacks;
    jvmtiError err;

    res = JNI_ENV_PTR(jvm)->GetEnv(JNI_ENV_ARG(jvm, (void **) &jvmti),
        JVMTI_VERSION_1_2);
    if (res != JNI_OK || jvmti == NULL) {
        printf("    Error: wrong result of a valid call to GetEnv!\n");
        return JNI_ERR;
    }

    size = (jint)sizeof(callbacks);

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMStart = Callback_VMStart;

    err = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, size);
    if (err != JVMTI_ERROR_NONE) {
        printf("    Error in SetEventCallbacks: %s (%d)\n", TranslateError(err), err);
        return JNI_ERR;
    }

    err = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_START, (jthread)NULL);
    if (err != JVMTI_ERROR_NONE) {
        printf("    Error in SetEventNotificationMode: %s (%d)\n", TranslateError(err), err);
        return JNI_ERR;
    }
    return JNI_OK;
}

#ifdef __cplusplus
}
#endif
