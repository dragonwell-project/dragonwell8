/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

/*
 * Test if rsocket is available
 */
#include <stdlib.h>
#ifdef __linux__
#include <dlfcn.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#endif
#include "jni.h"

jfieldID fid;

/*
 * Class:     RsocketTest
 * Method:    isRsocketAvailable0
 * Signature: ()Z
 */
JNIEXPORT jboolean
Java_RsocketTest_isRsocketAvailable0(JNIEnv *env, jclass cls) {
    jboolean result = JNI_FALSE;
#ifdef __linux__
    void *handle;
    int (*rs)(int, int, int);
    char str[74];
    strcpy(str, "librdmacm.so.1: cannot open shared object file: No such file or directory");
    fid = (*env)->GetStaticFieldID(env, cls , "libInstalled", "Z");

    handle = dlopen("librdmacm.so.1", RTLD_NOW);
    if (!handle) {
        int ret = strncmp(str, dlerror(), 74);
        if (ret == 0) {
            (*env)->SetStaticBooleanField(env, cls, fid, JNI_FALSE);
        }
        return JNI_FALSE;
    } else {
        (*env)->SetStaticBooleanField(env, cls, fid, JNI_TRUE);
    }
    rs = dlsym(handle, "rsocket");
    if (!rs)
        return JNI_FALSE;
    if ((*rs)(AF_INET, SOCK_STREAM, 0) > 0)
        result = JNI_TRUE;
    dlclose(handle);
#endif
    return result;
}
