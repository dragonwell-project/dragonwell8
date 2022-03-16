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

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.*;
import java.util.Objects;

import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;

public abstract class RdmaSocketImpl extends SocketImpl
{
    private static final NetAccess NET_ACCESS = SharedSecrets.getNetAccess();

    private ProtocolFamily family;

    Socket socket = null;
    ServerSocket serverSocket = null;

    int timeout;   // timeout in millisec

    int trafficClass;

    InputStream socketInputStream;
    OutputStream socketOutputStream;

    private boolean shut_rd = false;
    private boolean shut_wr = false;

    /* number of threads using the FileDescriptor */
    protected int fdUseCount = 0;

    /* lock when increment/decrementing fdUseCount */
    protected final Object fdLock = new Object();

    /* indicates a close is pending on the file descriptor */
    protected boolean closePending = false;

    /* indicates connection reset state */
    private int CONNECTION_NOT_RESET = 0;
    private int CONNECTION_RESET_PENDING = 1;
    private int CONNECTION_RESET = 2;
    private int resetState;
    private final Object resetLock = new Object();

    protected boolean stream;

    private static UnsupportedOperationException unsupported;

    static final sun.net.ext.RdmaSocketOptions rdmaOptions =
            sun.net.ext.RdmaSocketOptions.getInstance();

    static {
        java.security.AccessController.doPrivileged(
            new java.security.PrivilegedAction<Object>() {
                public Void run() {
                    System.loadLibrary("net");
                    System.loadLibrary("extnet");
                    return null;
                }
            });
        UnsupportedOperationException uoe = null;
        try {
            initProto();
        } catch (UnsupportedOperationException e) {
            uoe = e;
        }
        unsupported = uoe;
    }

    private static final Void checkSupported() {
        if (unsupported != null) {
            Exception e = unsupported;
            throw new UnsupportedOperationException(e.getMessage(), e);
        } else {
            return null;
        }
    }

    public RdmaSocketImpl(ProtocolFamily family) {
        this(checkSupported(), family);
    }

    private RdmaSocketImpl(Void unused, ProtocolFamily family) {
        Objects.requireNonNull(family, "null family");
        if (!(family == INET || family == INET6)) {
            throw new UnsupportedOperationException("Protocol family not supported");
        }
        if (family == INET6) {
            if (!NET_ACCESS.isIPv6Available()) {
                throw new UnsupportedOperationException(
                        "IPv6 not available");
            }
        }
        this.family = family;
    }

    private static volatile boolean checkedRdma;
    private static volatile boolean isRdmaAvailable;

    boolean isRdmaAvailable() {
        if (!checkedRdma) {
            isRdmaAvailable = isRdmaAvailable0();
            checkedRdma = true;
        }
        return isRdmaAvailable;
    }

    void setSocket(Socket soc) {
        this.socket = soc;
    }

    Socket getSocket() {
        return socket;
    }

    void setServerSocket(ServerSocket soc) {
        this.serverSocket = soc;
    }

    ServerSocket getServerSocket() {
        return serverSocket;
    }

    protected synchronized void create(boolean stream) throws IOException {
        this.stream = stream;
        if (stream) {
            fd = new FileDescriptor();

            boolean preferIPv6 = NET_ACCESS.isIPv6Available() && (family != INET);
            rdmaSocketCreate(preferIPv6, true);
        }
    }

    protected void connect(String host, int port)
            throws UnknownHostException, IOException {
        boolean connected = false;
        try {
            InetAddress address = InetAddress.getByName(host);
            this.port = port;
            this.address = address;

            connectToAddress(address, port, timeout);
            connected = true;
        } finally {
            if (!connected) {
                try {
                    close();
                } catch (IOException ioe) {
                }
            }
        }
    }

    protected void connect(InetAddress address, int port) throws IOException {
        if (family == INET && !(address instanceof Inet4Address))
                throw new IllegalArgumentException("address type mismatch");
        if (family == INET6 && !(address instanceof Inet6Address))
                throw new IllegalArgumentException("address type mismatch");

        this.port = port;
        this.address = address;
        try {
            connectToAddress(address, port, timeout);
            return;
        } catch (IOException e) {
            close();
            throw e;
        }
    }

    protected void connect(SocketAddress address, int timeout)
            throws IOException {
        boolean connected = false;
        try {
            if (address == null || !(address instanceof InetSocketAddress))
                throw new IllegalArgumentException("unsupported address type");
            InetSocketAddress addr = (InetSocketAddress) address;
            InetAddress ia = addr.getAddress();
            if (family == INET && !(ia instanceof Inet4Address))
                throw new IllegalArgumentException("address type mismatch");
            if (family == INET6 && !(ia instanceof Inet6Address))
                throw new IllegalArgumentException("address type mismatch");
            if (addr.isUnresolved())
                throw new UnknownHostException(addr.getHostName());
            this.port = addr.getPort();
            this.address = addr.getAddress();

            connectToAddress(this.address, port, timeout);
            connected = true;
        } finally {
            if (!connected) {
                try {
                    close();
                } catch (IOException ioe) {
                }
            }
        }
    }

