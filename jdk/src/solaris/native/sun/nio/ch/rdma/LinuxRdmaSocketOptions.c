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


#include "sun_nio_ch_rdma_LinuxRdmaSocketOptions.h"
#include "rdma_util_md.h"
#include "Rsocket.h"
#include "jni_util.h"

void throwByNameWithLastError(JNIEnv *env, const char *name,
        const char *defaultDetail) {
  char defaultMsg[255];
  sprintf(defaultMsg, "errno: %d, %s", errno, defaultDetail);
  JNU_ThrowByNameWithLastError(env, name, defaultMsg);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_LinuxRdmaSocketOptions_init(JNIEnv *env, jclass c) {
   loadRdmaFuncs(env);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_LinuxRdmaSocketOptions_setSockOpt(JNIEnv *env,
        jobject unused, jint fd, jint opt, jint value) {
    int optname;
    int level = SOL_RDMA;
    RDMA_MapSocketOption(opt, &level, &optname);
    int len = sizeof(value);
    int rv = RDMA_SetSockOpt(fd, level, optname, (char*)&value, len);
    if (rv < 0) {
        if (errno == ENOPROTOOPT) {
            JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                            "unsupported RDMA socket option");
        } else if (errno == EACCES || errno == EPERM) {
            JNU_ThrowByName(env, "java/net/SocketException", "Permission denied");
        } else {
            throwByNameWithLastError(env, "java/net/SocketException",
                                     "set RDMA option failed");
        }
    }
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_LinuxRdmaSocketOptions_getSockOpt(JNIEnv *env,
        jobject unused, jint fd, jint opt) {
    int optname;
    int level = SOL_RDMA;
    RDMA_MapSocketOption(opt, &level, &optname);
    int value;
    int len = sizeof(value);
    int rv = RDMA_GetSockOpt(fd, level, optname, (char*)&value, &len);

    if (rv < 0) {
        if (errno == ENOPROTOOPT) {
            JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                            "unsupported RDMA socket option");
        } else if (errno == EACCES || errno == EPERM) {
            JNU_ThrowByName(env, "java/net/SocketException", "Permission denied");
        } else {
            throwByNameWithLastError(env, "java/net/SocketException",
                                     "get RDMA option failed");
        }
        return -1;
    }
    return value;
}

JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_rdma_LinuxRdmaSocketOptions_rdmaSocketSupported(JNIEnv *env,
        jobject unused) {
    int rv, s;

    s = rs_socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return JNI_FALSE;
    }
    rs_close(s);
    return JNI_TRUE;
}
