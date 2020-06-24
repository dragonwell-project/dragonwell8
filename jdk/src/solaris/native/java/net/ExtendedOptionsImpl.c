/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

#include <jni.h>
#include <string.h>
#if defined(__linux__) || defined(MACOSX)
#include <netinet/tcp.h>
#include <netinet/in.h>
#endif

#include "net_util.h"
#include "jdk_net_SocketFlow.h"

static jclass sf_status_class;          /* Status enum type */

static jfieldID sf_status;
static jfieldID sf_priority;
static jfieldID sf_bandwidth;

static jfieldID sf_fd_fdID;             /* FileDescriptor.fd */

/* References to the literal enum values */

static jobject sfs_NOSTATUS;
static jobject sfs_OK;
static jobject sfs_NOPERMISSION;
static jobject sfs_NOTCONNECTED;
static jobject sfs_NOTSUPPORTED;
static jobject sfs_ALREADYCREATED;
static jobject sfs_INPROGRESS;
static jobject sfs_OTHER;

static jobject getEnumField(JNIEnv *env, char *name);
static void setStatus(JNIEnv *env, jobject obj, int errval);

/* OS specific code is implemented in these three functions */

static jboolean flowSupported0() ;

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_init
  (JNIEnv *env, jclass UNUSED) {

    static int initialized = 0;
    jclass c;

    /* Global class references */

    if (initialized) {
        return;
    }

    c = (*env)->FindClass(env, "jdk/net/SocketFlow$Status");
    CHECK_NULL(c);
    sf_status_class = (*env)->NewGlobalRef(env, c);
    CHECK_NULL(sf_status_class);

    /* int "fd" field of java.io.FileDescriptor  */

    c = (*env)->FindClass(env, "java/io/FileDescriptor");
    CHECK_NULL(c);
    sf_fd_fdID = (*env)->GetFieldID(env, c, "fd", "I");
    CHECK_NULL(sf_fd_fdID);


    /* SocketFlow fields */

    c = (*env)->FindClass(env, "jdk/net/SocketFlow");
    CHECK_NULL(c);

    /* status */

    sf_status = (*env)->GetFieldID(env, c, "status",
        "Ljdk/net/SocketFlow$Status;");
    CHECK_NULL(sf_status);

    /* priority */

    sf_priority = (*env)->GetFieldID(env, c, "priority", "I");
    CHECK_NULL(sf_priority);

    /* bandwidth */

    sf_bandwidth = (*env)->GetFieldID(env, c, "bandwidth", "J");
    CHECK_NULL(sf_bandwidth);

    /* Initialize the static enum values */

    sfs_NOSTATUS = getEnumField(env, "NO_STATUS");
    CHECK_NULL(sfs_NOSTATUS);
    sfs_OK = getEnumField(env, "OK");
    CHECK_NULL(sfs_OK);
    sfs_NOPERMISSION = getEnumField(env, "NO_PERMISSION");
    CHECK_NULL(sfs_NOPERMISSION);
    sfs_NOTCONNECTED = getEnumField(env, "NOT_CONNECTED");
    CHECK_NULL(sfs_NOTCONNECTED);
    sfs_NOTSUPPORTED = getEnumField(env, "NOT_SUPPORTED");
    CHECK_NULL(sfs_NOTSUPPORTED);
    sfs_ALREADYCREATED = getEnumField(env, "ALREADY_CREATED");
    CHECK_NULL(sfs_ALREADYCREATED);
    sfs_INPROGRESS = getEnumField(env, "IN_PROGRESS");
    CHECK_NULL(sfs_INPROGRESS);
    sfs_OTHER = getEnumField(env, "OTHER");
    CHECK_NULL(sfs_OTHER);
    initialized = JNI_TRUE;
}

static jobject getEnumField(JNIEnv *env, char *name) {
    jobject f;
    jfieldID fID = (*env)->GetStaticFieldID(env, sf_status_class, name,
        "Ljdk/net/SocketFlow$Status;");
    CHECK_NULL_RETURN(fID, NULL);

    f = (*env)->GetStaticObjectField(env, sf_status_class, fID);
    CHECK_NULL_RETURN(f, NULL);
    f  = (*env)->NewGlobalRef(env, f);
    CHECK_NULL_RETURN(f, NULL);
    return f;
}

