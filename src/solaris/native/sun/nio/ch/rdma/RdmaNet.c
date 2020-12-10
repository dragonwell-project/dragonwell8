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

#include "net_util.h"
#include "nio.h"
#include "nio_util.h"
#include "rdma_util_md.h"
#include "Rsocket.h"
#include "sun_nio_ch_Net.h"

static jfieldID fd_fdID;

JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_rdma_RdmaNet_isRdmaAvailable0(JNIEnv *env,
        jclass clazz) {
    return rdma_supported();
}

static int
configureBlocking(int fd, jboolean blocking) {
    int flags = rs_fcntl(fd, F_GETFL);
    int newflags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);

    return (flags == newflags) ? 0 : rs_fcntl(fd, F_SETFL, newflags);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_configureBlocking(JNIEnv *env, jclass clazz,
        jobject fdo, jboolean blocking) {
    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    if (configureBlocking(fd, blocking) < 0)
        JNU_ThrowIOExceptionWithLastError(env, "Configure blocking failed");
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_initIDs(JNIEnv *env, jclass clazz) {
    loadRdmaFuncs(env);
    CHECK_NULL(clazz = (*env)->FindClass(env, "java/io/FileDescriptor"));
    CHECK_NULL(fd_fdID = (*env)->GetFieldID(env, clazz, "fd", "I"));
    initInetAddressIDs(env);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaNet_socket0(JNIEnv *env, jclass clazz,
        jboolean preferIPv6, jboolean stream, jboolean reuse) {
    int fd;
    int type = (stream ? SOCK_STREAM : SOCK_DGRAM);
    int domain = (ipv6_available() && preferIPv6) ? AF_INET6 : AF_INET;

    fd = rs_socket(domain, type, 0);

    rs_fcntl(fd, F_SETFL, rs_fcntl(fd, F_GETFL) | O_NONBLOCK);

    if (fd < 0) {
        return handleSocketError(env, errno);
    }

    if (reuse) {
        int arg = 1;
        if (rs_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&arg,
                       sizeof(arg)) < 0) {
            JNU_ThrowByNameWithLastError(env,
                    JNU_JAVANETPKG "SocketException",
                    "Unable to set SO_REUSEADDR");
            rs_close(fd);
            return -1;
        }
    }
    return fd;
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_bind0(JNIEnv *env, jclass clazz,
        jobject fdo, jboolean preferIPv6, jobject iao, int port) {
    struct sockaddr sa;
    int sa_len = 0;
    int rv = 0;

    if (NET_InetAddressToSockaddr(env, iao, port, &sa, &sa_len,
            preferIPv6) != 0) {
        return;
    }

    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    rv = RDMA_Bind(fd, &sa, sa_len);
    if (rv != 0) {
        handleSocketError(env, errno);
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_listen(JNIEnv *env, jclass clazz,
        jobject fdo, jint backlog) {
    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    if (rs_listen(fd, backlog) < 0)
       handleSocketError(env, errno);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaNet_connect0(JNIEnv *env, jclass clazz,
        jboolean preferIPv6, jobject fdo, jobject iao, jint port) {
    struct sockaddr sa;
    int sa_len = 0;
    int rv;

    if (NET_InetAddressToSockaddr(env, iao, port, &sa, &sa_len,
            preferIPv6) != 0) {
        return IOS_THROWN;
    }

    int fd = (*env)->GetIntField(env, fdo, fd_fdID);

    rv = rs_connect(fd, &sa, sa_len);

    if (rv != 0) {
        if (errno == EINPROGRESS) {
            return IOS_UNAVAILABLE;
        } else if (errno == EINTR) {
            return IOS_INTERRUPTED;
        }
        return handleSocketError(env, errno);
    }
    return 1;
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaNet_localPort(JNIEnv *env, jclass clazz,
        jobject fdo) {
    struct sockaddr sa;
    socklen_t sa_len = sizeof(struct sockaddr);
    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    if (rs_getsockname(fd, &sa, &sa_len) < 0) {
        handleSocketError(env, errno);
        return -1;
    }
    return NET_GetPortFromSockaddr(&sa);
}

JNIEXPORT jobject JNICALL
Java_sun_nio_ch_rdma_RdmaNet_localInetAddress(JNIEnv *env, jclass clazz,
        jobject fdo) {
    struct sockaddr sa;
    socklen_t sa_len = sizeof(struct sockaddr);
    int port;
    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    if (rs_getsockname(fd, &sa, &sa_len) < 0) {
        handleSocketError(env, errno);
        return NULL;
    }
    return NET_SockaddrToInetAddress(env, &sa, &port);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaNet_getIntOption0(JNIEnv *env, jclass clazz,
        jobject fdo, jboolean mayNeedConversion, jint level, jint opt) {
    int result;
    void *arg;
    socklen_t arglen;
    int n;

    arg = (void *)&result;
    arglen = sizeof(result);

    int fd = (*env)->GetIntField(env, fdo, fd_fdID);

    if (mayNeedConversion) {
        n = RDMA_GetSockOpt(fd, level, opt, arg, (int*)&arglen);
    } else {
        n = rs_getsockopt(fd, level, opt, arg, &arglen);
    }

    if (n < 0) {
        JNU_ThrowByNameWithLastError(env,
                JNU_JAVANETPKG "SocketException",
                "jdk.internal.net.rdma.RdmaNet.getIntOption");
        return -1;
    }

    return (jint)result;
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_setIntOption0(JNIEnv *env, jclass clazz,
        jobject fdo, jboolean mayNeedConversion, jint level, jint opt,
        jint arg) {
    int result;
    void *parg;
    socklen_t arglen;
    int n;

    parg = (void*)&arg;
    arglen = sizeof(arg);

    int fd = (*env)->GetIntField(env, fdo, fd_fdID);

    if (mayNeedConversion) {
        n = RDMA_SetSockOpt(fd, level, opt, parg, arglen);
    } else {
        n = rs_setsockopt(fd, level, opt, parg, arglen);
    }
    if (n < 0) {
        JNU_ThrowByNameWithLastError(env,
                JNU_JAVANETPKG "SocketException",
                "jdk.internal.net.rdma.RdmaNet.setIntOption");
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaNet_shutdown(JNIEnv *env, jclass clazz,
        jobject fdo, jint jhow) {
    int how = (jhow == sun_nio_ch_Net_SHUT_RD) ? SHUT_RD :
        (jhow == sun_nio_ch_Net_SHUT_WR) ? SHUT_WR : SHUT_RDWR;
    int fd = (*env)->GetIntField(env, fdo, fd_fdID);
    if ((rs_shutdown(fd, how) < 0) && (errno != ENOTCONN))
        handleSocketError(env, errno);
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaNet_poll(JNIEnv* env, jclass clazz,
        jobject fdo, jint events, jlong timeout) {
    struct pollfd pfd;
    int rv;
    pfd.fd = (*env)->GetIntField(env, fdo, fd_fdID);
    pfd.events = events;
    if (timeout < -1) {
        timeout = -1;
    } else if (timeout > INT_MAX) {
        timeout = INT_MAX;
    }
    rv = rs_poll(&pfd, 1, (int)timeout);

    if (rv >= 0) {
        return pfd.revents;
    } else if (errno == EINTR) {
        // interrupted, no events to return
        return 0;
    } else {
        handleSocketError(env, errno);
        return IOS_THROWN;
    }
}

jint handleSocketError(JNIEnv *env, jint errorValue) {
    char *xn;
    switch (errorValue) {
        case EINPROGRESS:       /* Non-blocking connect */
            return 0;
#ifdef EPROTO
        case EPROTO:
            xn = JNU_JAVANETPKG "ProtocolException";
            break;
#endif
        case ECONNREFUSED:
        case ETIMEDOUT:
        case ENOTCONN:
            xn = JNU_JAVANETPKG "ConnectException";
            break;

        case EHOSTUNREACH:
            xn = JNU_JAVANETPKG "NoRouteToHostException";
            break;
        case EADDRINUSE:  /* Fall through */
        case EADDRNOTAVAIL:
            xn = JNU_JAVANETPKG "BindException";
            break;
        default:
            xn = JNU_JAVANETPKG "SocketException";
            break;
    }
    errno = errorValue;
    JNU_ThrowByNameWithLastError(env, xn, "NioSocketError");
    return IOS_THROWN;
}
