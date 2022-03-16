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

#include "java_net_SocketOptions.h"
#include "sun_nio_ch_rdma_RdmaSocketImpl.h"
#include "jvm.h"
#include "net_util.h"
#include "rdma_util_md.h"
#include "Rsocket.h"

/************************************************************************
 * RdmaSocketImpl
 */

static jfieldID IO_fd_fdID;

jfieldID psi_fdID;
jfieldID psi_addressID;
jfieldID psi_ipaddressID;
jfieldID psi_portID;
jfieldID psi_localportID;
jfieldID psi_timeoutID;
jfieldID psi_trafficClassID;
jfieldID psi_serverSocketID;
jfieldID psi_fdLockID;
jfieldID psi_closePendingID;

#define SET_NONBLOCKING(fd) {           \
        int flags = rs_fcntl(fd, F_GETFL); \
        flags |= O_NONBLOCK;            \
        rs_fcntl(fd, F_SETFL, flags);      \
}

#define SET_BLOCKING(fd) {              \
        int flags = rs_fcntl(fd, F_GETFL); \
        flags &= ~O_NONBLOCK;           \
        rs_fcntl(fd, F_SETFL, flags);      \
}

static jclass socketExceptionCls;

static int getFD(JNIEnv *env, jobject this) {
    jobject fdObj = (*env)->GetObjectField(env, this, psi_fdID);
    CHECK_NULL_RETURN(fdObj, -1);
    return (*env)->GetIntField(env, fdObj, IO_fd_fdID);
}


jfieldID
NET_GetFileDescriptorID(JNIEnv *env) {
    jclass cls = (*env)->FindClass(env, "java/io/FileDescriptor");
    CHECK_NULL_RETURN(cls, NULL);
    return (*env)->GetFieldID(env, cls, "fd", "I");
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_initProto(JNIEnv *env,
        jclass cls) {
    loadRdmaFuncs(env);

    jclass clazz = (*env)->FindClass(env,
            "jdk/internal/net/rdma/RdmaSocketImpl");
    psi_fdID = (*env)->GetFieldID(env, clazz , "fd",
            "Ljava/io/FileDescriptor;");
    CHECK_NULL(psi_fdID);
    psi_addressID = (*env)->GetFieldID(env, clazz, "address",
            "Ljava/net/InetAddress;");
    CHECK_NULL(psi_addressID);
    psi_timeoutID = (*env)->GetFieldID(env, clazz, "timeout", "I");
    CHECK_NULL(psi_timeoutID);
    psi_trafficClassID = (*env)->GetFieldID(env, clazz, "trafficClass", "I");
    CHECK_NULL(psi_trafficClassID);
    psi_portID = (*env)->GetFieldID(env, clazz, "port", "I");
    CHECK_NULL(psi_portID);
    psi_localportID = (*env)->GetFieldID(env, clazz, "localport", "I");
    CHECK_NULL(psi_localportID);
    psi_serverSocketID = (*env)->GetFieldID(env, clazz, "serverSocket",
                        "Ljava/net/ServerSocket;");
    CHECK_NULL(psi_serverSocketID);

    IO_fd_fdID = NET_GetFileDescriptorID(env);
    CHECK_NULL(IO_fd_fdID);

    initInetAddressIDs(env);
    JNU_CHECK_EXCEPTION(env);
}

JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_isRdmaAvailable0(JNIEnv *env,
        jclass cls) {
    return rdma_supported();
}

void
NET_SetTrafficClass(struct sockaddr *sa, int trafficClass) {
#ifdef AF_INET6
    if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
        sa6->sin6_flowinfo = htonl((trafficClass & 0xff) << 20);
    }
#endif /* AF_INET6 */
}

/*
 * Class:     jdk_net_RdmaSocketImpl
 * Method:    rdmaSocketCreate
 * Signature: (Z)V */
JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketCreate(JNIEnv *env,
        jclass cls, jboolean preferIPv6, jboolean stream) {
    jobject fdObj, ssObj;
    int fd;
    int type = (stream ? SOCK_STREAM : SOCK_DGRAM);
    int domain = (ipv6_available() && preferIPv6) ? AF_INET6 : AF_INET;

    if (socketExceptionCls == NULL) {
        jclass c = (*env)->FindClass(env, "java/net/SocketException");
        CHECK_NULL(c);
        socketExceptionCls = (jclass)(*env)->NewGlobalRef(env, c);
        CHECK_NULL(socketExceptionCls);
    }

    fdObj = (*env)->GetObjectField(env, cls, psi_fdID);

    if (fdObj == NULL) {
        (*env)->ThrowNew(env, socketExceptionCls, "null fd object");
        return;
    }

    if ((fd = rs_socket(domain, type, 0)) == -1) {
        NET_ThrowNew(env, errno, "can't create socket");
        return;
    }
    if (domain == AF_INET6) {
        int arg = 0;
        if (rs_setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&arg,
                       sizeof(int)) < 0) {
            NET_ThrowNew(env, errno, "cannot set IPPROTO_IPV6");
            rs_close(fd);
            return;
        }
    }

    ssObj = (*env)->GetObjectField(env, cls, psi_serverSocketID);
    if (ssObj != NULL) {
        int arg = 1;
        SET_NONBLOCKING(fd);
        if (RDMA_SetSockOpt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&arg,
                       sizeof(arg)) < 0) {
            NET_ThrowNew(env, errno, "cannot set SO_REUSEADDR");
            rs_close(fd);
            return;
        }
    }
    (*env)->SetIntField(env, fdObj, IO_fd_fdID, fd);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketConnect(JNIEnv *env,
        jclass cls, jboolean preferIPv6, jobject iaObj, jint port,
        jint timeout) {
    jint localport = (*env)->GetIntField(env, cls, psi_localportID);
    int len = 0;
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);

    jobject fdLock;

    jint trafficClass = (*env)->GetIntField(env, cls, psi_trafficClassID);

    jint fd;

    struct sockaddr sa;
    int connect_rv = -1;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException", "Socket closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }
    if (IS_NULL(iaObj)) {
        JNU_ThrowNullPointerException(env, "inet address argument null.");
        return;
    }

    if (NET_InetAddressToSockaddr(env, iaObj, port, &sa, &len,
            preferIPv6) != 0) {
        return;
    }

    if (trafficClass != 0 && ipv6_available()) {
        NET_SetTrafficClass(&sa, trafficClass);
    }

    if (timeout <= 0) {
        connect_rv = RDMA_Connect(fd, &sa, len);
    } else {
        SET_NONBLOCKING(fd);

        connect_rv = rs_connect(fd, &sa, len);

        if (connect_rv != 0) {
            socklen_t optlen;
            jlong nanoTimeout = (jlong) timeout * NET_NSEC_PER_MSEC;
            jlong prevNanoTime = JVM_NanoTime(env, 0);

            if (errno != EINPROGRESS) {
                JNU_ThrowByNameWithMessageAndLastError(env,
                        JNU_JAVANETPKG "ConnectException", "connect failed");
                SET_BLOCKING(fd);
                return;
            }

            while (1) {
                jlong newNanoTime;
                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = POLLOUT;

                errno = 0;

                connect_rv = RDMA_Poll(&pfd, 1, nanoTimeout / NET_NSEC_PER_MSEC);

                if (connect_rv >= 0) {
                    break;
                }
                if (errno != EINTR) {
                    break;
                }

                newNanoTime = JVM_NanoTime(env, 0);
                nanoTimeout -= (newNanoTime - prevNanoTime);
                if (nanoTimeout < NET_NSEC_PER_MSEC) {
                    connect_rv = 0;
                    break;
                }
                prevNanoTime = newNanoTime;

            }

            if (connect_rv == 0) {
                JNU_ThrowByName(env, JNU_JAVANETPKG "SocketTimeoutException",
                        "connect timed out");

                SET_BLOCKING(fd);
                rs_shutdown(fd, 2);
                return;
            }

            optlen = sizeof(connect_rv);
            if (rs_getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)&connect_rv,
                            &optlen) <0) {
                connect_rv = errno;
            }
        }

        SET_BLOCKING(fd);

        if (connect_rv != 0) {
            errno = connect_rv;
            connect_rv = -1;
        }
    }

    if (connect_rv < 0) {
        if (connect_rv == -1 && errno == EINVAL) {
            JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "Invalid argument or cannot assign requested address");
            return;
        }