/*
 * Retrieve the int file-descriptor from a public socket type object.
 * Gets impl, then the FileDescriptor from the impl, and then the fd
 * from that.
 */
static int getFD(JNIEnv *env, jobject fileDesc) {
    return (*env)->GetIntField(env, fileDesc, sf_fd_fdID);
}

/**
 * Sets the status field of a SocketFlow to one of the
 * canned enum values
 */
static void setStatus (JNIEnv *env, jobject obj, int errval) {
    switch (errval) {
      case 0: /* OK */
        (*env)->SetObjectField(env, obj, sf_status, sfs_OK);
        break;
      case EPERM:
        (*env)->SetObjectField(env, obj, sf_status, sfs_NOPERMISSION);
        break;
      case ENOTCONN:
        (*env)->SetObjectField(env, obj, sf_status, sfs_NOTCONNECTED);
        break;
      case EOPNOTSUPP:
        (*env)->SetObjectField(env, obj, sf_status, sfs_NOTSUPPORTED);
        break;
      case EALREADY:
        (*env)->SetObjectField(env, obj, sf_status, sfs_ALREADYCREATED);
        break;
      case EINPROGRESS:
        (*env)->SetObjectField(env, obj, sf_status, sfs_INPROGRESS);
        break;
      default:
        (*env)->SetObjectField(env, obj, sf_status, sfs_OTHER);
        break;
    }
}

#ifdef __solaris__

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    setFlowOption
 * Signature: (Ljava/io/FileDescriptor;Ljdk/net/SocketFlow;)V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_setFlowOption
  (JNIEnv *env, jclass UNUSED, jobject fileDesc, jobject flow) {
    int fd = getFD(env, fileDesc);

    if (fd < 0) {
        NET_ERROR(env, JNU_JAVANETPKG "SocketException", "socket closed");
        return;
    } else {
        sock_flow_props_t props;
        jlong bandwidth;
        int rv;

        jint priority = (*env)->GetIntField(env, flow, sf_priority);
        memset(&props, 0, sizeof(props));
        props.sfp_version = SOCK_FLOW_PROP_VERSION1;

        if (priority != jdk_net_SocketFlow_UNSET) {
            props.sfp_mask |= SFP_PRIORITY;
            props.sfp_priority = priority;
        }
        bandwidth = (*env)->GetLongField(env, flow, sf_bandwidth);
        if (bandwidth > -1)  {
            props.sfp_mask |= SFP_MAXBW;
            props.sfp_maxbw = (uint64_t) bandwidth;
        }
        rv = setsockopt(fd, SOL_SOCKET, SO_FLOW_SLA, &props, sizeof(props));
        if (rv < 0) {
            if (errno == ENOPROTOOPT) {
                JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                        "unsupported socket option");
            } else if (errno == EACCES || errno == EPERM) {
                NET_ERROR(env, JNU_JAVANETPKG "SocketException",
                                "Permission denied");
            } else {
                NET_ERROR(env, JNU_JAVANETPKG "SocketException",
                                "set option SO_FLOW_SLA failed");
            }
            return;
        }
        setStatus(env, flow, props.sfp_status);
    }
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    getFlowOption
 * Signature: (Ljava/io/FileDescriptor;Ljdk/net/SocketFlow;)V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_getFlowOption
  (JNIEnv *env, jclass UNUSED, jobject fileDesc, jobject flow) {
    int fd = getFD(env, fileDesc);

    if (fd < 0) {
        NET_ERROR(env, JNU_JAVANETPKG "SocketException", "socket closed");
        return;
    } else {
        sock_flow_props_t props;
        int status;
        socklen_t sz = sizeof(props);

        int rv = getsockopt(fd, SOL_SOCKET, SO_FLOW_SLA, &props, &sz);
        if (rv < 0) {
            if (errno == ENOPROTOOPT) {
                JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                        "unsupported socket option");
            } else if (errno == EACCES || errno == EPERM) {
                NET_ERROR(env, JNU_JAVANETPKG "SocketException",
                                "Permission denied");
            } else {
                NET_ERROR(env, JNU_JAVANETPKG "SocketException",
                                "set option SO_FLOW_SLA failed");
            }
            return;
        }
        /* first check status to see if flow exists */
        status = props.sfp_status;
        setStatus(env, flow, status);
        if (status == 0) { /* OK */
            /* can set the other fields now */
            if (props.sfp_mask & SFP_PRIORITY) {
                (*env)->SetIntField(env, flow, sf_priority, props.sfp_priority);
            }
            if (props.sfp_mask & SFP_MAXBW) {
                (*env)->SetLongField(env, flow, sf_bandwidth,
                                        (jlong)props.sfp_maxbw);
            }
        }
    }
}

