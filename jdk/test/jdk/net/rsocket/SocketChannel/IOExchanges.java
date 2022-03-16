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

/*
 * @test
 * @bug 8195160
 * @summary Tests various combinations of blocking/nonblocking connections
 * @requires (os.family == "linux")
 * @library ../ /test/lib
 * @build RsocketTest
 * @run testng/othervm IOExchanges
 * @author chegar
 */

import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ProtocolFamily;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.ByteBuffer;
import jdk.net.RdmaSockets;
import org.testng.SkipException;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.Test;
import static java.lang.System.out;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;
import static java.nio.channels.SelectionKey.OP_ACCEPT;
import static java.nio.channels.SelectionKey.OP_READ;
import static java.nio.channels.SelectionKey.OP_WRITE;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

public class IOExchanges {

    // Whether, or not, to use RDMA channels or regular socket channels.
    // For test assertion purposes during development.
    static final boolean useRDMA = true;

    static InetAddress addr;
    static ProtocolFamily family;

    @BeforeTest
    public void setup() throws Exception {
        if (useRDMA && !RsocketTest.isRsocketAvailable())
            throw new SkipException("rsocket is not available");

        addr = InetAddress.getLocalHost();
        family = addr instanceof Inet6Address ? INET6 : INET;
        out.printf("local address: %s%n", addr);
        out.printf("useRDMA: %b%n", useRDMA);
    }

    static SocketChannel openSocketChannel(ProtocolFamily family)
        throws IOException
    {
        return useRDMA ? RdmaSockets.openSocketChannel(family)
                       : SocketChannel.open();
    }
    static ServerSocketChannel openServerSocketChannel(ProtocolFamily family)
        throws IOException
    {
        return useRDMA ? RdmaSockets.openServerSocketChannel(family)
                       : ServerSocketChannel.open();
    }
    static Selector openSelector( ) throws IOException {
        return useRDMA ? RdmaSockets.openSelector() : Selector.open();
    }

    /*
     The following, non-exhaustive set, of tests exercise different combinations
     of blocking and non-blocking accept/connect calls along with I/O
     operations, that exchange a single byte. The intent it to test a reasonable
     set of blocking and non-blocking scenarios.

      The individual test method names follow their test scenario.
       [BAccep|SELNBAccep|SPINNBAccep] - Accept either:
                        blocking, select-non-blocking, spinning-non-blocking
       [BConn|NBConn] - blocking connect / non-blocking connect
       [BIO|NBIO]     - blocking / non-blocking I/O operations (read/write)
       [WR|RW] - connecting thread write/accepting thread reads first, and vice-versa
       [Id]    - unique test Id

        BAccep_BConn_BIO_WR_1
        BAccep_BConn_BIO_RW_2
        SELNBAccep_BConn_BIO_WR_3
        SELNBAccep_BConn_BIO_RW_4
        SPINNBAccep_BConn_BIO_WR_5
        SPINNBAccep_BConn_BIO_RW_6
        BAccep_NBConn_BIO_WR_7
        BAccep_NBConn_BIO_RW_8
        SELNBAccep_NBConn_BIO_WR_9
        SELNBAccep_NBConn_BIO_RW_10
        SPINNBAccep_NBConn_BIO_WR_11
        SPINNBAccep_NBConn_BIO_RW_12

        BAccep_BConn_NBIO_WR_1a        << Non-Blocking I/O
        BAccep_BConn_NBIO_RW_2a
        SELNBAccep_BConn_NBIO_WR_3a
        SELNBAccep_BConn_NBIO_RW_4a
        SPINNBAccep_BConn_NBIO_WR_5a
        SPINNBAccep_BConn_NBIO_RW_6a
        BAccep_NBConn_NBIO_WR_7a
        BAccep_NBConn_NBIO_RW_8a
        SELNBAccep_NBConn_NBIO_WR_9a
        SELNBAccep_NBConn_NBIO_RW_10a
        SPINBAccep_NBConn_NBIO_WR_11a
        SPINBAccep_NBConn_NBIO_RW_12a
    */