#if defined(EPROTO)
        if (errno == EPROTO) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "ProtocolException", "Protocol error");
            return;
        }
#endif
        if (errno == ECONNREFUSED) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "ConnectException", "Connection refused");
        } else if (errno == ETIMEDOUT) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "ConnectException", "Connection timed out");
        } else if (errno == EHOSTUNREACH) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "NoRouteToHostException", "Host unreachable");
        } else if (errno == EADDRNOTAVAIL) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "NoRouteToHostException",
                    "Address not available");
        } else if ((errno == EISCONN) || (errno == EBADF)) {
            JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                    "Socket closed");
        } else {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "SocketException", "connect failed");
        }
        return;
    }

    (*env)->SetIntField(env, fdObj, IO_fd_fdID, fd);

    (*env)->SetObjectField(env, cls, psi_addressID, iaObj);
    (*env)->SetIntField(env, cls, psi_portID, port);

    if (localport == 0) {
        socklen_t slen = sizeof(struct sockaddr);
        if (rs_getsockname(fd, &sa, &slen) == -1) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "SocketException",
                    "Error getting socket name");
        } else {
            localport = NET_GetPortFromSockaddr(&sa);
            (*env)->SetIntField(env, cls, psi_localportID, localport);
        }
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketBind(JNIEnv *env,
        jclass cls, jboolean preferIPv6, jobject iaObj, jint localport) {
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);
    int fd;
    int len = 0;
    struct sockaddr sa;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                        "Socket closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }
    if (IS_NULL(iaObj)) {
        JNU_ThrowNullPointerException(env, "iaObj is null.");
        return;
    }

    if (NET_InetAddressToSockaddr(env, iaObj, localport, &sa,
                                  &len, preferIPv6) != 0) {
        return;
    }

    if (RDMA_Bind(fd, &sa, len) < 0) {
        if (errno == EADDRINUSE || errno == EADDRNOTAVAIL ||
            errno == EPERM || errno == EACCES) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "BindException", "Bind failed");
        } else {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "SocketException", "Bind failed");
        }
        return;
    }
    (*env)->SetObjectField(env, cls, psi_addressID, iaObj);

    if (localport == 0) {
        socklen_t slen = sizeof(struct sockaddr);
        if (rs_getsockname(fd, &sa, &slen) == -1) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "SocketException",
                    "Error getting socket name");
            return;
        }
        localport = NET_GetPortFromSockaddr(&sa);
        (*env)->SetIntField(env, cls, psi_localportID, localport);
    } else {
        (*env)->SetIntField(env, cls, psi_localportID, localport);
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketListen(JNIEnv *env,
        jclass cls, jint count) {
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);
    int fd;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "Socket closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }

    if (count == 0x7fffffff)
        count -= 1;

    int rv = rs_listen(fd, count);
    if (rv == -1) {
        JNU_ThrowByNameWithMessageAndLastError(env,
                JNU_JAVANETPKG "SocketException", "Listen failed");
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketAccept(JNIEnv *env,
        jclass cls, jobject socket) {
    int port;
    jint timeout = (*env)->GetIntField(env, cls, psi_timeoutID);
    jlong prevNanoTime = 0;
    jlong nanoTimeout = (jlong) timeout * NET_NSEC_PER_MSEC;
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);

    jobject socketFdObj;
    jobject socketAddressObj;

    jint fd;

    jint newfd;

    struct sockaddr sa;
    socklen_t slen = sizeof(struct sockaddr);

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "Socket closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }
    if (IS_NULL(socket)) {
        JNU_ThrowNullPointerException(env, "socket is null");
        return;
    }

    for (;;) {
        int ret;
        jlong currNanoTime;

        if (prevNanoTime == 0 && nanoTimeout > 0) {
            prevNanoTime = JVM_NanoTime(env, 0);
        }

        if (timeout <= 0) {
            ret = RDMA_Timeout(env, fd, -1, 0);
        } else {
            ret = RDMA_Timeout(env, fd, nanoTimeout / NET_NSEC_PER_MSEC,
                    prevNanoTime);
        }

        if (ret == 0) {
            JNU_ThrowByName(env, JNU_JAVANETPKG "SocketTimeoutException",
                    "Accept timed out");
            return;
        } else if (ret == -1) {
            if (errno == EBADF) {
               JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                       "Socket closed");
            } else if (errno == ENOMEM) {
               JNU_ThrowOutOfMemoryError(env,
                       "NET_Timeout native heap allocation failed");
            } else {
               JNU_ThrowByNameWithMessageAndLastError(env,
                       JNU_JAVANETPKG "SocketException", "Accept failed");
            }
            return;
        }

        newfd = RDMA_Accept(fd, &sa, &slen);

        if (newfd >= 0) {
            SET_BLOCKING(newfd);
            break;
        }

        if (!(errno == ECONNABORTED || errno == EWOULDBLOCK)) {
            break;
        }

        if (nanoTimeout >= NET_NSEC_PER_MSEC) {
            currNanoTime = JVM_NanoTime(env, 0);
            nanoTimeout -= (currNanoTime - prevNanoTime);
            if (nanoTimeout < NET_NSEC_PER_MSEC) {
                JNU_ThrowByName(env, JNU_JAVANETPKG "SocketTimeoutException",
                        "Accept timed out");
                return;
            }
            prevNanoTime = currNanoTime;
        }
    }

    if (newfd < 0) {
        if (newfd == -2) {
            JNU_ThrowByName(env, JNU_JAVAIOPKG "InterruptedIOException",
                    "operation interrupted");
        } else {
            if (errno == EINVAL) {
                errno = EBADF;
            }
            if (errno == EBADF) {
                JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                        "Socket closed");
            } else {
                JNU_ThrowByNameWithMessageAndLastError(env,
                        JNU_JAVANETPKG "SocketException", "Accept failed");
            }
        }
        return;
    }

    socketAddressObj = NET_SockaddrToInetAddress(env, &sa, &port);
    if (socketAddressObj == NULL) {
        rs_close(newfd);
        return;
    }

    socketFdObj = (*env)->GetObjectField(env, socket, psi_fdID);
    (*env)->SetIntField(env, socketFdObj, IO_fd_fdID, newfd);
    (*env)->SetObjectField(env, socket, psi_addressID, socketAddressObj);
    (*env)->SetIntField(env, socket, psi_portID, port);
    port = (*env)->GetIntField(env, cls, psi_localportID);
    (*env)->SetIntField(env, socket, psi_localportID, port);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketClose(JNIEnv *env,
        jclass cls) {
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);
    jint fd;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "socket already closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }
    if (fd != -1) {
        (*env)->SetIntField(env, fdObj, IO_fd_fdID, -1);
        RDMA_SocketClose(fd);
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketShutdown(JNIEnv *env,
        jclass cls, jint howto) {
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);
    jint fd;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "socket already closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
    }
    rs_shutdown(fd, howto);
}


JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketSetOption(JNIEnv *env,
        jclass cls, jint cmd, jboolean on, jobject value) {
    int fd;
    int level, optname, optlen;
    union {
        int i;
        struct linger ling;
    } optval;

    fd = getFD(env, cls);
    if (fd < 0) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "Socket closed");
        return;
    }

    if (cmd == java_net_SocketOptions_SO_TIMEOUT) {
        return;
    }

    if (RDMA_MapSocketOption(cmd, &level, &optname)) {
        JNU_ThrowByName(env, "java/net/SocketException", "Invalid option");
        return;
    }

    switch (cmd) {
        case java_net_SocketOptions_SO_SNDBUF :
        case java_net_SocketOptions_SO_RCVBUF :
            {
                jclass cls;
                jfieldID fid;

                cls = (*env)->FindClass(env, "java/lang/Integer");
                CHECK_NULL(cls);
                fid = (*env)->GetFieldID(env, cls, "value", "I");
                CHECK_NULL(fid);

                optval.i = (*env)->GetIntField(env, value, fid);
                optlen = sizeof(optval.i);
                break;
            }
        default :
            optval.i = (on ? 1 : 0);
            optlen = sizeof(optval.i);

    }
    if (RDMA_SetSockOpt(fd, level, optname, (const void *)&optval, optlen) < 0) {
        JNU_ThrowByNameWithMessageAndLastError(env,
                JNU_JAVANETPKG "SocketException", "Error setting socket option");
    }
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketGetOption(JNIEnv *env,
        jclass cls, jint cmd, jobject iaContainerObj) {
    int fd;
    int level, optname, optlen;
    union {
        int i;
        struct linger ling;
    } optval;

    fd = getFD(env, cls);
    if (fd < 0) {
        JNU_ThrowByName(env, JNU_JAVANETPKG "SocketException",
                "Socket closed");
        return -1;
    }

    if (cmd == java_net_SocketOptions_SO_BINDADDR) {
        struct sockaddr sa;
        socklen_t len = sizeof(struct sockaddr);
        int port;
        jobject iaObj;
        jclass iaCntrClass;
        jfieldID iaFieldID;

        if (rs_getsockname(fd, &sa, &len) < 0) {
            JNU_ThrowByNameWithMessageAndLastError(env,
                    JNU_JAVANETPKG "SocketException",
                    "Error getting socket name");
            return -1;
        }
        iaObj = NET_SockaddrToInetAddress(env, &sa, &port);
        CHECK_NULL_RETURN(iaObj, -1);

        iaCntrClass = (*env)->GetObjectClass(env, iaContainerObj);
        iaFieldID = (*env)->GetFieldID(env, iaCntrClass,
                "addr", "Ljava/net/InetAddress;");
        CHECK_NULL_RETURN(iaFieldID, -1);
        (*env)->SetObjectField(env, iaContainerObj, iaFieldID, iaObj);
        return 0;
    }

    if (RDMA_MapSocketOption(cmd, &level, &optname)) {
        JNU_ThrowByName(env, "java/net/SocketException", "Invalid option");
        return -1;
    }

    optlen = sizeof(optval.i);

    if (RDMA_GetSockOpt(fd, level, optname, (void *)&optval, &optlen) < 0) {
        JNU_ThrowByNameWithMessageAndLastError(env,
        JNU_JAVANETPKG "SocketException", "Error getting socket option");
        return -1;
    }

    switch (cmd) {
        case java_net_SocketOptions_SO_SNDBUF:
        case java_net_SocketOptions_SO_RCVBUF:
            return optval.i;

        default :
            return (optval.i == 0) ? -1 : 1;
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_rdma_RdmaSocketImpl_rdmaSocketSendUrgentData(
        JNIEnv *env, jclass cls, jint data) {
    jobject fdObj = (*env)->GetObjectField(env, cls, psi_fdID);
    int n, fd;
    unsigned char d = data & 0xFF;

    if (IS_NULL(fdObj)) {
        JNU_ThrowByName(env, "java/net/SocketException", "Socket closed");
        return;
    } else {
        fd = (*env)->GetIntField(env, fdObj, IO_fd_fdID);
        if (fd == -1) {
            JNU_ThrowByName(env, "java/net/SocketException", "Socket closed");
            return;
        }

    }
    n = RDMA_Send(fd, (char *)&d, 1, MSG_OOB);
    if (n == -1) {
        JNU_ThrowIOExceptionWithLastError(env, "Write failed");
    }
}
