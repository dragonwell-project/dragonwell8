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
import sun.net.ext.RdmaSocketOptions;
import sun.nio.ch.*;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.channels.*;
import java.nio.channels.spi.SelectorProvider;
import java.util.Collections;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.locks.ReentrantLock;

import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;

public class RdmaSocketChannelImpl
    extends SocketChannel
    implements SelChImpl
{
    // The protocol family of the socket
    private final ProtocolFamily family;

    private static RdmaSocketDispatcher nd;
    private final FileDescriptor fd;
    private final int fdVal;

    private final ReentrantLock readLock = new ReentrantLock();
    private final ReentrantLock writeLock = new ReentrantLock();

    private final Object stateLock = new Object();

    private volatile boolean isInputClosed;
    private volatile boolean isOutputClosed;

    private boolean isReuseAddress;

    private static final int ST_UNCONNECTED = 0;
    private static final int ST_CONNECTIONPENDING = 1;
    private static final int ST_CONNECTED = 2;
    private static final int ST_CLOSING = 3;
    private static final int ST_KILLPENDING = 4;
    private static final int ST_KILLED = 5;
    private volatile int state;  // need stateLock to change

    private long readerThread;
    private long writerThread;

    private InetSocketAddress localAddress;
    private InetSocketAddress remoteAddress;

    private Socket socket;

    private static final UnsupportedOperationException unsupported;
    private static final IOUtilAccess IO_UTIL_ACCESS = SharedSecrets.getIoUtilAccess();
    private static final NetAccess NET_ACCESS = SharedSecrets.getNetAccess();

    private static final SelectorProvider checkSupported(SelectorProvider sp) {
        if (unsupported != null)
            throw new UnsupportedOperationException(unsupported.getMessage(),
                                                    unsupported);
        else
            return sp;
    }

    protected RdmaSocketChannelImpl(SelectorProvider sp, ProtocolFamily family)
            throws IOException {
        super(checkSupported(sp));

        Objects.requireNonNull(family, "null family");
        if (!(family == INET || family == INET6)) {
            throw new UnsupportedOperationException("Protocol family not supported");
        }
        if (family == INET6) {
            if (!NET_ACCESS.isIPv6Available()) {
                throw new UnsupportedOperationException("IPv6 not available");
            }
        }

        this.family = family;
        this.fd = RdmaNet.socket(family, true);
        IOUtil.configureBlocking(fd, false);
        this.fdVal = IOUtil.fdVal(fd);
    }

    RdmaSocketChannelImpl(SelectorProvider sp,
                          FileDescriptor fd,
                          InetSocketAddress isa) throws IOException {
        super(checkSupported(sp));
        this.family = NET_ACCESS.isIPv6Available() ? INET6 : INET;
        this.fd = fd;
        this.fdVal = IOUtil.fdVal(fd);
        IOUtil.configureBlocking(fd, false);
        synchronized (stateLock) {
            this.localAddress = RdmaNet.localAddress(fd);
            this.remoteAddress = isa;
            this.state = ST_CONNECTED;
        }
    }

    private void ensureOpen() throws ClosedChannelException {
        if (!isOpen())
            throw new ClosedChannelException();
    }

    private void ensureOpenAndConnected() throws ClosedChannelException {
        int state = this.state;
        if (state < ST_CONNECTED) {
            throw new NotYetConnectedException();
        } else if (state > ST_CONNECTED) {
            throw new ClosedChannelException();
        }
    }

    @Override
    public Socket socket() {
        synchronized (stateLock) {
            if (socket == null)
                socket = RdmaSocketAdaptor.create(this);
            return socket;
        }
    }

    @Override
    public SocketAddress getLocalAddress() throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            return NET_ACCESS.getRevealedLocalAddress(localAddress);
        }
    }

    @Override
    public SocketAddress getRemoteAddress() throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            return remoteAddress;
        }
    }

    @Override
    public <T> SocketChannel setOption(SocketOption<T> name, T value)
            throws IOException {
        Objects.requireNonNull(name);
        if (!supportedOptions().contains(name))
            throw new UnsupportedOperationException("'" + name
                    + "' not supported");

        synchronized (stateLock) {
            ensureOpen();
            RdmaNet.setSocketOption(fd, RdmaNet.UNSPEC, name, value);
            return this;
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T> T getOption(SocketOption<T> name)
            throws IOException {
        Objects.requireNonNull(name);
        if (!supportedOptions().contains(name))
            throw new UnsupportedOperationException("'" + name
                    + "' not supported");

        synchronized (stateLock) {
            ensureOpen();
            return (T) RdmaNet.getSocketOption(fd, RdmaNet.UNSPEC, name);
        }
    }

    private static class DefaultOptionsHolder {
        static final Set<SocketOption<?>> defaultOptions = defaultOptions();

        private static Set<SocketOption<?>> defaultOptions() {
            HashSet<SocketOption<?>> set = new HashSet<>();
            set.add(StandardSocketOptions.SO_SNDBUF);
            set.add(StandardSocketOptions.SO_RCVBUF);
            set.add(StandardSocketOptions.SO_REUSEADDR);
            set.add(StandardSocketOptions.TCP_NODELAY);
            RdmaSocketOptions rdmaOptions =
                    RdmaSocketOptions.getInstance();
            set.addAll(rdmaOptions.options());
            return Collections.unmodifiableSet(set);
        }
    }

    public Set<SocketOption<?>> supportedOptions() {
         return DefaultOptionsHolder.defaultOptions;
    }

    private void beginRead(boolean blocking) throws ClosedChannelException {
        if (blocking) {
            // set hook for Thread.interrupt
            begin();

            synchronized (stateLock) {
                ensureOpenAndConnected();
                // record thread so it can be signalled if needed
                readerThread = NativeThread.current();
            }
        } else {
            ensureOpenAndConnected();
        }
    }
    private void endRead(boolean blocking, boolean completed)
            throws AsynchronousCloseException
    {
        if (blocking) {
            synchronized (stateLock) {
                readerThread = 0;
                // notify any thread waiting in implCloseSelectableChannel
                if (state == ST_CLOSING) {
                    stateLock.notifyAll();
                }
            }
            // remove hook for Thread.interrupt
            end(completed);
        }
    }

    @Override
    public int read(ByteBuffer buf) throws IOException {
        Objects.requireNonNull(buf);

        readLock.lock();
        try {
            boolean blocking = isBlocking();
            int n = 0;
            try {
                beginRead(blocking);

                // check if input is shutdown
                if (isInputClosed)
                    return IOStatus.EOF;

                n = IO_UTIL_ACCESS.read(fd, buf, -1, nd);
                if (n == IOStatus.UNAVAILABLE && blocking) {
                    do {
                        RdmaNet.poll(fd, Net.POLLIN, -1);
                        n = IO_UTIL_ACCESS.read(fd, buf, -1, nd);
                    } while (n == IOStatus.UNAVAILABLE && isOpen());
                }
            } finally {
                endRead(blocking, n > 0);
                if (n <= 0 && isInputClosed)
                    return IOStatus.EOF;
            }
            return IOStatus.normalize(n);
        } finally {
            readLock.unlock();
        }
    }

    @Override
    public long read(ByteBuffer[] dsts, int offset, int length)
            throws IOException
    {
        readLock.lock();
        try {
            boolean blocking = isBlocking();
            long n = 0;
            try {
                beginRead(blocking);

                // check if input is shutdown
                if (isInputClosed)
                    return IOStatus.EOF;

                n = IO_UTIL_ACCESS.read(fd, dsts, offset, length, nd);
                if (n == IOStatus.UNAVAILABLE && blocking) {
                    do {
                        RdmaNet.poll(fd, Net.POLLIN, -1);
                        n = IO_UTIL_ACCESS.read(fd, dsts, offset, length, nd);
                    } while (n == IOStatus.UNAVAILABLE && isOpen());
                }
            } finally {
                endRead(blocking, n > 0);
                if (n <= 0 && isInputClosed)
                    return IOStatus.EOF;
            }
            return IOStatus.normalize(n);
        } finally {
            readLock.unlock();
        }
    }

    private void beginWrite(boolean blocking) throws ClosedChannelException {
        if (blocking) {
            // set hook for Thread.interrupt
            begin();

            synchronized (stateLock) {
                ensureOpenAndConnected();
                if (isOutputClosed)
                    throw new ClosedChannelException();
                // record thread so it can be signalled if needed
                writerThread = NativeThread.current();
            }
        } else {
            ensureOpenAndConnected();
        }
    }

    private void endWrite(boolean blocking, boolean completed)
            throws AsynchronousCloseException {
        if (blocking) {
            synchronized (stateLock) {
                writerThread = 0;
                // notify any thread waiting in implCloseSelectableChannel
                if (state == ST_CLOSING) {
                    stateLock.notifyAll();
                }
            }
            // remove hook for Thread.interrupt
            end(completed);
        }
    }

    @Override
    public int write(ByteBuffer buf) throws IOException {
        Objects.requireNonNull(buf);

        writeLock.lock();
        try {
            boolean blocking = isBlocking();
            int n = 0;
            try {
                beginWrite(blocking);
                n = IO_UTIL_ACCESS.write(fd, buf, -1, nd);
                if (n == IOStatus.UNAVAILABLE && blocking) {
                    do {
                        RdmaNet.poll(fd, Net.POLLOUT, -1);
                        n = IO_UTIL_ACCESS.write(fd, buf, -1, nd);
                    } while (n == IOStatus.UNAVAILABLE && isOpen());
                }
            } finally {
                endWrite(blocking, n > 0);
                if (n <= 0 && isOutputClosed)
                    throw new AsynchronousCloseException();
            }
            return IOStatus.normalize(n);
        } finally {
            writeLock.unlock();
        }
    }

    @Override
    public long write(ByteBuffer[] srcs, int offset, int length)
            throws IOException {
        writeLock.lock();
        try {
            boolean blocking = isBlocking();
            long n = 0;
            try {
                beginWrite(blocking);
                n = IO_UTIL_ACCESS.write(fd, srcs, offset, length, nd);
                if (n == IOStatus.UNAVAILABLE && blocking) {
                    do {
                        RdmaNet.poll(fd, Net.POLLOUT, -1);
                        n = IO_UTIL_ACCESS.write(fd, srcs, offset, length, nd);
                    } while (n == IOStatus.UNAVAILABLE && isOpen());
                }
            } finally {
                endWrite(blocking, n > 0);
                if (n <= 0 && isOutputClosed)
                    throw new AsynchronousCloseException();
            }
            return IOStatus.normalize(n);
        } finally {
            writeLock.unlock();
        }
    }

    int sendOutOfBandData(byte b) throws IOException {
        writeLock.lock();
        try {
            boolean blocking = isBlocking();
            int n = 0;
            try {
                beginWrite(blocking);
                n = sendOutOfBandData(fd, b);
                if (n == IOStatus.UNAVAILABLE && blocking) {
                    do {
                        RdmaNet.poll(fd, Net.POLLOUT, -1);
                        n = sendOutOfBandData(fd, b);
                    } while (n == IOStatus.INTERRUPTED && isOpen());
                }
            } finally {
                endWrite(blocking, n > 0);
                if (n <= 0 && isOutputClosed)
                    throw new AsynchronousCloseException();
            }
            return IOStatus.normalize(n);
        } finally {
            writeLock.unlock();
        }
    }

    @Override
    protected void implConfigureBlocking(boolean block) throws IOException {
        readLock.lock();
        try {
            writeLock.lock();
            try {
                synchronized (stateLock) {
                    ensureOpen();
                    // do nothing
                }
            } finally {
                writeLock.unlock();
            }
        } finally {
            readLock.unlock();
        }
    }

    InetSocketAddress localAddress() {
        synchronized (stateLock) {
            return localAddress;
        }
    }

    InetSocketAddress remoteAddress() {
        synchronized (stateLock) {
            return remoteAddress;
        }
    }

    private final InetSocketAddress anyLocalAddress() throws IOException {
        if (family == INET)
            return new InetSocketAddress(InetAddress.getByName("0.0.0.0"), 0);
        else if (family == INET6)
            return new InetSocketAddress(InetAddress.getByName("::"), 0);
        else
            throw new UnsupportedAddressTypeException();
    }

    @Override
    public SocketChannel bind(SocketAddress local) throws IOException {
        readLock.lock();
        try {
            writeLock.lock();
            try {
                synchronized (stateLock) {
                    ensureOpen();
                    if (state == ST_CONNECTIONPENDING)
                        throw new ConnectionPendingException();
                    if (localAddress != null)
                        throw new AlreadyBoundException();
                    InetSocketAddress isa = (local == null)
                                            ? anyLocalAddress()
                                            : RdmaNet.checkAddress(local, family);
                    SecurityManager sm = System.getSecurityManager();
                    if (sm != null) {
                        sm.checkListen(isa.getPort());
                    }
                    RdmaNet.bind(family, fd, isa.getAddress(), isa.getPort());
                    localAddress = RdmaNet.localAddress(fd);
                }
            } finally {
                writeLock.unlock();
            }
        } finally {
            readLock.unlock();
        }
        return this;
    }

    @Override
    public boolean isConnected() {
        return (state == ST_CONNECTED);
    }

    @Override
    public boolean isConnectionPending() {
        return (state == ST_CONNECTIONPENDING);
    }

    private void beginConnect(boolean blocking, InetSocketAddress isa)
            throws IOException {
        if (blocking) {
            // set hook for Thread.interrupt
            begin();
        }
        synchronized (stateLock) {
            ensureOpen();
            int state = this.state;
            if (state == ST_CONNECTED)
                throw new AlreadyConnectedException();
            if (state == ST_CONNECTIONPENDING)
                throw new ConnectionPendingException();
            assert state == ST_UNCONNECTED;
            this.state = ST_CONNECTIONPENDING;

            remoteAddress = isa;

            if (blocking) {
                // record thread so it can be signalled if needed
                readerThread = NativeThread.current();
            }
        }
    }

    private void endConnect(boolean blocking, boolean completed)
            throws IOException {
        endRead(blocking, completed);

        if (completed) {
            synchronized (stateLock) {
                if (state == ST_CONNECTIONPENDING) {
                    localAddress = RdmaNet.localAddress(fd);
                    state = ST_CONNECTED;
                }
            }
        }
    }

    @Override
    public boolean connect(SocketAddress sa) throws IOException {
        InetSocketAddress isa = RdmaNet.checkAddress(sa, family);
        SecurityManager sm = System.getSecurityManager();
        if (sm != null)
            sm.checkConnect(isa.getAddress().getHostAddress(), isa.getPort());

        InetAddress ia = isa.getAddress();
        if (ia.isAnyLocalAddress())
            ia = InetAddress.getLocalHost();

        try {
            readLock.lock();
            try {
                writeLock.lock();
                try {
                    boolean blocking = isBlocking();
                    boolean connected = false;
                    try {
                        beginConnect(blocking, isa);
                        int n = RdmaNet.connect(family, fd, ia, isa.getPort());
                        if (n == IOStatus.UNAVAILABLE && blocking) {
                            do {
                                RdmaNet.poll(fd, Net.POLLOUT, -1);
                                n = checkConnect(fd, false);
                            } while (n == IOStatus.INTERRUPTED && isOpen());
                        }
                        connected = (n > 0) && isOpen();
                    } finally {
                        endConnect(blocking, connected);
                    }
                    return connected;
                } finally {
                    writeLock.unlock();
                }
            } finally {
                readLock.unlock();
            }
        } catch (IOException ioe) {
            // connect failed, close the channel
            close();
            throw ioe;
        }
    }

    private void beginFinishConnect(boolean blocking)
            throws ClosedChannelException {
        if (blocking) {
            // set hook for Thread.interrupt
            begin();
        }
        synchronized (stateLock) {
            ensureOpen();
            if (state != ST_CONNECTIONPENDING)
                throw new NoConnectionPendingException();
            if (blocking) {
                // record thread so it can be signalled if needed
                readerThread = NativeThread.current();
            }
        }
    }

    private void endFinishConnect(boolean blocking, boolean completed)
            throws IOException
    {
        endRead(blocking, completed);

        if (completed) {
            synchronized (stateLock) {
                if (state == ST_CONNECTIONPENDING) {
                    localAddress = RdmaNet.localAddress(fd);
                    state = ST_CONNECTED;
                }
            }
        }
    }

    @Override
    public boolean finishConnect() throws IOException {
        try {
            readLock.lock();
            try {
                writeLock.lock();
                try {
                    // no-op if already connected
                    if (isConnected())
                        return true;

                    boolean blocking = isBlocking();
                    boolean connected = false;
                    try {
                        beginFinishConnect(blocking);
                        int n = checkConnect(fd, false);
                        if (n == IOStatus.UNAVAILABLE && blocking) {
                            do {
                                RdmaNet.poll(fd, Net.POLLOUT, -1);
                                n = checkConnect(fd, false);
                            } while (n == IOStatus.UNAVAILABLE && isOpen());
                        }
                        connected = (n > 0) && isOpen();
                    } finally {
                        endFinishConnect(blocking, connected);
                    }
                    assert (blocking && connected) ^ !blocking;
                    return connected;
                } finally {
                    writeLock.unlock();
                }
            } finally {
                readLock.unlock();
            }
        } catch (IOException ioe) {
            // connect failed, close the channel
            close();
            throw ioe;
        }
    }

    @Override
    protected void implCloseSelectableChannel() throws IOException {
        assert !isOpen();
        boolean blocking;
        boolean connected;
        boolean interrupted = false;

        // set state to ST_CLOSING
        synchronized (stateLock) {
            assert state < ST_CLOSING;
            blocking = isBlocking();
            connected = (state == ST_CONNECTED);
            state = ST_CLOSING;
        }

        // wait for any outstanding I/O operations to complete
        if (blocking) {
            synchronized (stateLock) {
                assert state == ST_CLOSING;
                long reader = readerThread;
                long writer = writerThread;
                if (reader != 0 || writer != 0) {
                    nd.preClose(fd);
                    connected = false; // fd is no longer connected socket

                    if (reader != 0)
                        NativeThread.signal(reader);
                    if (writer != 0)
                        NativeThread.signal(writer);

                    // wait for blocking I/O operations to end
                    while (readerThread != 0 || writerThread != 0) {
                        try {
                            stateLock.wait();
                        } catch (InterruptedException e) {
                            interrupted = true;
                        }
                    }
                }
            }
        } else {
            // non-blocking mode: wait for read/write to complete
            readLock.lock();
            try {
                writeLock.lock();
                writeLock.unlock();
            } finally {
                readLock.unlock();
            }
        }

        // set state to ST_KILLPENDING
        synchronized (stateLock) {
            assert state == ST_CLOSING;
            // if connected and the channel is registered with a Selector then
            // shutdown the output if possible so that the peer reads EOF.
            if (connected && isRegistered()) {
                try {
                    RdmaNet.shutdown(fd, Net.SHUT_WR);
                } catch (IOException ignore) { }
            }
            state = ST_KILLPENDING;
        }

        // close socket if not registered with Selector
        if (!isRegistered())
            kill();

        // restore interrupt status
        if (interrupted)
            Thread.currentThread().interrupt();
    }

    @Override
    public void kill() throws IOException {
        synchronized (stateLock) {
            if (state == ST_KILLPENDING) {
                state = ST_KILLED;
                nd.rdmaClose(fd);
            }
        }
    }

    @Override
    public SocketChannel shutdownInput() throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            if (!isConnected())
                throw new NotYetConnectedException();
            if (!isInputClosed) {
                RdmaNet.shutdown(fd, Net.SHUT_RD);
                long thread = readerThread;
                if (thread != 0)
                    NativeThread.signal(thread);
                isInputClosed = true;
            }
            return this;
        }
    }

    @Override
    public SocketChannel shutdownOutput() throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            if (!isConnected())
                throw new NotYetConnectedException();
            if (!isOutputClosed) {
                RdmaNet.shutdown(fd, Net.SHUT_WR);
                long thread = writerThread;
                if (thread != 0)
                    NativeThread.signal(thread);
                isOutputClosed = true;
            }
            return this;
        }
    }

    boolean isInputOpen() {
        return !isInputClosed;
    }

    boolean isOutputOpen() {
        return !isOutputClosed;
    }

    /**
     * Poll this channel's socket for reading up to the given timeout.
     * @return {@code true} if the socket is polled
     */
    boolean pollRead(long timeout) throws IOException {
        boolean blocking = isBlocking();
        assert Thread.holdsLock(blockingLock()) && blocking;

        readLock.lock();
        try {
            boolean polled = false;
            try {
                beginRead(blocking);
                int events = RdmaNet.poll(fd, Net.POLLIN, timeout);
                polled = (events != 0);
            } finally {
                endRead(blocking, polled);
            }
            return polled;
        } finally {
            readLock.unlock();
        }
    }

    /**
     * Poll this channel's socket for a connection, up to the given timeout.
     * @return {@code true} if the socket is polled
     */
    boolean pollConnected(long timeout) throws IOException {
        boolean blocking = isBlocking();
        assert Thread.holdsLock(blockingLock()) && blocking;

        readLock.lock();
        try {
            writeLock.lock();
            try {
                boolean polled = false;
                try {
                    beginFinishConnect(blocking);
                    int events = RdmaNet.poll(fd, Net.POLLCONN, timeout);
                    polled = (events != 0);
                } finally {
                    // invoke endFinishConnect with completed = false so that
                    // the state is not changed to ST_CONNECTED. The socket
                    // adaptor will use finishConnect to finish.
                    endFinishConnect(blocking, /*completed*/false);
                }
                return polled;
            } finally {
                writeLock.unlock();
            }
        } finally {
            readLock.unlock();
        }
    }

    public boolean translateReadyOps(int ops, int initialOps,
            SelectionKeyImpl ski) {
        int intOps = ski.nioInterestOps();
        int oldOps = ski.nioReadyOps();
        int newOps = initialOps;

        if ((ops & Net.POLLNVAL) != 0) {
            return false;
        }

        if ((ops & (Net.POLLERR | Net.POLLHUP)) != 0) {
            newOps = intOps;
            ski.nioReadyOps(newOps);
            return (newOps & ~oldOps) != 0;
        }

        boolean connected = isConnected();
        if (((ops & Net.POLLIN) != 0) &&
            ((intOps & SelectionKey.OP_READ) != 0) && connected)
            newOps |= SelectionKey.OP_READ;

        if (((ops & Net.POLLCONN) != 0) &&
            ((intOps & SelectionKey.OP_CONNECT) != 0) && isConnectionPending())
            newOps |= SelectionKey.OP_CONNECT;

        if (((ops & Net.POLLOUT) != 0) &&
            ((intOps & SelectionKey.OP_WRITE) != 0) && connected)
            newOps |= SelectionKey.OP_WRITE;

        ski.nioReadyOps(newOps);
        return (newOps & ~oldOps) != 0;
    }

    public boolean translateAndUpdateReadyOps(int ops, SelectionKeyImpl ski) {
        return translateReadyOps(ops, ski.nioReadyOps(), ski);
    }

    public boolean translateAndSetReadyOps(int ops, SelectionKeyImpl ski) {
        return translateReadyOps(ops, 0, ski);
    }

    public int translateInterestOps(int ops) {
        int newOps = 0;
        if ((ops & SelectionKey.OP_READ) != 0)
            newOps |= Net.POLLIN;
        if ((ops & SelectionKey.OP_WRITE) != 0)
            newOps |= Net.POLLOUT;
        if ((ops & SelectionKey.OP_CONNECT) != 0)
            newOps |= Net.POLLCONN;
        return newOps;
    }

    public FileDescriptor getFD() {
        return fd;
    }

    public int getFDVal() {
        return fdVal;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(this.getClass().getSuperclass().getName());
        sb.append('[');
        if (!isOpen())
            sb.append("closed");
        else {
            synchronized (stateLock) {
                switch (state) {
                case ST_UNCONNECTED:
                    sb.append("unconnected");
                    break;
                case ST_CONNECTIONPENDING:
                    sb.append("connection-pending");
                    break;
                case ST_CONNECTED:
                    sb.append("connected");
                    if (isInputClosed)
                        sb.append(" ishut");
                    if (isOutputClosed)
                        sb.append(" oshut");
                    break;
                }
                InetSocketAddress addr = localAddress();
                if (addr != null) {
                    sb.append(" local=");
                    sb.append(NET_ACCESS.getRevealedLocalAddressAsString(addr));
                }
                if (remoteAddress() != null) {
                    sb.append(" remote=");
                    sb.append(remoteAddress().toString());
                }
            }
        }
        sb.append(']');
        return sb.toString();
    }

    @Override
    public void translateAndSetInterestOps(int ops, SelectionKeyImpl sk) {
        int newOps = 0;
        if ((ops & SelectionKey.OP_READ) != 0)
            newOps |= Net.POLLIN;
        if ((ops & SelectionKey.OP_WRITE) != 0)
            newOps |= Net.POLLOUT;
        if ((ops & SelectionKey.OP_CONNECT) != 0)
            newOps |= Net.POLLCONN;
        sk.selector.putEventOps(sk, newOps);
    }
// -- Native methods --

    private static native void initIDs() throws UnsupportedOperationException;

    private static native int checkConnect(FileDescriptor fd, boolean block)
            throws IOException;

    private static native int sendOutOfBandData(FileDescriptor fd, byte data)
            throws IOException;

    static {
        IOUtil.load();
        System.loadLibrary("extnet");
        UnsupportedOperationException uoe = null;
        try {
            initIDs();
        } catch (UnsupportedOperationException e) {
            uoe = e;
        }
        unsupported = uoe;
        nd = new RdmaSocketDispatcher();
    }
}
