/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

import java.io.Closeable;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.StandardProtocolFamily;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import jdk.net.RdmaSockets;

import jdk.test.lib.Utils;


public class TestServers {

    private TestServers() { }

    /**
     * An abstract server identifies a server which listens on a port on on a
     * given machine.
     */
    static abstract class AbstractServer {

        private AbstractServer() {
        }

        public abstract int getPort();

        public abstract InetAddress getAddress();
    }

    /**
     * A downgraded type of AbstractServer which will refuse connections. Note:
     * use it once and throw it away - this implementation opens an anonymous
     * socket and closes it, returning the address of the closed socket. If
     * other servers are started afterwards, the address/port might get reused
     * and become connectable again - so it's not a good idea to assume that
     * connections using this address/port will always be refused. Connections
     * will be refused as long as the address/port of the refusing server has
     * not been reused.
     */
    static class RefusingServer extends AbstractServer {

        final InetAddress address;
        final int port;

        private RefusingServer(InetAddress address, int port) {
            this.address = address;
            this.port = port;
        }

        @Override
        public int getPort() {
            return port;
        }

        @Override
        public InetAddress getAddress() {
            return address;
        }

        public static RefusingServer newRefusingServer() throws IOException {
            return new RefusingServer(InetAddress.getLocalHost(),
                       Utils.refusingEndpoint().getPort());
        }
    }

    /**
     * An abstract class for implementing small TCP servers for the nio tests
     * purposes. Disclaimer: This is a naive implementation that uses the old
     * networking APIs (not those from {@code java.nio.*}) and shamelessly
     * extends/creates Threads instead of using an executor service.
     */
    static abstract class AbstractTcpServer extends AbstractServer
            implements Runnable, Closeable {

        protected final long linger; // #of ms to wait before responding
        private Thread acceptThread; // thread waiting for accept
        // list of opened connections that should be closed on close.
        private List<TcpConnectionThread> connections = new ArrayList<>();
        private ServerSocket serverSocket; // the server socket
        private boolean started = false; // whether the server is started
        Throwable error = null;

        /**
         * Creates a new abstract TCP server.
         *
         * @param linger the amount of time the server should wait before
         * responding to requests.
         */
        protected AbstractTcpServer(long linger) {
            this.linger = linger;
        }

        /**
         * The local port to which the server is bound.
         *
         * @return The local port to which the server is bound.
         * @exception IllegalStateException is thrown if the server is not
         * started.
         */
        @Override
        public final synchronized int getPort() {
            if (!started) {
                throw new IllegalStateException("Not started");
            }
            return serverSocket.getLocalPort();
        }

        /**
         * The local address to which the server is bound.
         *
         * @return The local address to which the server is bound.
         * @exception IllegalStateException is thrown if the server is not
         * started.
         */
        @Override
        public final synchronized InetAddress getAddress() {
            if (!started) {
                throw new IllegalStateException("Not started");
            }
            return serverSocket.getInetAddress();
        }

        /**
         * Tells whether the server is started.
         *
         * @return true if the server is started.
         */
        public final synchronized boolean isStarted() {
            return started;
        }

        /**
         * Creates a new server socket.
         *
         * @param port local port to bind to.
         * @param backlog requested maximum length of the queue of incoming
         * connections.
         * @param address local address to bind to.
         * @return a new bound server socket ready to accept connections.
         * @throws IOException if the socket cannot be created or bound.
         */
        protected ServerSocket newServerSocket(int port, int backlog,
                InetAddress address)
                throws IOException {
            ServerSocket ss = RdmaSockets.openServerSocket(StandardProtocolFamily.INET);
            ss.bind(new InetSocketAddress(address, port), backlog);
            return ss;
        }

        /**
         * Starts listening for connections.
         *
         * @throws IOException if the server socket cannot be created or bound.
         */
        public final synchronized void start() throws IOException {
            if (started) {
                return;
            }
            final ServerSocket socket =
                    newServerSocket(0, 100, InetAddress.getLocalHost());
            serverSocket = socket;
            acceptThread = new Thread(this);
            acceptThread.setDaemon(true);
            acceptThread.start();
            started = true;
        }

        /**
         * Calls {@code Thread.sleep(linger);}
         */
        protected final void lingerIfRequired() {
            if (linger > 0) {
                try {
                    Thread.sleep(linger);
                } catch (InterruptedException x) {
                    Thread.interrupted();
                    final ServerSocket socket = serverSocket();
                    if (socket != null && !socket.isClosed()) {
                        System.err.println("Thread interrupted...");
                    }
                }
            }
        }

        final synchronized ServerSocket serverSocket() {
            return this.serverSocket;
        }

        /**
         * The main accept loop.
         */
        @Override
        public final void run() {
            final ServerSocket sSocket = serverSocket();
            try {
                Socket s;
                while (isStarted() && !Thread.interrupted()
                        && (s = sSocket.accept()) != null) {
                    lingerIfRequired();
                    listen(s);
                }
            } catch (Exception x) {
                error = x;
            } finally {
                synchronized (this) {
                    if (!sSocket.isClosed()) {
                        try {
                            sSocket.close();
                        } catch (IOException x) {
                            System.err.println("Failed to close server socket");
                        }
                    }
                    if (started && this.serverSocket == sSocket) {
                        started = false;
                        this.serverSocket = null;
                        this.acceptThread = null;
                    }
                }
            }
        }

        /**
         * Represents a connection accepted by the server.
         */
        protected abstract class TcpConnectionThread extends Thread {

            protected final Socket socket;

            protected TcpConnectionThread(Socket socket) {
                this.socket = socket;
                this.setDaemon(true);
            }

            public void close() throws IOException {
                socket.close();
                interrupt();
            }
        }

        /**
         * Creates a new TcpConnnectionThread to handle the connection through
         * an accepted socket.
         *
         * @param s the socket returned by {@code serverSocket.accept()}.
         * @return a new TcpConnnectionThread to handle the connection through
         * an accepted socket.
         */
        protected abstract TcpConnectionThread createConnection(Socket s);

        /**
         * Creates and starts a new TcpConnectionThread to handle the accepted
         * socket.
         *
         * @param s the socket returned by {@code serverSocket.accept()}.
         */
        private synchronized void listen(Socket s) {
            TcpConnectionThread c = createConnection(s);
            c.start();
            addConnection(c);
        }

        /**
         * Add the connection to the list of accepted connections.
         *
         * @param connection an accepted connection.
         */
        protected synchronized void addConnection(
                TcpConnectionThread connection) {
            connections.add(connection);
        }

        /**
         * Remove the connection from the list of accepted connections.
         *
         * @param connection an accepted connection.
         */
        protected synchronized void removeConnection(
                TcpConnectionThread connection) {
            connections.remove(connection);
        }

        /**
         * Close the server socket and all the connections present in the list
         * of accepted connections.
         *
         * @throws IOException
         */
        @Override
        public synchronized void close() throws IOException {
            if (serverSocket != null && !serverSocket.isClosed()) {
                serverSocket.close();
            }
            if (acceptThread != null) {
                acceptThread.interrupt();
            }
            int failed = 0;
            for (TcpConnectionThread c : connections) {
                try {
                    c.close();
                } catch (IOException x) {
                    // no matter - we're closing.
                    failed++;
                }
            }
            connections.clear();
            if (failed > 0) {
                throw new IOException("Failed to close some connections");
            }
        }
    }