    @Test
    public void BAccep_BConn_BIO_WR_1() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t1", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x01).flip();
                    assertEquals(sc.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc.read(bb.clear()), -1);
                }
            });
            t.start();

            try (SocketChannel sc = ssc.accept()) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                assertEquals(sc.read(bb), 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x01);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void BAccep_BConn_BIO_RW_2() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t2", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x02);
                }
            });
            t.start();

            try (SocketChannel sc = ssc.accept()) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x02).flip();
                assertEquals(sc.write(bb), 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                assertEquals(sc.read(bb.clear()), -1);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SELNBAccep_BConn_BIO_WR_3() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family);
             Selector selector = openSelector()) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t3", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x03).flip();
                    assertEquals(sc.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc.read(bb.clear()), -1);
                }
            });
            t.start();

            ssc.configureBlocking(false).register(selector, OP_ACCEPT);
            assertEquals(selector.select(), 1);

            try (SocketChannel sc = ssc.accept()) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                assertEquals(sc.read(bb), 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x03);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SELNBAccep_BConn_BIO_RW_4() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family);
             Selector selector = openSelector()) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t4", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x04);
                }
            });
            t.start();

            ssc.configureBlocking(false).register(selector, OP_ACCEPT);
            assertEquals(selector.select(), 1);

            try (SocketChannel sc = ssc.accept()) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x04).flip();
                assertEquals(sc.write(bb), 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                assertEquals(sc.read(bb.clear()), -1);

            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SPINNBAccep_BConn_BIO_WR_5() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t5", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x05).flip();
                    assertEquals(sc.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc.read(bb.clear()), -1);
                }
            });
            t.start();

            SocketChannel accepted;
            for (;;) {
                accepted = ssc.accept();
                if (accepted != null) {
                    out.println("accepted new connection");
                    break;
                }
                Thread.onSpinWait();
            }

            try (SocketChannel sc = accepted) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                assertEquals(sc.read(bb), 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x05);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SPINNBAccep_BConn_BIO_RW_6() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t6", () -> {
                try (SocketChannel sc = openSocketChannel(family)) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x06);
                }
            });
            t.start();

            SocketChannel accepted;
            for (;;) {
                accepted = ssc.accept();
                if (accepted != null) {
                    out.println("accepted new connection");
                    break;
                }
                Thread.onSpinWait();
            }

            try (SocketChannel sc = accepted) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x06).flip();
                assertEquals(sc.write(bb), 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                assertEquals(sc.read(bb.clear()), -1);

            }
            t.awaitCompletion();
        }
    }

    // Similar to the previous six scenarios, but with same-thread
    // non-blocking connect.
