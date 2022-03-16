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

#include <poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <net_util_md.h>

#define sun_nio_ch_rdma_RdmaSocketOptions_SQSIZE 0x3001
#define sun_nio_ch_rdma_RdmaSocketOptions_RQSIZE 0x3002
#define sun_nio_ch_rdma_RdmaSocketOptions_INLINE 0x3003

/************************************************************************
 * Functions
 */
int rdma_supported();
int RDMA_MapSocketOption(jint cmd, int *level, int *optname);
int RDMA_Bind(int fd, struct sockaddr *sa, int len);
int RDMA_Timeout(JNIEnv *env, int s, long timeout, jlong  nanoTimeStamp);
int RDMA_Read(int s, void* buf, size_t len);
int RDMA_NonBlockingRead(int s, void* buf, size_t len);
int RDMA_RecvFrom(int s, void *buf, int len, unsigned int flags,
                 struct sockaddr *from, socklen_t *fromlen);
int RDMA_ReadV(int s, const struct iovec * vector, int count);
int RDMA_Send(int s, void *msg, int len, unsigned int flags);
int RDMA_SendTo(int s, const void *msg, int len,  unsigned  int
               flags, const struct sockaddr *to, int tolen);
int RDMA_Writev(int s, const struct iovec * vector, int count);
int RDMA_Connect(int s, struct sockaddr *addr, int addrlen);
int RDMA_Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int RDMA_SocketClose(int s);
int RDMA_Poll(struct pollfd *ufds, unsigned int nfds, int timeout);
int RDMA_SetSockOpt(int fd, int level, int  opt, const void *arg, int len);
int RDMA_GetSockOpt(int fd, int level, int opt, void *result, int *len);