    /**
     * A small TCP Server that emulates the echo service for tests purposes. See
     * http://en.wikipedia.org/wiki/Echo_Protocol This server uses an anonymous
     * port - NOT the standard port 7. We don't guarantee that its behavior
     * exactly matches the RFC - the only purpose of this server is to have
     * something that responds to nio tests...
     */
    static class EchoServer extends AbstractTcpServer {

        public EchoServer() {
            this(0L);
        }

        public EchoServer(long linger) {
            super(linger);
        }

        @Override
        protected TcpConnectionThread createConnection(Socket s) {
            return new EchoConnection(s);
        }

        private final class EchoConnection extends TcpConnectionThread {

            public EchoConnection(Socket socket) {
                super(socket);
            }

            @Override
            public void run() {
                try {
                    final InputStream is = socket.getInputStream();
                    final OutputStream out = socket.getOutputStream();
                    byte[] b = new byte[255];
                    int n;
                    while ((n = is.read(b)) > 0) {
                        lingerIfRequired();
                        out.write(b, 0, n);
                    }
                } catch (IOException io) {
                    // fall through to finally
                } finally {
                    if (!socket.isClosed()) {
                        try {
                            socket.close();
                        } catch (IOException x) {
                            System.err.println(
                                    "Failed to close echo connection socket");
                        }
                    }
                    removeConnection(this);
                }
            }
        }

        public static EchoServer startNewServer() throws IOException {
            return startNewServer(0);
        }

        public static EchoServer startNewServer(long linger) throws IOException {
            final EchoServer echoServer = new EchoServer(linger);
            echoServer.start();
            return echoServer;
        }
    }


    /**
     * A small TCP Server that accept connections but does not response to any input.
     */
    static final class NoResponseServer extends EchoServer {
        public NoResponseServer() {
            super(Long.MAX_VALUE);
        }

        public static NoResponseServer startNewServer() throws IOException {
            final NoResponseServer noResponseServer = new NoResponseServer();
            noResponseServer.start();
            return noResponseServer;
        }
    }


    /**
     * A small TCP server that emulates the Day & Time service for tests
     * purposes. See http://en.wikipedia.org/wiki/Daytime_Protocol This server
     * uses an anonymous port - NOT the standard port 13. We don't guarantee
     * that its behavior exactly matches the RFC - the only purpose of this
     * server is to have something that responds to nio tests...
     */
    static final class DayTimeServer extends AbstractTcpServer {

        public DayTimeServer() {
            this(0L);
        }

        public DayTimeServer(long linger) {
            super(linger);
        }

        @Override
        protected TcpConnectionThread createConnection(Socket s) {
            return new DayTimeServerConnection(s);
        }

        @Override
        protected void addConnection(TcpConnectionThread connection) {
            // do nothing - the connection just write the date and terminates.
        }

        @Override
        protected void removeConnection(TcpConnectionThread connection) {
            // do nothing - we're not adding connections to the list...
        }

        private final class DayTimeServerConnection extends TcpConnectionThread {

            public DayTimeServerConnection(Socket socket) {
                super(socket);
            }

            @Override
            public void run() {
                try {
                    final OutputStream out = socket.getOutputStream();
                    lingerIfRequired();
                    out.write(new Date(System.currentTimeMillis())
                            .toString().getBytes("US-ASCII"));
                    out.flush();
                } catch (IOException io) {
                    // fall through to finally
                } finally {
                    if (!socket.isClosed()) {
                        try {
                            socket.close();
                        } catch (IOException x) {
                            System.err.println(
                                    "Failed to close echo connection socket");
                        }
                    }
                }
            }
        }

        public static DayTimeServer startNewServer()
                throws IOException {
            return startNewServer(0);
        }

        public static DayTimeServer startNewServer(long linger)
                throws IOException {
            final DayTimeServer daytimeServer = new DayTimeServer(linger);
            daytimeServer.start();
            return daytimeServer;
        }
    }
}
