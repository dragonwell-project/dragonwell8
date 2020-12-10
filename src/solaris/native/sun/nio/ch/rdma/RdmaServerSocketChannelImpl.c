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

#include "sun_nio_ch_rdma_RdmaServerSocketChannelImpl.h"
#include "net_util.h"
#include "nio.h"
#include"Rsocket.h"

static jfieldID fd_fdID;        /* java.io.FileDescriptor.fd */
static jclass isa_class;        /* java.net.InetSocketAddress */
static jmethodID isa_ctorID;    /*   .InetSocketAddress(InetAddress, int) */


JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaServerSocketChannelImpl_initIDs(JNIEnv *env,
        jclass c) {
    loadRdmaFuncs(env);

    jclass cls;

    cls = (*env)->FindClass(env, "java/io/FileDescriptor");
    CHECK_NULL(cls);
    fd_fdID = (*env)->GetFieldID(env, cls, "fd", "I");
    CHECK_NULL(fd_fdID);

    cls = (*env)->FindClass(env, "java/net/InetSocketAddress");
    CHECK_NULL(cls);
    isa_class = (*env)->NewGlobalRef(env, cls);
    if (isa_class == NULL) {
        JNU_ThrowOutOfMemoryError(env, NULL);
        return;
    }
    isa_ctorID = (*env)->GetMethodID(env, cls, "<init>",
                                     "(Ljava/net/InetAddress;I)V");
    CHECK_NULL(isa_ctorID);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaServerSocketChannelImpl_checkAccept(JNIEnv *env,
        jobject this, jobject fdo) {
    jint fd = (*env)->GetIntField(env, fdo, fd_fdID);
    int result = 0;
    struct pollfd poller;

    poller.fd = fd;
    poller.events = POLLIN;
    poller.revents = 0;

    result = rs_poll(&poller, 1, -1);

    if (result < 0) {
        if (errno == EINTR) {
            return IOS_INTERRUPTED;
        } else {
            JNU_ThrowIOExceptionWithLastError(env, "rpoll failed");
            return IOS_THROWN;
        }
    }

    if (poller.revents & poller.events) {
        return 1;
    }
    return 0;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaServerSocketChannelImpl_accept0(JNIEnv *env,
        jobject this, jobject ssfdo, jobject newfdo, jobjectArray isaa) {
    jint ssfd = (*env)->GetIntField(env, ssfdo, fd_fdID);
    jint newfd;
    struct sockaddr sa;
    socklen_t sa_len = sizeof(struct sockaddr);
    jobject remote_ia = 0;
    jobject isa;
    jint remote_port = 0;

    /*
     * accept connection but ignore ECONNABORTED indicating that
     * a connection was eagerly accepted but was reset before
     * accept() was called.
     */
    for (;;) {
        newfd = rs_accept(ssfd, &sa, &sa_len);
        if (newfd >= 0) {
            break;
        }
        if (errno != ECONNABORTED) {
            break;
        }
        /* ECONNABORTED => restart accept */
    }

    if (newfd < 0) {
        if (errno == EAGAIN)
            return IOS_UNAVAILABLE;
        if (errno == EINTR)
            return IOS_INTERRUPTED;
        JNU_ThrowIOExceptionWithLastError(env, "Accept failed");
        return IOS_THROWN;
    }

    (*env)->SetIntField(env, newfdo, fd_fdID, newfd);
    remote_ia = NET_SockaddrToInetAddress(env, &sa, (int *)&remote_port);
    CHECK_NULL_RETURN(remote_ia, IOS_THROWN);
    isa = (*env)->NewObject(env, isa_class, isa_ctorID,
        remote_ia, remote_port);
    CHECK_NULL_RETURN(isa, IOS_THROWN);
    (*env)->SetObjectArrayElement(env, isaa, 0, isa);
    return 1;
}
