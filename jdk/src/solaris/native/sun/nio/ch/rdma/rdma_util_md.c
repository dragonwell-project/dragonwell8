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

#include <assert.h>
#include "java_net_SocketOptions.h"
#include "sun_nio_ch_rdma_LinuxRdmaSocketOptions.h"
#include "jvm.h"
#include <netinet/tcp.h> // defines TCP_NODELAY
#include "net_util.h"
#include "rdma_util_md.h"
#include "Rsocket.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <pthread.h>

#define BLOCKING_IO_RETURN_INT(FD, FUNC) {      \
    int ret;                                    \
    threadEntry_t self;                         \
    fdEntry_t *fdEntry = getFdEntry(FD);        \
    if (fdEntry == NULL) {                      \
        errno = EBADF;                          \
        return -1;                              \
    }                                           \
    do {                                        \
        startOp(fdEntry, &self);                \
        ret = FUNC;                             \
        endOp(fdEntry, &self);                  \
    } while (ret == -1 && errno == EINTR);      \
    return ret;                                 \
}

typedef struct threadEntry {
    pthread_t thr;                      /* this thread */
    struct threadEntry *next;           /* next thread */
    int intr;                           /* interrupted */
} threadEntry_t;

typedef struct {
    pthread_mutex_t lock;               /* fd lock */
    threadEntry_t *threads;             /* threads blocked on fd */
} fdEntry_t;

static int sigWakeup = (__SIGRTMAX - 2);

static fdEntry_t* fdTable = NULL;

static const int fdTableMaxSize = 0x1000; /* 4K */

static int fdTableLen = 0;

static int fdLimit = 0;

static fdEntry_t** fdOverflowTable = NULL;

static int fdOverflowTableLen = 0;

static const int fdOverflowTableSlabSize = 0x10000; /* 64k */

pthread_mutex_t fdOverflowTableLock = PTHREAD_MUTEX_INITIALIZER;

static void sig_wakeup(int sig) {
}

static void __attribute((constructor)) init() {
    struct rlimit nbr_files;
    sigset_t sigset;
    struct sigaction sa;
    int i = 0;

    if (-1 == getrlimit(RLIMIT_NOFILE, &nbr_files)) {
        fprintf(stderr, "library initialization failed - "
                "unable to get max # of allocated fds\n");
        abort();
    }
    if (nbr_files.rlim_max != RLIM_INFINITY) {
        fdLimit = nbr_files.rlim_max;
    } else {
        fdLimit = INT_MAX;
    }

    fdTableLen = fdLimit < fdTableMaxSize ? fdLimit : fdTableMaxSize;
    fdTable = (fdEntry_t*) calloc(fdTableLen, sizeof(fdEntry_t));
    if (fdTable == NULL) {
        fprintf(stderr, "library initialization failed - "
                "unable to allocate file descriptor table - out of memory");
        abort();
    } else {
        for (i = 0; i < fdTableLen; i ++) {
            pthread_mutex_init(&fdTable[i].lock, NULL);
        }
    }

    if (fdLimit > fdTableMaxSize) {
        fdOverflowTableLen = ((fdLimit - fdTableMaxSize) / fdOverflowTableSlabSize) + 1;
        fdOverflowTable = (fdEntry_t**) calloc(fdOverflowTableLen, sizeof(fdEntry_t*));
        if (fdOverflowTable == NULL) {
            fprintf(stderr, "library initialization failed - "
                    "unable to allocate file descriptor overflow table - out of memory");
            abort();
        }
    }

    sa.sa_handler = sig_wakeup;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sigWakeup, &sa, NULL);

    sigemptyset(&sigset);
    sigaddset(&sigset, sigWakeup);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

static inline fdEntry_t *getFdEntry(int fd) {
    fdEntry_t* result = NULL;

    if (fd < 0) {
        return NULL;
    }

    assert(fd < fdLimit);

    if (fd < fdTableMaxSize) {
        assert(fd < fdTableLen);
        result = &fdTable[fd];
    } else {
        const int indexInOverflowTable = fd - fdTableMaxSize;
        const int rootindex = indexInOverflowTable / fdOverflowTableSlabSize;
        const int slabindex = indexInOverflowTable % fdOverflowTableSlabSize;
        fdEntry_t* slab = NULL;
        assert(rootindex < fdOverflowTableLen);
        assert(slabindex < fdOverflowTableSlabSize);
        pthread_mutex_lock(&fdOverflowTableLock);
        if (fdOverflowTable[rootindex] == NULL) {
            fdEntry_t* const newSlab =
                (fdEntry_t*)calloc(fdOverflowTableSlabSize, sizeof(fdEntry_t));
            if (newSlab == NULL) {
                fprintf(stderr, "Unable to allocate file descriptor overflow"
                        " table slab - out of memory");
                pthread_mutex_unlock(&fdOverflowTableLock);
                abort();
            } else {
                int i;
                for (i = 0; i < fdOverflowTableSlabSize; i ++) {
                    pthread_mutex_init(&newSlab[i].lock, NULL);
                }
                fdOverflowTable[rootindex] = newSlab;
            }
        }
        pthread_mutex_unlock(&fdOverflowTableLock);
        slab = fdOverflowTable[rootindex];
        result = &slab[slabindex];
    }
    return result;
}

