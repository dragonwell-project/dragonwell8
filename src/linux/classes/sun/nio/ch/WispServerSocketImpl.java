/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package sun.nio.ch;

import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.io.IOException;
import java.net.*;
import java.nio.channels.SelectionKey;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.concurrent.TimeUnit;


// Make a server socket channel be like a socket and yield on block

public class WispServerSocketImpl
{
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    private WispSocketLockSupport wispSocketLockSupport = new WispSocketLockSupport();
    // The channel being adapted
    private ServerSocketChannelImpl ssc = null;

    // Timeout "option" value for accepts
    private volatile int timeout = 0;

    public WispServerSocketImpl() {
    }

    public void bind(SocketAddress local) throws IOException {
        bind(local, 50);
    }

    public void bind(SocketAddress local, int backlog) throws IOException {
        if (local == null)
            local = new InetSocketAddress(0);
        try {
            getChannelImpl().bind(local, backlog);
        } catch (Exception x) {
            Net.translateException(x);
        }
    }

    public InetAddress getInetAddress() {
        if (ssc == null || !ssc.isBound())
            return null;
        return Net.getRevealedLocalAddress(ssc.localAddress()).getAddress();
    }

    public int getLocalPort() {
        if (ssc == null || !ssc.isBound())
            return -1;
        return Net.asInetSocketAddress(ssc.localAddress()).getPort();
    }

    public Socket accept() throws IOException {
        try {
            wispSocketLockSupport.beginRead();
            return accept0();
        } finally {
            wispSocketLockSupport.endRead();
        }
    }

    private Socket accept0() throws IOException {
        final ServerSocketChannel ch = getChannelImpl();
        try {
            SocketChannel res;

            if (getSoTimeout() > 0) {
                WEA.addTimer(System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(getSoTimeout()));
            }

            while ((res = ch.accept()) == null) {
                WEA.registerEvent(ch, SelectionKey.OP_ACCEPT);
                WEA.park(-1);

                if (getSoTimeout() > 0 && WEA.isTimeout()) {
                    throw new SocketTimeoutException("time out");
                }
            }
            res.configureBlocking(false);
            return SharedSecrets.getJavaNetSocketAccess().createSocketFromChannel(res);

        } catch (Exception x) {
            Net.translateException(x, true);
            return null;
        } finally {
            if (getSoTimeout() > 0) {
                WEA.cancelTimer();
            }
            WEA.unregisterEvent();
        }
    }

    public void close() throws IOException {
        if (ssc != null) {
            ssc.close();
            wispSocketLockSupport.unparkBlockedWispTask();
        }
    }

    public ServerSocketChannel getChannel() {
        return ssc;
    }

    public boolean isBound() {
        return ssc != null && ssc.isBound();
    }

    public boolean isClosed() {
        return ssc != null && !ssc.isOpen();
    }

    public void setSoTimeout(int timeout) throws SocketException {
        this.timeout = timeout;
    }

    public int getSoTimeout() throws IOException {
        return timeout;
    }

    public void setReuseAddress(boolean on) throws SocketException {
        try {
            getChannelImpl().setOption(StandardSocketOptions.SO_REUSEADDR, on);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    public boolean getReuseAddress() throws SocketException {
        try {
            return getChannelImpl().getOption(StandardSocketOptions.SO_REUSEADDR);
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return false;       // Never happens
        }
    }

    public String toString() {
        if (!isBound())
            return "ServerSocket[unbound]";
        return "ServerSocket[addr=" + getInetAddress() +
                ",localport=" + getLocalPort()  + "]";
    }

    public void setReceiveBufferSize(int size) throws SocketException {
        if (size <= 0)
            throw new IllegalArgumentException("size can not be 0 or negative");
        try {
            getChannelImpl().setOption(StandardSocketOptions.SO_RCVBUF, size);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    public int getReceiveBufferSize() throws SocketException {
        try {
            return getChannelImpl().getOption(StandardSocketOptions.SO_RCVBUF);
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return -1;          // Never happens
        }
    }

    private ServerSocketChannelImpl getChannelImpl() throws SocketException {
        if (ssc == null) {
            try {
                ssc = (ServerSocketChannelImpl) ServerSocketChannel.open();
                ssc.configureBlocking(false);
            } catch (IOException e) {
                throw new SocketException(e.getMessage());
            }
        }
        return ssc;
    }
}
