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
import java.nio.channels.DatagramChannel;
import java.nio.channels.SelectionKey;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;


// Make a udp socket channel look like a datagram socket.

public class WispUdpSocketImpl {

    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    private WispSocketLockSupport wispSocketLockSupport = new WispSocketLockSupport();
    // 1 verse 1 related socket
    private DatagramChannelImpl dc;

    private DatagramSocket so;

    private volatile int timeout = 0;

    public WispUdpSocketImpl(DatagramSocket so) {
        this.so = so;
    }

    public WispUdpSocketImpl(DatagramChannel dc, DatagramSocket so) {
        this.so = so;
        this.dc = (DatagramChannelImpl) dc;
    }

    public void bind(SocketAddress local) throws SocketException {
        try {
            if (local == null)
                local = new InetSocketAddress(0);
            getChannelImpl().bind(local);
        } catch (Exception x) {
            Net.translateToSocketException(x);
        }
    }

    public void connect(InetAddress address, int port) {
        try {
            connect(new InetSocketAddress(address, port));
        } catch (SocketException x) {
        }
    }

    public void connect(SocketAddress remote) throws SocketException {
        if (remote == null)
            throw new IllegalArgumentException("Address can't be null");

        InetSocketAddress isa = Net.asInetSocketAddress(remote);
        int port = isa.getPort();
        if (port < 0 || port > 0xFFFF)
            throw new IllegalArgumentException("connect: " + port);
        if (remote == null)
            throw new IllegalArgumentException("connect: null address");
        if (isClosed())
            return;
        try {
            getChannelImpl().connect(remote);
        } catch (Exception x) {
            Net.translateToSocketException(x);
        }
    }

    public void disconnect() {
        try {
            getChannelImpl().disconnect();
        } catch (IOException x) {
            throw new Error(x);
        }
    }

    public boolean isBound() {
        return dc != null && dc.localAddress() != null;
    }

    public boolean isConnected() {
        return dc != null && dc.remoteAddress() != null;
    }

    public InetAddress getInetAddress() {
        return (isConnected()
                ? Net.asInetSocketAddress(dc.remoteAddress()).getAddress()
                : null);
    }

    public int getPort() {
        return (isConnected()
                ? Net.asInetSocketAddress(dc.remoteAddress()).getPort()
                : -1);
    }

    public void send(DatagramPacket p) throws IOException {
        try {
            wispSocketLockSupport.beginWrite();
            send0(p);
        } finally {
            wispSocketLockSupport.endWrite();
        }
    }

    private SocketAddress receive(ByteBuffer bb) throws IOException {
        try {
            wispSocketLockSupport.beginRead();
            return receive0(bb);
        } finally {
            wispSocketLockSupport.endRead();
        }
    }


    private void send0(DatagramPacket p) throws IOException {
        final DatagramChannelImpl ch = getChannelImpl();
        try {
            ByteBuffer bb = ByteBuffer.wrap(p.getData(),
                    p.getOffset(),
                    p.getLength());

            boolean useWrite = ch.isConnected() && p.getAddress() == null;
            if (useWrite) {
                InetSocketAddress isa = (InetSocketAddress)
                        ch.remoteAddress();
                p.setPort(isa.getPort());
                p.setAddress(isa.getAddress());
            }

            int writeLen = bb.remaining();
            if ((useWrite ? ch.write(bb) : ch.send(bb, p.getSocketAddress())) == writeLen)
                return;

            /* Don't call so.getSoTimeout() here as it will try to acquire the lock of datagram socket
               which might be already held by receiver, hence deadlock happens */
            if (getSoTimeout() > 0)
                WEA.addTimer(System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(getSoTimeout()));

            do {
                WEA.registerEvent(ch, SelectionKey.OP_WRITE);
                WEA.park(-1);

                if (getSoTimeout() > 0 && WEA.isTimeout()) {
                    throw new SocketTimeoutException("time out");
                }
                if (useWrite) {
                    ch.write(bb);
                } else {
                    ch.send(bb, p.getSocketAddress());
                }
            } while (bb.remaining() > 0);

        } catch (IOException x) {
            Net.translateException(x);
        } finally {
            if (getSoTimeout() > 0) {
                WEA.cancelTimer();
            }
            WEA.unregisterEvent();
        }
    }


    private SocketAddress receive0(ByteBuffer bb) throws IOException {
        final DatagramChannelImpl ch = getChannelImpl();
        SocketAddress sa;

        try {
            if ((sa = ch.receive(bb)) != null)
                return sa;

            if (getSoTimeout() > 0)
                WEA.addTimer(System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(getSoTimeout()));

            do {
                WEA.registerEvent(ch, SelectionKey.OP_READ);
                WEA.park(-1);

                if (getSoTimeout() > 0 && WEA.isTimeout()) {
                    throw new SocketTimeoutException("time out");
                }
            } while ((sa = ch.receive(bb)) == null);
        } finally {
            if (getSoTimeout() > 0) {
                WEA.cancelTimer();
            }
            WEA.unregisterEvent();
        }

        return sa;
    }