    private void connectToAddress(InetAddress address, int port, int timeout)
            throws IOException {
        if (false && address.isAnyLocalAddress()) {
            doConnect(InetAddress.getLocalHost(), port, timeout);
        } else {
            doConnect(address, port, timeout);
        }
    }

    abstract boolean isOptionSupported(int opt);


    @Override
    public void setOption(int opt, Object val) throws SocketException {
        if (isClosedOrPending()) {
            throw new SocketException("Socket Closed");
        }
        boolean on = true;
        switch (opt) {
        case SO_TIMEOUT:
            if (val == null || (!(val instanceof Integer)))
                throw new SocketException("Bad parameter for SO_TIMEOUT");
            int tmp = ((Integer) val).intValue();
            if (tmp < 0)
                throw new IllegalArgumentException("timeout < 0");
            timeout = tmp;
            break;
        case SO_BINDADDR:
            throw new SocketException("Cannot re-bind socket");
        case TCP_NODELAY:
            if (val == null || !(val instanceof Boolean))
                throw new SocketException("bad parameter for TCP_NODELAY");
            on = ((Boolean)val).booleanValue();
            break;
        case SO_SNDBUF:
        case SO_RCVBUF:
            int value = ((Integer)val).intValue();
            int maxValue = 1024 * 1024 * 1024 - 1;   //maximum value for the buffer
            if (val == null || !(val instanceof Integer) ||
                !(value > 0)) {
                throw new SocketException("bad parameter for SO_SNDBUF " +
                                          "or SO_RCVBUF");
            }
            if (value >= maxValue)
                value = maxValue;
            break;
        case SO_REUSEADDR:
            if (val == null || !(val instanceof Boolean))
                throw new SocketException("bad parameter for SO_REUSEADDR");
            on = ((Boolean)val).booleanValue();
            if (serverSocket != null && serverSocket.isBound())
                    throw new UnsupportedOperationException(
                            "RDMA server socket cannot set " +
                            "SO_REUSEADDR after bind.");
            if (socket != null && socket.isConnected())
                    throw new UnsupportedOperationException(
                            "RDMA socket cannot set " +
                            "SO_REUSEADDR after connect.");
            break;
        default:
            throw new SocketException("unrecognized TCP option: " + opt);
        }
        socketSetOption(opt, on, val);
    }

    @Override
    public Object getOption(int opt) throws SocketException {
        if (isClosedOrPending()) {
            throw new SocketException("Socket Closed");
        }
        if (opt == SO_TIMEOUT) {
            return timeout;
        }
        int ret = 0;

        switch (opt) {
        case TCP_NODELAY:
            ret = rdmaSocketGetOption(opt, null);
            return Boolean.valueOf(ret != -1);
        case SO_REUSEADDR:
            ret = rdmaSocketGetOption(opt, null);
            return Boolean.valueOf(ret != -1);
        case SO_BINDADDR:
            RdmaInetAddressContainer in = new RdmaInetAddressContainer();
            ret = rdmaSocketGetOption(opt, in);
            return in.addr;
        case SO_SNDBUF:
        case SO_RCVBUF:
            ret = rdmaSocketGetOption(opt, null);
            return ret;
        default:
            return null;
        }
    }

    protected void socketSetOption(int opt, boolean b, Object val)
            throws SocketException {
        try {
            rdmaSocketSetOption(opt, b, val);
        } catch (SocketException se) {
            if (socket == null || !socket.isConnected())
                throw se;
        }
    }

    synchronized void doConnect(InetAddress address, int port, int timeout)
            throws IOException {
        try {
            acquireFD();
            boolean preferIPv6 = NET_ACCESS.isIPv6Available() && (family != INET);
            try {
                rdmaSocketConnect(preferIPv6, address, port, timeout);
                synchronized (fdLock) {
                    if (closePending) {
                        throw new SocketException ("Socket closed");
                    }
                }
            } finally {
                releaseFD();
            }
        } catch (IOException e) {
            close();
            throw e;
        }
    }

    private final InetAddress anyLocalAddress() throws IOException {
        if (family == INET)
            return InetAddress.getByName("0.0.0.0");
        else if (family == INET6)
            return InetAddress.getByName("::");
        else
            throw new IllegalArgumentException("Unsupported address type " + family);
    }

    protected synchronized void bind(InetAddress address, int lport)
            throws IOException {
        if (address == null)
            throw new IllegalArgumentException("address is null");

        if (address.isAnyLocalAddress())
            address = anyLocalAddress();

        if (family == INET && !(address instanceof Inet4Address))
            throw new IllegalArgumentException("address type mismatch");
        if (family == INET6 && !(address instanceof Inet6Address))
            throw new IllegalArgumentException("address type mismatch");
        boolean preferIPv6 = NET_ACCESS.isIPv6Available() && (family != INET);
        rdmaSocketBind(preferIPv6, address, lport);
    }

    protected synchronized void listen(int count) throws IOException {
        rdmaSocketListen(count);
    }

    protected void accept(SocketImpl s) throws IOException {
        acquireFD();
        try {
            rdmaSocketAccept(s);
        } finally {
            releaseFD();
        }
    }