/*
    @Test
    public void BAccep_NBConn_BIO_WR_7() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t7", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x07).flip();
                        assertEquals(sc.write(bb), 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        assertEquals(sc.read(bb.clear()), -1);
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc2.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x07);
                    sc2.shutdownOutput();
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void BAccep_NBConn_BIO_RW_8() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t8", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10);
                        assertEquals(sc.read(bb), 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), 0x08);
                        sc.shutdownOutput();
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x08).flip();
                    assertEquals(sc2.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc2.read(bb.clear()), -1);
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SELNBAccep_NBConn_BIO_WR_9() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family);
                 Selector selector = openSelector()) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                ssc.configureBlocking(false).register(selector, OP_ACCEPT);
                assertEquals(selector.select(), 1);

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t9", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x09).flip();
                        assertEquals(sc.write(bb), 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        assertEquals(sc.read(bb.clear()), -1);
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc2.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x09);
                    sc2.shutdownOutput();
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SELNBAccep_NBConn_BIO_RW_10() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family);
                 Selector selector = openSelector()) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                ssc.configureBlocking(false).register(selector, OP_ACCEPT);
                assertEquals(selector.select(), 1);

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t10", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10);
                        assertEquals(sc.read(bb), 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), 0x10);
                        sc.shutdownOutput();
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x10).flip();
                    assertEquals(sc2.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc2.read(bb.clear()), -1);
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SPINNBAccep_NBConn_BIO_WR_11() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                SocketChannel accepted;
                for (;;) {
                    accepted = ssc.accept();
                    if (accepted != null) {
                        out.println("accepted new connection");
                        break;
                    }
                    Thread.onSpinWait();
                }

                try (SocketChannel sc2 = accepted) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t11", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x11).flip();
                        assertEquals(sc.write(bb), 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        assertEquals(sc.read(bb.clear()), -1);
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    assertEquals(sc2.read(bb), 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x11);
                    sc2.shutdownOutput();
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SPINNBAccep_NBConn_BIO_RW_12() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                SocketChannel accepted;
                for (;;) {
                    accepted = ssc.accept();
                    if (accepted != null) {
                        out.println("accepted new connection");
                        break;
                    }
                    Thread.onSpinWait();
                }

                try (SocketChannel sc2 = accepted) {
                    assertTrue(sc.finishConnect());
                    sc.configureBlocking(true);
                    TestThread t = TestThread.of("t12", () -> {
                        ByteBuffer bb = ByteBuffer.allocate(10);
                        assertEquals(sc.read(bb), 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), 0x12);
                        sc.shutdownOutput();
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x12).flip();
                    assertEquals(sc2.write(bb), 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    assertEquals(sc2.read(bb.clear()), -1);
                    t.awaitCompletion();
                }
            }
        }
    }
*/
    // ---
    // Similar to the previous twelve scenarios but with non-blocking IO
    // ---

    @Test
    public void BAccep_BConn_NBIO_WR_1a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t1a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x1A).flip();
                    sc.configureBlocking(false);
                    SelectionKey k = sc.register(selector, OP_WRITE);
                    selector.select();
                    int c;
                    while ((c = sc.write(bb)) < 1);
                    assertEquals(c, 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    k.interestOps(OP_READ);
                    selector.select();
                    bb.clear();
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, -1);
                }
            });
            t.start();

            try (SocketChannel sc = ssc.accept();
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                sc.configureBlocking(false);
                sc.register(selector, OP_READ);
                selector.select();
                int c;
                while ((c = sc.read(bb)) == 0) ;
                assertEquals(c, 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x1A);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void BAccep_BConn_NBIO_RW_2a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t2a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc.configureBlocking(false);
                    sc.register(selector, OP_READ);
                    selector.select();
                    int c;
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x2A);
                }
            });
            t.start();

            try (SocketChannel sc = ssc.accept();
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x2A).flip();
                sc.configureBlocking(false);
                SelectionKey k = sc.register(selector, OP_WRITE);
                selector.select();
                int c;
                while ((c = sc.write(bb)) < 1);
                assertEquals(c, 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                k.interestOps(OP_READ);
                selector.select();
                bb.clear();
                while ((c = sc.read(bb)) == 0);
                assertEquals(c, -1);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SELNBAccep_BConn_NBIO_WR_3a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family);
             Selector aselector = openSelector()) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t3a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x3A).flip();
                    sc.configureBlocking(false);
                    SelectionKey k = sc.register(selector, OP_WRITE);
                    selector.select();
                    int c;
                    while ((c = sc.write(bb)) < 1);
                    assertEquals(c, 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    k.interestOps(OP_READ);
                    selector.select();
                    bb.clear();
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, -1);
                }
            });
            t.start();

            ssc.configureBlocking(false).register(aselector, OP_ACCEPT);
            assertEquals(aselector.select(), 1);

            try (SocketChannel sc = ssc.accept();
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                sc.configureBlocking(false);
                sc.register(selector, OP_READ);
                selector.select();
                int c;
                while ((c = sc.read(bb)) == 0) ;
                assertEquals(c, 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x3A);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SELNBAccep_BConn_NBIO_RW_4a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family);
             Selector aselector = openSelector()) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t4a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc.configureBlocking(false);
                    sc.register(selector, OP_READ);
                    selector.select();
                    int c;
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x4A);
                }
            });
            t.start();

            ssc.configureBlocking(false).register(aselector, OP_ACCEPT);
            assertEquals(aselector.select(), 1);

            try (SocketChannel sc = ssc.accept();
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x4A).flip();
                sc.configureBlocking(false);
                SelectionKey k = sc.register(selector, OP_WRITE);
                selector.select();
                int c;
                while ((c = sc.write(bb)) < 1);
                assertEquals(c, 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                k.interestOps(OP_READ);
                selector.select();
                bb.clear();
                while ((c = sc.read(bb)) == 0);
                assertEquals(c, -1);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SPINNBAccep_BConn_NBIO_WR_5a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t5a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x5A).flip();
                    sc.configureBlocking(false);
                    SelectionKey k = sc.register(selector, OP_WRITE);
                    selector.select();
                    int c;
                    while ((c = sc.write(bb)) < 1);
                    assertEquals(c, 1);
                    out.printf("wrote: 0x%x%n", bb.get(0));
                    k.interestOps(OP_READ);
                    selector.select();
                    bb.clear();
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, -1);
                }
            });
            t.start();

            SocketChannel accepted;
            for (;;) {
                accepted = ssc.accept();
                if (accepted != null) {
                    out.println("accepted new connection");
                    break;
                }
                Thread.onSpinWait();
            }

            try (SocketChannel sc = accepted;
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10);
                sc.configureBlocking(false);
                sc.register(selector, OP_READ);
                selector.select();
                int c;
                while ((c = sc.read(bb)) == 0) ;
                assertEquals(c, 1);
                out.printf("read:  0x%x%n", bb.get(0));
                assertEquals(bb.get(0), 0x5A);
            }
            t.awaitCompletion();
        }
    }

    @Test
    public void SPINNBAccep_BConn_NBIO_RW_6a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            TestThread t = TestThread.of("t6a", () -> {
                try (SocketChannel sc = openSocketChannel(family);
                     Selector selector = openSelector()) {
                    assertTrue(sc.connect(new InetSocketAddress(addr, port)));
                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc.configureBlocking(false);
                    sc.register(selector, OP_READ);
                    selector.select();
                    int c;
                    while ((c = sc.read(bb)) == 0);
                    assertEquals(c, 1);
                    out.printf("read:  0x%x%n", bb.get(0));
                    assertEquals(bb.get(0), 0x6A);
                }
            });
            t.start();

            SocketChannel accepted;
            for (;;) {
                accepted = ssc.accept();
                if (accepted != null) {
                    out.println("accepted new connection");
                    break;
                }
                Thread.onSpinWait();
            }

            try (SocketChannel sc = accepted;
                 Selector selector = openSelector()) {
                ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x6A).flip();
                sc.configureBlocking(false);
                SelectionKey k = sc.register(selector, OP_WRITE);
                selector.select();
                int c;
                while ((c = sc.write(bb)) < 1);
                assertEquals(c, 1);
                out.printf("wrote: 0x%x%n", bb.get(0));
                k.interestOps(OP_READ);
                selector.select();
                bb.clear();
                while ((c = sc.read(bb)) == 0);
                assertEquals(c, -1);

            }
            t.awaitCompletion();
        }
    }

    // Similar to the previous six scenarios but with same-thread
    // non-blocking connect.