static jboolean flowsupported;
static jboolean flowsupported_set = JNI_FALSE;

static jboolean flowSupported0() {
    /* Do a simple dummy call, and try to figure out from that */
    sock_flow_props_t props;
    int rv, s;
    if (flowsupported_set) {
        return flowsupported;
    }
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        flowsupported = JNI_FALSE;
        flowsupported_set = JNI_TRUE;
        return JNI_FALSE;
    }
    memset(&props, 0, sizeof(props));
    props.sfp_version = SOCK_FLOW_PROP_VERSION1;
    props.sfp_mask |= SFP_PRIORITY;
    props.sfp_priority = SFP_PRIO_NORMAL;
    rv = setsockopt(s, SOL_SOCKET, SO_FLOW_SLA, &props, sizeof(props));
    if (rv != 0 && errno == ENOPROTOOPT) {
        rv = JNI_FALSE;
    } else {
        rv = JNI_TRUE;
    }
    close(s);
    flowsupported = rv;
    flowsupported_set = JNI_TRUE;
    return flowsupported;
}

#else /* __solaris__ */

/* Non Solaris. Functionality is not supported. So, throw UnsupportedOpExc */

JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_setFlowOption
  (JNIEnv *env, jclass UNUSED, jobject fileDesc, jobject flow) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
        "unsupported socket option");
}

JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_getFlowOption
  (JNIEnv *env, jclass UNUSED, jobject fileDesc, jobject flow) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
        "unsupported socket option");
}

static jboolean flowSupported0() {
    return JNI_FALSE;
}

// Keep alive options are available for MACOSX and Linux only for
// the time being.
#if defined(__linux__) || defined(MACOSX)

// Map socket option level/name to OS specific constant
#ifdef __linux__
#define SOCK_OPT_LEVEL SOL_TCP
#define SOCK_OPT_NAME_KEEPIDLE TCP_KEEPIDLE
#define SOCK_OPT_NAME_KEEPIDLE_STR "TCP_KEEPIDLE"
#endif
#ifdef MACOSX
#define SOCK_OPT_LEVEL IPPROTO_TCP
#define SOCK_OPT_NAME_KEEPIDLE TCP_KEEPALIVE
#define SOCK_OPT_NAME_KEEPIDLE_STR "TCP_KEEPALIVE"
#endif

static jint socketOptionSupported(jint sockopt) {
    jint one = 1;
    jint rv, s;
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        return 0;
    }
    rv = setsockopt(s, SOCK_OPT_LEVEL, sockopt, (void *) &one, sizeof (one));
    if (rv != 0 && errno == ENOPROTOOPT) {
        rv = 0;
    } else {
        rv = 1;
    }
    close(s);
    return rv;
}

static void handleError(JNIEnv *env, jint rv, const char *errmsg) {
    if (rv < 0) {
        if (errno == ENOPROTOOPT) {
            JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                    "unsupported socket option");
        } else {
            JNU_ThrowByNameWithLastError(env, "java/net/SocketException", errmsg);
        }
    }
}

static void setTcpSocketOption
(JNIEnv *env, jobject fileDesc, jint optval, int opt, int optlevel, const char* errmsg) {
    int fd = getFD(env, fileDesc);

    if (fd < 0) {
        NET_ERROR(env, JNU_JAVANETPKG "SocketException", "socket closed");
        return;
    } else {
        jint rv = setsockopt(fd, optlevel, opt, &optval, sizeof (optval));
        handleError(env, rv, errmsg);
    }
}

static jint getTcpSocketOption
(JNIEnv *env, jobject fileDesc, int opt, int optlevel, const char* errmsg) {
    int fd = getFD(env, fileDesc);

    if (fd < 0) {
        NET_ERROR(env, JNU_JAVANETPKG "SocketException", "socket closed");
        return -1;
    } else {
        jint optval, rv;
        socklen_t sz = sizeof (optval);
        rv = getsockopt(fd, optlevel, opt, &optval, &sz);
        handleError(env, rv, errmsg);
        return optval;
    }
}