    protected synchronized InputStream getInputStream() throws IOException {
        synchronized (fdLock) {
            if (isClosedOrPending())
                throw new IOException("Socket Closed");
            if (shut_rd)
                throw new IOException("Socket input is shutdown");
            if (socketInputStream == null)
                socketInputStream = new RdmaSocketInputStream(this);
        }
        return socketInputStream;
    }

    protected synchronized OutputStream getOutputStream() throws IOException {
        synchronized (fdLock) {
            if (isClosedOrPending())
                throw new IOException("Socket Closed");
            if (shut_wr)
                throw new IOException("Socket output is shutdown");
            if (socketOutputStream == null)
                socketOutputStream = new RdmaSocketOutputStream(this);
        }
        return socketOutputStream;
    }

    protected FileDescriptor getFileDescriptor() {
        return fd;
    }

    protected void setFileDescriptor(FileDescriptor fd) {
        this.fd = fd;
    }

    protected void setAddress(InetAddress address) {
        this.address = address;
    }

    void setPort(int port) {
        this.port = port;
    }

    void setLocalPort(int localport) {
        this.localport = localport;
    }

    protected synchronized int available() throws IOException {
        throw new UnsupportedOperationException(
                "unsupported socket operation");
    }

    protected void close() throws IOException {
        synchronized(fdLock) {
            if (fd != null) {
                if (fdUseCount == 0) {
                    if (closePending) {
                        return;
                    }
                    closePending = true;
                    rdmaSocketClose();
                    fd = null;
                    return;
                } else {
                    if (!closePending) {
                        closePending = true;
                        fdUseCount--;
                        rdmaSocketClose();
                    }
                }
            }
        }
    }

    void reset() throws IOException {
        if (fd != null) {
            rdmaSocketClose();
        }
        fd = null;
        postReset();
    }

    void postReset() throws IOException {
        address = null;
        port = 0;
        localport = 0;
    }

    protected void shutdownInput() throws IOException {
        if (fd != null) {
            rdmaSocketShutdown(SHUT_RD);
            if (socketInputStream != null) {
                ((RdmaSocketInputStream)socketInputStream).setEOF(true);
            }
            shut_rd = true;
        }
    }

    protected void shutdownOutput() throws IOException {
        if (fd != null) {
            rdmaSocketShutdown(SHUT_WR);
            shut_wr = true;
        }
    }

    protected boolean supportsUrgentData () {
        return true;
    }

    protected void sendUrgentData (int data) throws IOException {
        if (fd == null) {
            throw new IOException("Socket Closed");
        }
        rdmaSocketSendUrgentData(data);
    }

    FileDescriptor acquireFD() {
        synchronized (fdLock) {
            fdUseCount++;
            return fd;
        }
    }

    void releaseFD() {
        synchronized (fdLock) {
            fdUseCount--;
            if (fdUseCount == -1) {
                if (fd != null) {
                    try {
                        rdmaSocketClose();
                    } catch (IOException e) {
                    } finally {
                        fd = null;
                    }
                }
            }
        }
    }

    public boolean isConnectionReset() {
        synchronized (resetLock) {
            return (resetState == CONNECTION_RESET);
        }
    }

    public boolean isConnectionResetPending() {
        synchronized (resetLock) {
            return (resetState == CONNECTION_RESET_PENDING);
        }
    }

    public void setConnectionReset() {
        synchronized (resetLock) {
            resetState = CONNECTION_RESET;
        }
    }

    public void setConnectionResetPending() {
        synchronized (resetLock) {
            if (resetState == CONNECTION_NOT_RESET) {
                resetState = CONNECTION_RESET_PENDING;
            }
        }

    }

    public boolean isClosedOrPending() {
        synchronized (fdLock) {
            if (closePending || (fd == null)) {
                return true;
            } else {
                return false;
            }
        }
    }

    public int getTimeout() {
        return timeout;
    }

    protected InetAddress getInetAddress() {
        return address;
    }

    protected int getPort() {
        return port;
    }

    protected int getLocalPort() {
        return localport;
    }

    public static final int SHUT_RD = 0;
    public static final int SHUT_WR = 1;

    static native void initProto() throws UnsupportedOperationException;

    private static native boolean isRdmaAvailable0();

    native void rdmaSocketCreate(boolean preferIPv6, boolean isServer)
            throws IOException;

    native void rdmaSocketConnect(boolean preferIPv6, InetAddress address,
            int port, int timeout) throws IOException;

    native void rdmaSocketBind(boolean preferIPv6, InetAddress address,
            int port) throws IOException;

    native void rdmaSocketListen(int count) throws IOException;

    native void rdmaSocketAccept(SocketImpl s) throws IOException;

    native void rdmaSocketClose() throws IOException;

    native void rdmaSocketShutdown(int howto) throws IOException;

    native void rdmaSocketSetOption(int cmd, boolean on, Object value)
            throws SocketException;

    native int rdmaSocketGetOption(int opt, Object iaContainerObj)
            throws SocketException;

    native void rdmaSocketSendUrgentData(int data) throws IOException;
}