/*
    @Test
    public void BAccep_NBConn_NBIO_WR_7a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t7a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x7A).flip();
                            sc.configureBlocking(false);
                            SelectionKey k = sc.register(selector, OP_WRITE);
                            selector.select();
                            int c;
                            while ((c = sc.write(bb)) < 1) ;
                            assertEquals(c, 1);
                            out.printf("wrote: 0x%x%n", bb.get(0));
                            k.interestOps(OP_READ);
                            selector.select();
                            bb.clear();
                            while ((c = sc.read(bb)) == 0) ;
                            assertEquals(c, -1);
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        sc2.register(selector, OP_READ);
                        selector.select();
                        int c;
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), 0x7A);
                        sc2.shutdownOutput();
                    }
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void BAccep_NBConn_NBIO_RW_8a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t8a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10);
                            sc.register(selector, OP_READ);
                            selector.select();
                            int c;
                            while ((c = sc.read(bb)) == 0);
                            assertEquals(c, 1);
                            out.printf("read:  0x%x%n", bb.get(0));
                            assertEquals(bb.get(0), (byte)0x8A);
                            sc.shutdownOutput();
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x8A).flip();
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        SelectionKey k = sc2.register(selector, OP_WRITE);
                        selector.select();
                        int c;
                        while ((c = sc2.write(bb)) < 1) ;
                        assertEquals(c, 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        k.interestOps(OP_READ);
                        selector.select();
                        bb.clear();
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, -1);
                    }
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SELNBAccep_NBConn_NBIO_WR_9a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                Selector aselector = openSelector();
                ssc.configureBlocking(false).register(aselector, OP_ACCEPT);
                assertEquals(aselector.select(), 1);

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t9a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0x9A).flip();
                            sc.configureBlocking(false);
                            SelectionKey k = sc.register(selector, OP_WRITE);
                            selector.select();
                            int c;
                            while ((c = sc.write(bb)) < 1) ;
                            assertEquals(c, 1);
                            out.printf("wrote: 0x%x%n", bb.get(0));
                            k.interestOps(OP_READ);
                            selector.select();
                            bb.clear();
                            while ((c = sc.read(bb)) == 0) ;
                            assertEquals(c, -1);
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        sc2.register(selector, OP_READ);
                        selector.select();
                        int c;
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), (byte)0x9A);
                        sc2.shutdownOutput();
                    }
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SELNBAccep_NBConn_NBIO_RW_10a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                Selector aselector = openSelector();
                ssc.configureBlocking(false).register(aselector, OP_ACCEPT);
                assertEquals(aselector.select(), 1);

                try (SocketChannel sc2 = ssc.accept()) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t10a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10);
                            sc.register(selector, OP_READ);
                            selector.select();
                            int c;
                            while ((c = sc.read(bb)) == 0);
                            assertEquals(c, 1);
                            out.printf("read:  0x%x%n", bb.get(0));
                            assertEquals(bb.get(0), (byte)0xAA);
                            sc.shutdownOutput();
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0xAA).flip();
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        SelectionKey k = sc2.register(selector, OP_WRITE);
                        selector.select();
                        int c;
                        while ((c = sc2.write(bb)) < 1) ;
                        assertEquals(c, 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        k.interestOps(OP_READ);
                        selector.select();
                        bb.clear();
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, -1);
                    }
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SPINBAccep_NBConn_NBIO_WR_11a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                SocketChannel accepted;
                for (;;) {
                    accepted = ssc.accept();
                    if (accepted != null) {
                        out.println("accepted new connection");
                        break;
                    }
                    Thread.onSpinWait();
                }

                try (SocketChannel sc2 = accepted) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t11a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0xBA).flip();
                            sc.configureBlocking(false);
                            SelectionKey k = sc.register(selector, OP_WRITE);
                            selector.select();
                            int c;
                            while ((c = sc.write(bb)) < 1) ;
                            assertEquals(c, 1);
                            out.printf("wrote: 0x%x%n", bb.get(0));
                            k.interestOps(OP_READ);
                            selector.select();
                            bb.clear();
                            while ((c = sc.read(bb)) == 0) ;
                            assertEquals(c, -1);
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10);
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        sc2.register(selector, OP_READ);
                        selector.select();
                        int c;
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, 1);
                        out.printf("read:  0x%x%n", bb.get(0));
                        assertEquals(bb.get(0), (byte)0xBA);
                        sc2.shutdownOutput();
                    }
                    t.awaitCompletion();
                }
            }
        }
    }

    @Test
    public void SPINBAccep_NBConn_NBIO_RW_12a() throws Throwable {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(addr, 0));
            final int port = ssc.socket().getLocalPort();

            try (SocketChannel sc = openSocketChannel(family)) {
                sc.configureBlocking(false);
                sc.connect(new InetSocketAddress(addr, port));

                SocketChannel accepted;
                for (;;) {
                    accepted = ssc.accept();
                    if (accepted != null) {
                        out.println("accepted new connection");
                        break;
                    }
                    Thread.onSpinWait();
                }

                try (SocketChannel sc2 = accepted) {
                    assertTrue(sc.finishConnect());
                    TestThread t = TestThread.of("t10a", () -> {
                        try (Selector selector = openSelector()) {
                            ByteBuffer bb = ByteBuffer.allocate(10);
                            sc.register(selector, OP_READ);
                            selector.select();
                            int c;
                            while ((c = sc.read(bb)) == 0);
                            assertEquals(c, 1);
                            out.printf("read:  0x%x%n", bb.get(0));
                            assertEquals(bb.get(0), (byte)0xCA);
                            sc.shutdownOutput();
                        }
                    });
                    t.start();

                    ByteBuffer bb = ByteBuffer.allocate(10).put((byte)0xCA).flip();
                    sc2.configureBlocking(false);
                    try (Selector selector = openSelector()) {
                        SelectionKey k = sc2.register(selector, OP_WRITE);
                        selector.select();
                        int c;
                        while ((c = sc2.write(bb)) < 1) ;
                        assertEquals(c, 1);
                        out.printf("wrote: 0x%x%n", bb.get(0));
                        k.interestOps(OP_READ);
                        selector.select();
                        bb.clear();
                        while ((c = sc2.read(bb)) == 0) ;
                        assertEquals(c, -1);
                    }
                    t.awaitCompletion();
                }
            }
        }
    }
*/
    // --

    static class TestThread extends Thread {
        private final UncheckedRunnable runnable;
        private volatile Throwable throwable;

        TestThread(UncheckedRunnable runnable, String name) {
            super(name);
            this.runnable = runnable;
        }

        @Override
        public void run() {
            try {
                runnable.run();
            } catch (Throwable t) {
                out.printf("[%s] caught unexpected: %s%n", getName(), t);
                throwable = t;
            }
        }

        interface UncheckedRunnable {
            void run() throws Throwable;
        }

        static TestThread of(String name, UncheckedRunnable runnable) {
            return new TestThread(runnable, name);
        }

        void awaitCompletion() throws Throwable {
            this.join();
            if (throwable != null)
                throw throwable;
        }
    }
}
