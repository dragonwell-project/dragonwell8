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
import java.nio.channels.*;
import java.nio.channels.spi.SelectorProvider;
import java.util.Collections;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.locks.ReentrantLock;

import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;

public class RdmaServerSocketChannelImpl
    extends ServerSocketChannel
    implements SelChImpl
{
    //The protocol family of the socket
    private final ProtocolFamily family;

    private static RdmaSocketDispatcher nd;

    private final FileDescriptor fd;
    private final int fdVal;

    private final ReentrantLock acceptLock = new ReentrantLock();

    private final Object stateLock = new Object();

    private static final int ST_INUSE = 0;
    private static final int ST_CLOSING = 1;
    private static final int ST_KILLPENDING = 2;
    private static final int ST_KILLED = 3;
    private static final NetAccess NET_ACCESS = SharedSecrets.getNetAccess();
    private int state;

    private long thread;

    private InetSocketAddress localAddress;

    private boolean isReuseAddress;

    private ServerSocket socket;

    private static final UnsupportedOperationException unsupported;

    private static final SelectorProvider checkSupported(SelectorProvider sp) {
        if (unsupported != null)
            throw new UnsupportedOperationException(unsupported.getMessage(), unsupported);
        else
            return sp;
    }

    RdmaServerSocketChannelImpl(SelectorProvider sp, ProtocolFamily family)
            throws IOException {
        super(checkSupported(sp));
        Objects.requireNonNull(family, "'family' is null");
        if (!(family == INET || family == INET6)) {
            throw new UnsupportedOperationException(
                    "Protocol family not supported");
        }
        if (family == INET6) {
            if (!NET_ACCESS.isIPv6Available()) {
                throw new UnsupportedOperationException(
                        "IPv6 not available");
            }
        }
        this.family = family;
        this.fd = RdmaNet.serverSocket(family, true);
        this.fdVal = IOUtil.fdVal(fd);
    }

    private void ensureOpen() throws ClosedChannelException {
        if (!isOpen())
            throw new ClosedChannelException();
    }

    @Override
    public ServerSocket socket() {
        synchronized (stateLock) {
            if (socket == null)
                socket = RdmaServerSocketAdaptor.create(this);
            return socket;
        }
    }

    @Override
    public SocketAddress getLocalAddress() throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            return (localAddress == null)
                    ? null
                    : NET_ACCESS.getRevealedLocalAddress(localAddress);
        }
    }

    @Override
    public <T> ServerSocketChannel setOption(SocketOption<T> name, T value)
            throws IOException
    {
        Objects.requireNonNull(name);
        if (!supportedOptions().contains(name))
            throw new UnsupportedOperationException("'" + name
                    + "' not supported");
        synchronized (stateLock) {
            ensureOpen();
            if (isBound() && (name == StandardSocketOptions.SO_REUSEADDR))
                throw new UnsupportedOperationException(
                        "RDMA server socket channel cannot set the socket option "
                        + name.toString() + " after bind.");

            RdmaNet.setSocketOption(fd, RdmaNet.UNSPEC, name, value);
            return this;
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T> T getOption(SocketOption<T> name)
            throws IOException
    {
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
            HashSet<SocketOption<?>> set = new HashSet<>(2);
            set.add(StandardSocketOptions.SO_RCVBUF);
            set.add(StandardSocketOptions.SO_REUSEADDR);
            if (RdmaNet.isRdmaAvailable()) {
                RdmaSocketOptions rdmaOptions =
                        RdmaSocketOptions.getInstance();
                set.addAll(rdmaOptions.options());
            }
            return Collections.unmodifiableSet(set);
        }
    }

    public final Set<SocketOption<?>> supportedOptions() {
        return DefaultOptionsHolder.defaultOptions;
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
    public ServerSocketChannel bind(SocketAddress local, int backlog)
            throws IOException {
        synchronized (stateLock) {
            ensureOpen();
            if (localAddress != null)
                throw new AlreadyBoundException();
            InetSocketAddress isa = (local == null)
                                    ? anyLocalAddress()
                                    : RdmaNet.checkAddress(local, family);
            SecurityManager sm = System.getSecurityManager();
            if (sm != null)
                sm.checkListen(isa.getPort());
            RdmaNet.bind(family, fd, isa.getAddress(), isa.getPort());
            RdmaNet.listen(fd, backlog < 1 ? 50 : backlog);
            localAddress = RdmaNet.localAddress(fd);
        }
        return this;
    }

    private void begin(boolean blocking) throws ClosedChannelException {
        if (blocking)
            begin();
        synchronized (stateLock) {
            ensureOpen();
            if (localAddress == null)
                throw new NotYetBoundException();
            if (blocking)
                thread = NativeThread.current();
        }
    }

    private void end(boolean blocking, boolean completed)
            throws AsynchronousCloseException {
        if (blocking) {
            synchronized (stateLock) {
                thread = 0;
                if (state == ST_CLOSING) {
                    stateLock.notifyAll();
                }
            }
            end(completed);
        }
    }

    @Override
    public SocketChannel accept() throws IOException {
        acceptLock.lock();
        try {
            int n = 0;
            FileDescriptor newfd = new FileDescriptor();
            InetSocketAddress[] isaa = new InetSocketAddress[1];

            boolean blocking = isBlocking();
            try {
                begin(blocking);
                do {
                    if (blocking) {
                        do {
                            n  = checkAccept(this.fd);
                        } while ((n == 0 || n == IOStatus.INTERRUPTED)
                                && isOpen());
                    }
                    n = accept(this.fd, newfd, isaa);
                } while (n == IOStatus.INTERRUPTED && isOpen());
            } finally {
                end(blocking, n > 0);
                assert IOStatus.check(n);
            }

            if (n < 1)
                return null;

            // newly accepted socket is initially in blocking mode
            RdmaNet.configureBlocking(newfd, true);

            InetSocketAddress isa = isaa[0];
            SocketChannel sc = new RdmaSocketChannelImpl(provider(),
                    newfd, isa);

            // check permitted to accept connections from the remote address
            SecurityManager sm = System.getSecurityManager();
            if (sm != null) {
                try {
                    sm.checkAccept(isa.getAddress().getHostAddress(),
                            isa.getPort());
                } catch (SecurityException x) {
                    sc.close();
                    throw x;
                }
            }
            return sc;

        } finally {
            acceptLock.unlock();
        }
    }

    @Override
    protected void implConfigureBlocking(boolean block) throws IOException {
        acceptLock.lock();
        try {
            synchronized (stateLock) {
                ensureOpen();
                RdmaNet.configureBlocking(fd, block);
            }
        } finally {
            acceptLock.unlock();
        }
    }

    @Override
    protected void implCloseSelectableChannel() throws IOException {
        assert !isOpen();

        boolean interrupted = false;
        boolean blocking;

        // set state to ST_CLOSING
        synchronized (stateLock) {
            assert state < ST_CLOSING;
            state = ST_CLOSING;
            blocking = isBlocking();
        }

        // wait for any outstanding accept to complete
        if (blocking) {
            synchronized (stateLock) {
                assert state == ST_CLOSING;
                long th = thread;
                if (th != 0) {
                    nd.preClose(fd);
                    NativeThread.signal(th);

                    // wait for accept operation to end
                    while (thread != 0) {
                        try {
                            stateLock.wait();
                        } catch (InterruptedException e) {
                            interrupted = true;
                        }
                    }
                }
            }
        } else {
            // non-blocking mode: wait for accept to complete
            acceptLock.lock();
            acceptLock.unlock();
        }

        // set state to ST_KILLPENDING
        synchronized (stateLock) {
            assert state == ST_CLOSING;
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

    boolean isBound() {
        synchronized (stateLock) {
            return localAddress != null;
        }
    }

    InetSocketAddress localAddress() {
        synchronized (stateLock) {
            return localAddress;
        }
    }

    /**
     * Poll this channel's socket for a new connection up to the given timeout.
     * @return {@code true} if there is a connection to accept
     */
    boolean pollAccept(long timeout) throws IOException {
        assert Thread.holdsLock(blockingLock()) && isBlocking();
        acceptLock.lock();
        try {
            boolean polled = false;
            try {
                begin(true);
                int events = RdmaNet.poll(fd, Net.POLLIN, timeout);
                polled = (events != 0);
            } finally {
                end(true, polled);
            }
            return polled;
        } finally {
            acceptLock.unlock();
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

        if (((ops & Net.POLLIN) != 0) &&
            ((intOps & SelectionKey.OP_ACCEPT) != 0))
                newOps |= SelectionKey.OP_ACCEPT;

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
        if ((ops & SelectionKey.OP_ACCEPT) != 0)
            newOps |= Net.POLLIN;
        return newOps;
    }

    public FileDescriptor getFD() {
        return fd;
    }

    public int getFDVal() {
        return fdVal;
    }

    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(this.getClass().getName());
        sb.append('[');
        if (!isOpen()) {
            sb.append("closed");
        } else {
            synchronized (stateLock) {
                InetSocketAddress addr = localAddress;
                if (addr == null) {
                    sb.append("unbound");
                } else {
                    sb.append(NET_ACCESS.getRevealedLocalAddressAsString(addr));
                }
            }
        }
        sb.append(']');
        return sb.toString();
    }

    private int accept(FileDescriptor ssfd, FileDescriptor newfd,
            InetSocketAddress[] isaa) throws IOException {
        return accept0(ssfd, newfd, isaa);
    }

    @Override
    public void translateAndSetInterestOps(int ops, SelectionKeyImpl sk) {
        int newOps = 0;

        // Translate ops
        if ((ops & SelectionKey.OP_ACCEPT) != 0)
            newOps |= Net.POLLIN;
        // Place ops into pollfd array
        sk.selector.putEventOps(sk, newOps);
    }

    // -- Native methods --
    private static native int checkAccept(FileDescriptor fd)
            throws IOException;

    private native int accept0(FileDescriptor ssfd, FileDescriptor newfd,
            InetSocketAddress[] isaa) throws IOException;

    private static native void initIDs()throws UnsupportedOperationException;

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
