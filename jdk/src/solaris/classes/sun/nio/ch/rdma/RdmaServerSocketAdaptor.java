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

package sun.nio.ch.rdma;

import sun.misc.SharedSecrets;
import sun.nio.ch.Net;
import sun.nio.ch.NetAccess;

import java.io.IOException;
import java.net.*;
import java.nio.channels.IllegalBlockingModeException;
import java.nio.channels.NotYetBoundException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

class RdmaServerSocketAdaptor
    extends ServerSocket
{
    private final RdmaServerSocketChannelImpl ssc;

    private volatile int timeout;

    private static final NetAccess NET_ACCESS = SharedSecrets.getNetAccess();


    public static ServerSocket create(RdmaServerSocketChannelImpl ssc) {
        try {
            return new RdmaServerSocketAdaptor(ssc);
        } catch (IOException x) {
            throw new Error(x);
        }
    }

    private RdmaServerSocketAdaptor(RdmaServerSocketChannelImpl ssc)
            throws IOException {
        this.ssc = ssc;
    }

    public void bind(SocketAddress local) throws IOException {
        bind(local, 50);
    }

    public void bind(SocketAddress local, int backlog) throws IOException {
        if (local == null)
            local = new InetSocketAddress(0);
        try {
            ssc.bind(local, backlog);
        } catch (Exception x) {
            NET_ACCESS.translateException(x);
        }
    }

    public InetAddress getInetAddress() {
        InetSocketAddress local = ssc.localAddress();
        if (local == null) {
            return null;
        } else {
            return NET_ACCESS.getRevealedLocalAddress(local).getAddress();
        }
    }

    public int getLocalPort() {
        InetSocketAddress local = ssc.localAddress();
        if (local == null) {
            return -1;
        } else {
            return local.getPort();
        }
    }

    public Socket accept() throws IOException {
        synchronized (ssc.blockingLock()) {
            try {
                if (!ssc.isBound())
                    throw new NotYetBoundException();

                long to = this.timeout;
                if (to == 0) {
                    // for compatibility reasons: accept connection if available
                    // when configured non-blocking
                    SocketChannel sc = ssc.accept();
                    if (sc == null && !ssc.isBlocking())
                        throw new IllegalBlockingModeException();
                    return sc.socket();
                }

                if (!ssc.isBlocking())
                    throw new IllegalBlockingModeException();
                for (;;) {
                    long st = System.currentTimeMillis();
                    if (ssc.pollAccept(to))
                        return ssc.accept().socket();
                    to -= System.currentTimeMillis() - st;
                    if (to <= 0)
                        throw new SocketTimeoutException();
                }

            } catch (Exception x) {
                NET_ACCESS.translateException(x);
                assert false;
                return null;            // Never happens
            }
        }
    }

    public void close() throws IOException {
        ssc.close();
    }

    public ServerSocketChannel getChannel() {
        return ssc;
    }

    public boolean isBound() {
        return ssc.isBound();
    }

    public boolean isClosed() {
        return !ssc.isOpen();
    }

    public void setSoTimeout(int timeout) throws SocketException {
        this.timeout = timeout;
    }

    public int getSoTimeout() throws SocketException {
        return timeout;
    }

    public void setReuseAddress(boolean on) throws SocketException {
        try {
            ssc.setOption(StandardSocketOptions.SO_REUSEADDR, on);
        } catch (IOException x) {
            NET_ACCESS.translateToSocketException(x);
        }
    }

    public boolean getReuseAddress() throws SocketException {
        try {
            return ssc.getOption(StandardSocketOptions.SO_REUSEADDR)
                      .booleanValue();
        } catch (IOException x) {
            NET_ACCESS.translateToSocketException(x);
            return false;       // Never happens
        }
    }

    public String toString() {
        if (!isBound())
            return "RdmaServerSocket[unbound]";
        return "RdmaServerSocket[addr=" + getInetAddress() +
               ",localport=" + getLocalPort()  + "]";
    }

    public void setReceiveBufferSize(int size) throws SocketException {
        // size 0 valid for ServerSocketChannel, invalid for ServerSocket
        if (size <= 0)
            throw new IllegalArgumentException("size cannot be 0 or negative");
        try {
            ssc.setOption(StandardSocketOptions.SO_RCVBUF, size);
        } catch (IOException x) {
            NET_ACCESS.translateToSocketException(x);
        }
    }

    public int getReceiveBufferSize() throws SocketException {
        try {
            return ssc.getOption(StandardSocketOptions.SO_RCVBUF).intValue();
        } catch (IOException x) {
            NET_ACCESS.translateToSocketException(x);
            return -1;          // Never happens
        }
    }
}