static inline void startOp(fdEntry_t *fdEntry, threadEntry_t *self) {
    self->thr = pthread_self();
    self->intr = 0;

    pthread_mutex_lock(&(fdEntry->lock));
    {
        self->next = fdEntry->threads;
        fdEntry->threads = self;
    }
    pthread_mutex_unlock(&(fdEntry->lock));
}

static inline void endOp (fdEntry_t *fdEntry, threadEntry_t *self) {
    int orig_errno = errno;
    pthread_mutex_lock(&(fdEntry->lock));
    {
        threadEntry_t *curr, *prev=NULL;
        curr = fdEntry->threads;
        while (curr != NULL) {
            if (curr == self) {
                if (curr->intr) {
                    orig_errno = EBADF;
                }
                if (prev == NULL) {
                    fdEntry->threads = curr->next;
                } else {
                    prev->next = curr->next;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    pthread_mutex_unlock(&(fdEntry->lock));
    errno = orig_errno;
}

#define RESTARTABLE(_cmd, _result) do { \
    do { \
        _result = _cmd; \
    } while((_result == -1) && (errno == EINTR)); \
} while(0)

int rdma_supported() {
    int one = 1;
    int rv, s;
    s = rs_socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

int RDMA_MapSocketOption(jint cmd, int *level, int *optname) {
    static struct {
        jint cmd;
        int level;
        int optname;
    } const opts[] = {
        { java_net_SocketOptions_TCP_NODELAY,           IPPROTO_TCP,    TCP_NODELAY },
        { java_net_SocketOptions_SO_SNDBUF,             SOL_SOCKET,     SO_SNDBUF },
        { java_net_SocketOptions_SO_RCVBUF,             SOL_SOCKET,     SO_RCVBUF },
        { java_net_SocketOptions_SO_REUSEADDR,          SOL_SOCKET,     SO_REUSEADDR },
        { sun_nio_ch_rdma_RdmaSocketOptions_SQSIZE,             SOL_RDMA,       RDMA_SQSIZE },
        { sun_nio_ch_rdma_RdmaSocketOptions_RQSIZE,             SOL_RDMA,       RDMA_RQSIZE },
        { sun_nio_ch_rdma_RdmaSocketOptions_INLINE,             SOL_RDMA,       RDMA_INLINE },
    };
    int i;
    for (i=0; i<(int)(sizeof(opts) / sizeof(opts[0])); i++) {
        if (cmd == opts[i].cmd) {
            *level = opts[i].level;
            *optname = opts[i].optname;
            return 0;
        }
    }
    return -1;
}

int RDMA_GetSockOpt(int fd, int level, int opt, void *result, int *len) {
    int rv;
    socklen_t socklen = *len;

    rv = rs_getsockopt(fd, level, opt, result, &socklen);
    *len = socklen;

    if (rv < 0) {
        return rv;
    }

    if ((level == SOL_SOCKET) && ((opt == SO_SNDBUF)
                                  || (opt == SO_RCVBUF))) {
        int n = *((int *)result);
        n /= 2;
        *((int *)result) = n;
    }
    return rv;
}

int RDMA_SetSockOpt(int fd, int level, int  opt, const void *arg, int len) {
    int *bufsize;
    if (level == SOL_SOCKET && opt == SO_RCVBUF) {
        int *bufsize = (int *)arg;
        if (*bufsize < 1024) {
            *bufsize = 1024;
        }
    }

    return rs_setsockopt(fd, level, opt, arg, len);
}

int RDMA_Bind(int fd, struct sockaddr *sa, int len) {
    int rv;
    int arg, alen;

    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *sock = ( struct sockaddr_in*)&sa;
        if ((ntohl(sock->sin_addr.s_addr) & 0x7f0000ff) == 0x7f0000ff) {
            errno = EADDRNOTAVAIL;
            return -1;
        }
    }
    rv = rs_bind(fd, sa, len);
    return rv;
}

jint RDMA_Wait(JNIEnv *env, jint fd, jint flags, jint timeout) {
    jlong prevNanoTime = JVM_NanoTime(env, 0);
    jlong nanoTimeout = (jlong) timeout * NET_NSEC_PER_MSEC;
    jint read_rv;

    while (1) {
        jlong newNanoTime;
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = 0;
        if (flags & NET_WAIT_READ)
          pfd.events |= POLLIN;
        if (flags & NET_WAIT_WRITE)
          pfd.events |= POLLOUT;
        if (flags & NET_WAIT_CONNECT)
          pfd.events |= POLLOUT;

        errno = 0;
        read_rv = RDMA_Poll(&pfd, 1, nanoTimeout / NET_NSEC_PER_MSEC);

        newNanoTime = JVM_NanoTime(env, 0);
        nanoTimeout -= (newNanoTime - prevNanoTime);
        if (nanoTimeout < NET_NSEC_PER_MSEC) {
          return read_rv > 0 ? 0 : -1;
        }
        prevNanoTime = newNanoTime;

        if (read_rv > 0) {
          break;
        }
      }
    return (nanoTimeout / NET_NSEC_PER_MSEC);
}

static int rdma_closefd(int fd2) {
    int rv, orig_errno;
    fdEntry_t *fdEntry = getFdEntry(fd2);
    if (fdEntry == NULL) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&(fdEntry->lock));
    do {
        rv = rs_close(fd2);
    } while (rv == -1 && errno == EINTR);

    threadEntry_t *curr = fdEntry->threads;
    while (curr != NULL) {
        curr->intr = 1;
        pthread_kill( curr->thr, sigWakeup );
        curr = curr->next;
    }
    orig_errno = errno;
    pthread_mutex_unlock(&(fdEntry->lock));
    errno = orig_errno;
    return rv;
}

int RDMA_SocketClose(int fd) {
    return rdma_closefd(fd);
}

int RDMA_Read(int s, void* buf, size_t len) {
    BLOCKING_IO_RETURN_INT(s, rs_recv(s, buf, len, 0));
}

int RDMA_NonBlockingRead(int s, void* buf, size_t len) {
    BLOCKING_IO_RETURN_INT(s, rs_recv(s, buf, len, MSG_DONTWAIT));
}

int RDMA_ReadV(int s, const struct iovec * vector, int count) {
    BLOCKING_IO_RETURN_INT(s, rs_readv(s, vector, count) );
}

int RDMA_RecvFrom(int s, void *buf, int len, unsigned int flags,
        struct sockaddr *from, socklen_t *fromlen) {
    BLOCKING_IO_RETURN_INT(s, rs_recvfrom(s, buf, len, flags, from, fromlen));
}

int RDMA_Send(int s, void *msg, int len, unsigned int flags) {
    BLOCKING_IO_RETURN_INT(s, rs_send(s, msg, len, flags));
}

int RDMA_WriteV(int s, const struct iovec * vector, int count) {
    BLOCKING_IO_RETURN_INT(s, rs_writev(s, vector, count));
}

int NET_RSendTo(int s, const void *msg, int len,  unsigned  int
       flags, const struct sockaddr *to, int tolen) {
    BLOCKING_IO_RETURN_INT(s, rs_sendto(s, msg, len, flags, to, tolen));
}

int RDMA_Accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    BLOCKING_IO_RETURN_INT(s, rs_accept(s, addr, addrlen));
}

int RDMA_Connect(int s, struct sockaddr *addr, int addrlen) {
    BLOCKING_IO_RETURN_INT(s, rs_connect(s, addr, addrlen));
}

int RDMA_Poll(struct pollfd *ufds, unsigned int nfds, int timeout) {
    BLOCKING_IO_RETURN_INT(ufds[0].fd, rs_poll(ufds, nfds, timeout));
}

int RDMA_Timeout(JNIEnv *env, int s, long timeout, jlong nanoTimeStamp) {
    jlong prevNanoTime = nanoTimeStamp;
    jlong nanoTimeout = (jlong)timeout * NET_NSEC_PER_MSEC;
    fdEntry_t *fdEntry = getFdEntry(s);

    if (fdEntry == NULL) {
        errno = EBADF;
        return -1;
    }

    for(;;) {
        struct pollfd pfd;
        int rv;
        threadEntry_t self;

        pfd.fd = s;
        pfd.events = POLLIN | POLLERR;

        startOp(fdEntry, &self);
        rv = rs_poll(&pfd, 1, nanoTimeout / NET_NSEC_PER_MSEC);
        endOp(fdEntry, &self);
        if (rv < 0 && errno == EINTR) {
            jlong newNanoTime = JVM_NanoTime(env, 0);
            nanoTimeout -= newNanoTime - prevNanoTime;
            if (nanoTimeout < NET_NSEC_PER_MSEC) {
                return 0;
            }
            prevNanoTime = newNanoTime;
        } else {
            return rv;
        }
    }
}
