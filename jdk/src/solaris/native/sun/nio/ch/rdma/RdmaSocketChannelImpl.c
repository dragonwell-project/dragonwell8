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

#include "sun_nio_ch_rdma_RdmaSocketChannelImpl.h"
#include "nio_util.h"
#include "nio.h"
#include"Rsocket.h"

static jfieldID fd_fdID;

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketChannelImpl_initIDs(JNIEnv *env,
        jclass clazz) {
    loadRdmaFuncs(env);
    CHECK_NULL(clazz = (*env)->FindClass(env, "java/io/FileDescriptor"));
    CHECK_NULL(fd_fdID = (*env)->GetFieldID(env, clazz, "fd", "I"));
}

jint fdVal(JNIEnv *env, jobject fdo) {
    return (*env)->GetIntField(env, fdo, fd_fdID);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaSocketChannelImpl_checkConnect(JNIEnv *env,
        jobject this, jobject fdo, jboolean block) {
    int error = 0;
    socklen_t n = sizeof(int);
    jint fd = fdVal(env, fdo);
    int result = 0;
    struct pollfd poller;

    poller.fd = fd;
    poller.events = POLLOUT;
    poller.revents = 0;

    result = rs_poll(&poller, 1, block ? -1 : 0);

    if (result < 0) {
        if (errno == EINTR) {
            return IOS_INTERRUPTED;
        } else {
            JNU_ThrowIOExceptionWithLastError(env, "rpoll failed");
            return IOS_THROWN;
        }
    }
    if (!block && (result == 0))
        return IOS_UNAVAILABLE;

    if (result > 0) {
        errno = 0;
        result = rs_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &n);
        if (result < 0) {
            return handleSocketError(env, errno);
        } else if (error) {
            return handleSocketError(env, error);
        } else if ((poller.revents & POLLHUP) != 0) {
            return handleSocketError(env, ENOTCONN);
        }
        // connected
        return 1;
    }
    return 0;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaSocketChannelImpl_sendOutOfBandData(JNIEnv* env,
        jclass this, jobject fdo, jbyte b) {
    int n = rs_send(fdVal(env, fdo), (const void*)&b, 1, MSG_OOB);
    return convertReturnVal(env, n, JNI_FALSE);
}
