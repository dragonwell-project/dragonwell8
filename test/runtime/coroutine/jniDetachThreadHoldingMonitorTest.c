/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
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

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


JavaVM *jvm;       /* denotes a Java VM */

jobject lock;
volatile int fooGotLock;

int
main()
{
    JNIEnv *env;
    JavaVMInitArgs vm_args; /* JDK/JRE 6 VM initialization arguments */
    JavaVMOption options[4];
    options[0].optionString = "-XX:+EnableCoroutine";
    options[1].optionString = "-XX:-UseBiasedLocking";
    options[2].optionString = "-Dcom.alibaba.transparentAsync=true";
    options[3].optionString = "-XX:+UseWispMonitor";
    vm_args.version = JNI_VERSION_1_6;
    vm_args.nOptions = 4;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = false;
    /* load and initialize a Java VM, return a JNI interface
     * pointer in env */
    if (JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args) != JNI_OK) {
        exit(-1);
    }

    jclass cls = env->FindClass("java/nio/channels/spi/SelectorProvider");
    printf("class = %p\n", cls);
    jfieldID fid = env->GetStaticFieldID(cls, "lock", "Ljava/lang/Object;");
    printf("fid = %p\n", fid);
    lock = env->GetStaticObjectField(cls, fid);
    printf("lock = %p\n", lock);

    pthread_t tid;
    void *foo(void *arg);
    pthread_create(&tid, NULL, foo, NULL);

    while (!fooGotLock)
        ; // wait

    if (env->MonitorEnter(lock) != JNI_OK) {
        exit(-1);
    }

    return jvm->DestroyJavaVM() == JNI_OK ? 0 : -1;
}


void *foo(void *arg)
{

    JNIEnv *env;
    if (jvm->AttachCurrentThread((void**)&env, NULL) != JNI_OK) {
        exit(-1);
    }

    if (env->MonitorEnter(lock) != JNI_OK) {
        exit(-1);
    }

    fooGotLock = 1;

    usleep(100 * 1000); // 100 ms

    if (jvm->DetachCurrentThread() != JNI_OK) { // unpark main thread
        exit(-1);
    }

    return NULL;
}