    public int receive(DatagramPacket p, int bufLen) throws IOException {
        int dataLen = 0;
        try {
            ByteBuffer bb = ByteBuffer.wrap(p.getData(),
                    p.getOffset(),
                    bufLen);
            SocketAddress sender = receive(bb);
            p.setSocketAddress(sender);
            dataLen = bb.position() - p.getOffset();
        } catch (IOException x) {
            Net.translateException(x);
        }
        return dataLen;
    }

    public InetAddress getLocalAddress() {
        if (isClosed())
            return null;
        DatagramChannelImpl ch = null;
        try {
            ch = getChannelImpl();
        } catch (SocketException e) {
            // return 0.0.0.0
        }
        SocketAddress local = ch == null ? null : ch.localAddress();
        if (local == null)
            local = new InetSocketAddress(0);
        InetAddress result = ((InetSocketAddress) local).getAddress();
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            try {
                sm.checkConnect(result.getHostAddress(), -1);
            } catch (SecurityException x) {
                return new InetSocketAddress(0).getAddress();
            }
        }
        return result;
    }

    public int getLocalPort() {
        if (isClosed())
            return -1;
        try {
            SocketAddress local = getChannelImpl().getLocalAddress();
            if (local != null) {
                return ((InetSocketAddress) local).getPort();
            }
        } catch (Exception x) {
        }
        return 0;
    }

    public void setSoTimeout(int timeout) throws SocketException {
        this.timeout = timeout;
    }

    public int getSoTimeout() throws SocketException {
        return timeout;
    }

    private void setBooleanOption(SocketOption<Boolean> name, boolean value)
            throws SocketException {
        try {
            getChannelImpl().setOption(name, value);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    private void setIntOption(SocketOption<Integer> name, int value)
            throws SocketException {
        try {
            getChannelImpl().setOption(name, value);
        } catch (IOException x) {
            Net.translateToSocketException(x);
        }
    }

    private boolean getBooleanOption(SocketOption<Boolean> name) throws SocketException {
        try {
            return getChannelImpl().getOption(name);
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return false;       // keep compiler happy
        }
    }

    private int getIntOption(SocketOption<Integer> name) throws SocketException {
        try {
            return getChannelImpl().getOption(name);
        } catch (IOException x) {
            Net.translateToSocketException(x);
            return -1;          // keep compiler happy
        }
    }

    public void setSendBufferSize(int size) throws SocketException {
        if (size <= 0)
            throw new IllegalArgumentException("Invalid send size");
        setIntOption(StandardSocketOptions.SO_SNDBUF, size);
    }

    public int getSendBufferSize() throws SocketException {
        return getIntOption(StandardSocketOptions.SO_SNDBUF);
    }

    public void setReceiveBufferSize(int size) throws SocketException {
        if (size <= 0)
            throw new IllegalArgumentException("Invalid receive size");
        setIntOption(StandardSocketOptions.SO_RCVBUF, size);
    }

    public int getReceiveBufferSize() throws SocketException {
        return getIntOption(StandardSocketOptions.SO_RCVBUF);
    }

    public void setReuseAddress(boolean on) throws SocketException {
        setBooleanOption(StandardSocketOptions.SO_REUSEADDR, on);
    }

    public boolean getReuseAddress() throws SocketException {
        return getBooleanOption(StandardSocketOptions.SO_REUSEADDR);

    }

    public void setBroadcast(boolean on) throws SocketException {
        setBooleanOption(StandardSocketOptions.SO_BROADCAST, on);
    }

    public boolean getBroadcast() throws SocketException {
        return getBooleanOption(StandardSocketOptions.SO_BROADCAST);
    }

    public void setTrafficClass(int tc) throws SocketException {
        setIntOption(StandardSocketOptions.IP_TOS, tc);
    }

    public int getTrafficClass() throws SocketException {
        return getIntOption(StandardSocketOptions.IP_TOS);
    }

    public void close() {
        if (dc != null) {
            try {
                dc.close();
            } catch (IOException x) {
                throw new Error(x);
            }
            wispSocketLockSupport.unparkBlockedWispTask();
        }
    }

    public boolean isClosed() {
        return dc != null && !dc.isOpen();
    }

    public DatagramChannel getChannel() {
        return dc;
    }

    private DatagramChannelImpl getChannelImpl() throws SocketException {
        if (dc == null) {
            try {
                dc = (DatagramChannelImpl) DatagramChannel.open();
                dc.configureBlocking(false);
            } catch (IOException e) {
                throw new SocketException(e.getMessage());
            }
        }
        return dc;
    }
}