#else /* __linux__ || MACOSX */

/* Keep alive options not supported for non-linux/non-macosx so throw UnsupportedOpExc */

static void setTcpSocketOption
(JNIEnv *env, jobject fileDesc, jint optval, int opt, int optlevel, const char* errmsg) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
        "unsupported socket option");
}

static jint getTcpSocketOption
(JNIEnv *env, jobject fileDesc, int opt, int optlevel, const char* errmsg) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
        "unsupported socket option");
}

#endif /* __linux__ || MACOSX*/

#endif /* __solaris__ */

JNIEXPORT jboolean JNICALL Java_sun_net_ExtendedOptionsImpl_flowSupported
  (JNIEnv *env, jclass UNUSED) {
    return flowSupported0();
}

#if defined(__linux__) || defined(MACOSX)

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    keepAliveOptionsSupported
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_net_ExtendedOptionsImpl_keepAliveOptionsSupported
(JNIEnv *env, jobject unused) {
    return socketOptionSupported(SOCK_OPT_NAME_KEEPIDLE) && socketOptionSupported(TCP_KEEPCNT)
            && socketOptionSupported(TCP_KEEPINTVL);
}

#else

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    keepAliveOptionsSupported
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_sun_net_ExtendedOptionsImpl_keepAliveOptionsSupported
(JNIEnv *env, jobject unused) {
    return JNI_FALSE;
}

#endif /* __linux__ || MACOSX */

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    setTcpKeepAliveProbes
 * Signature: (Ljava/io/FileDescriptor;I)V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_setTcpKeepAliveProbes
(JNIEnv *env, jobject unused, jobject fileDesc, jint optval) {
    setTcpSocketOption(env, fileDesc, optval, TCP_KEEPCNT, SOCK_OPT_LEVEL,
                       "set option TCP_KEEPCNT failed");
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    setTcpKeepAliveTime
 * Signature: (Ljava/io/FileDescriptor;I)V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_setTcpKeepAliveTime
(JNIEnv *env, jobject unused, jobject fileDesc, jint optval) {
    setTcpSocketOption(env, fileDesc, optval, SOCK_OPT_NAME_KEEPIDLE, SOCK_OPT_LEVEL,
                       "set option " SOCK_OPT_NAME_KEEPIDLE_STR " failed");
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    setTcpKeepAliveIntvl
 * Signature: (Ljava/io/FileDescriptor;I)V
 */
JNIEXPORT void JNICALL Java_sun_net_ExtendedOptionsImpl_setTcpKeepAliveIntvl
(JNIEnv *env, jobject unused, jobject fileDesc, jint optval) {
    setTcpSocketOption(env, fileDesc, optval, TCP_KEEPINTVL, SOCK_OPT_LEVEL,
                       "set option TCP_KEEPINTVL failed");
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    getTcpKeepAliveProbes
 * Signature: (Ljava/io/FileDescriptor;)I
 */
JNIEXPORT jint JNICALL Java_sun_net_ExtendedOptionsImpl_getTcpKeepAliveProbes
(JNIEnv *env, jobject unused, jobject fileDesc) {
    return getTcpSocketOption(env, fileDesc, TCP_KEEPCNT, SOCK_OPT_LEVEL,
                              "get option TCP_KEEPCNT failed");
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    getTcpKeepAliveTime
 * Signature: (Ljava/io/FileDescriptor;)I
 */
JNIEXPORT jint JNICALL Java_sun_net_ExtendedOptionsImpl_getTcpKeepAliveTime
(JNIEnv *env, jobject unused, jobject fileDesc) {
    return getTcpSocketOption(env, fileDesc, SOCK_OPT_NAME_KEEPIDLE, SOCK_OPT_LEVEL,
                              "get option " SOCK_OPT_NAME_KEEPIDLE_STR " failed");
}

/*
 * Class:     sun_net_ExtendedOptionsImpl
 * Method:    getTcpKeepAliveIntvl
 * Signature: (Ljava/io/FileDescriptor;)I
 */
JNIEXPORT jint JNICALL Java_sun_net_ExtendedOptionsImpl_getTcpKeepAliveIntvl
(JNIEnv *env, jobject unused, jobject fileDesc) {
    return getTcpSocketOption(env, fileDesc, TCP_KEEPINTVL, SOCK_OPT_LEVEL,
                              "get option TCP_KEEPINTVL failed");
}
