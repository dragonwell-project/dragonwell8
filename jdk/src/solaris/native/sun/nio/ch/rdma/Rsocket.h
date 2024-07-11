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

#include <sys/socket.h>
#include <poll.h>
#include "jni.h"

/* Function types to support dynamic linking of socket API extension functions
 * for RDMA. This is so that there is no linkage depandancy during build or
 * runtime for librdmacm.*/
typedef int rs_rsocket_func(int domain, int type, int protocol);
typedef int rs_rfcntl_func(int fd, int cmd, ... /* arg */ );
typedef int rs_rlisten_func(int sockfd, int backlog);
typedef int rs_rbind_func(int sockfd, const struct sockaddr *addr,
        socklen_t addrlen);
typedef int rs_rconnect_func(int sockfd, const struct sockaddr *addr,
        socklen_t addlen);
typedef int rs_rgetsockname_func(int sockfd, struct sockaddr *addr,
        socklen_t *addrlen);
typedef int rs_rgetsockopt_func(int sockfd, int level, int optname,
        void *optval, socklen_t *optlen);
typedef int rs_rsetsockopt_func(int sockfd, int level, int optname,
        const void *optval, socklen_t optlen);
typedef int rs_rshutdown_func(int sockfd, int how);
typedef int rs_rpoll_func(struct pollfd *fds, nfds_t nfds, int timeout);
typedef size_t rs_rsend_func(int sockfd, const void *buf, size_t len,
        int flags);
typedef int rs_raccept_func(int sockfd, struct sockaddr *addr,
        socklen_t *addrlen);
typedef int rs_rclose_func(int sockfd);
typedef ssize_t rs_rread_func(int sockfd, void *buf, size_t count);
typedef ssize_t rs_rreadv_func(int sockfd, const struct iovec *iov,
        int iovcnt);
typedef ssize_t rs_rwrite_func(int sockfd, const void *buf, size_t count);
typedef ssize_t rs_rwritev_func(int sockfd, const struct iovec *iov,
        int iovcnt);
typedef ssize_t rs_rrecv_func(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t rs_rrecvfrom_func(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t rs_rsendto_func(int sockfd, const void *buf, size_t len,
        int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

extern rs_rsocket_func* rs_socket;
extern rs_rfcntl_func* rs_fcntl;
extern rs_rlisten_func* rs_listen;
extern rs_rbind_func* rs_bind;
extern rs_rconnect_func* rs_connect;
extern rs_rgetsockname_func* rs_getsockname;
extern rs_rgetsockopt_func* rs_getsockopt;
extern rs_rsetsockopt_func* rs_setsockopt;
extern rs_rshutdown_func* rs_shutdown;
extern rs_rpoll_func* rs_poll;
extern rs_rsend_func* rs_send;
extern rs_raccept_func* rs_accept;
extern rs_rclose_func* rs_close;
extern rs_rread_func* rs_read;
extern rs_rreadv_func* rs_readv;
extern rs_rwrite_func* rs_write;
extern rs_rwritev_func* rs_writev;
extern rs_rrecv_func* rs_recv;
extern rs_rrecvfrom_func* rs_recvfrom;
extern rs_rsendto_func* rs_sendto;

jboolean loadRdmaFuncs(JNIEnv* env);

/* Definitions taken from librdmacm/include/rdma/rsocket.h */
#ifndef SOL_RDMA
#define SOL_RDMA 0x10000
#endif
enum {
        RDMA_SQSIZE,
        RDMA_RQSIZE,
        RDMA_INLINE,
};
