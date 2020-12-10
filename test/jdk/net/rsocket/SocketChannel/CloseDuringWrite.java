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

/* @test
 * @bug 8195160
 * @summary Test asynchronous close during a blocking write
 * @requires (os.family == "linux")
 * @library .. /test/lib
 * @build RsocketTest
 * @run main/othervm CloseDuringWrite
 * @key randomness
 */

import java.io.Closeable;
import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ProtocolFamily;
import java.net.SocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.Random;
import jdk.net.RdmaSockets;
import jtreg.SkippedException;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;

public class CloseDuringWrite {

    static final Random rand = new Random();

    /**
     * A task that closes a Closeable
     */
    static class Closer implements Callable<Void> {
        final Closeable c;
        Closer(Closeable c) {
            this.c = c;
        }
        public Void call() throws IOException {
            c.close();
            return null;
        }
    }

    public static void main(String[] args) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        InetAddress lh = InetAddress.getLocalHost();
        ProtocolFamily family = lh instanceof Inet6Address ? INET6 : INET;
        System.out.printf("local address: %s%n", lh);

        ScheduledExecutorService pool = Executors.newSingleThreadScheduledExecutor();
        try (ServerSocketChannel ssc = RdmaSockets.openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(lh, 0));
            int port = ssc.socket().getLocalPort();
            SocketAddress sa = new InetSocketAddress(lh, port);

            ByteBuffer bb = ByteBuffer.allocate(2 * 1024 * 1024);

            for (int i = 0; i < 20; i++) {
                System.out.println(i);
                try (SocketChannel source = RdmaSockets.openSocketChannel(family)) {
                    Runnable runnable = new Runnable() {
                        @Override
                        public void run() {
                            try {
                                System.out.println("connecting ...");
                                source.connect(sa);
                                System.out.println("connected");
                            } catch (Exception e) {
                                e.printStackTrace();
                                throw new RuntimeException("Test Failed");
                            }
                        }
                    };

                    Thread t = new Thread(runnable);
                    t.start();
                    try (SocketChannel sink = ssc.accept()) {
                        System.out.println("accepted new connection");
                        // schedule channel to be closed
                        Closer c = new Closer(source);
                        int when = 1000 + rand.nextInt(2000);
                        Future<Void> result = pool.schedule(c, when, TimeUnit.MILLISECONDS);

                        // the write should either succeed or else throw a
                        // ClosedChannelException (more likely an
                        // AsynchronousCloseException)
                        try {
                            for (;;) {
                                int limit = rand.nextInt(bb.capacity());
                                bb.position(0);
                                bb.limit(limit);
                                int n = source.write(bb);
                                System.out.format("wrote %d, expected %d%n", n, limit);
                            }
                        } catch (ClosedChannelException expected) {
                            System.out.println(expected + " (expected)");
                        } finally {
                            result.get();
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    throw new RuntimeException("Test Failed");
                }
            }
        } finally {
            pool.shutdown();
        }
    }
}
