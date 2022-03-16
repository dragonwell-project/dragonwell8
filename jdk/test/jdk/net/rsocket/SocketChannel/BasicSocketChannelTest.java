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
 * @summary Test for basic functionality for RdmaSocketChannel
 *         and RdmaServerSocketChannel
 * @requires (os.family == "linux")
 * @library ../ /test/lib
 * @build RsocketTest
 * @run main/othervm BasicSocketChannelTest
 */

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UncheckedIOException;
import java.net.*;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.ByteBuffer;
import jdk.net.RdmaSockets;
import jtreg.SkippedException;
import static java.lang.String.format;
import static java.lang.System.out;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;
import static java.nio.charset.StandardCharsets.UTF_8;

public class BasicSocketChannelTest {

    static final String MESSAGE = "This is a MESSAGE!";
    static final int MESSAGE_LENGTH = MESSAGE.length();

    public static void main(String args[]) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        InetAddress iaddr = InetAddress.getLocalHost();
        ProtocolFamily family = iaddr instanceof Inet6Address ? INET6 : INET;
        out.printf("local address: %s%n", iaddr);

        testChannel(iaddr, family);
        testSocket(iaddr, family);
    }

    // Tests SocketChannel and ServerSocketChannel
    static void testChannel(InetAddress iaddr, ProtocolFamily family) throws Exception {
        out.printf("--- testChannel iaddr:%s, family:%s%n", iaddr, family);

        try (ServerSocketChannel ssc = RdmaSockets.openServerSocketChannel(family)) {
            ssc.bind(new InetSocketAddress(iaddr, 0));
            final int port = ssc.socket().getLocalPort();

            Thread t = new Thread("Channel") {
                public void run() {
                    try (SocketChannel sc1 = RdmaSockets.openSocketChannel(family)) {
                        sc1.connect(new InetSocketAddress(iaddr, port));
                        if (!sc1.isConnected())
                            throw new RuntimeException("Unconnected channel:" + sc1);
                        ByteBuffer input = ByteBuffer.allocate(MESSAGE_LENGTH);
                        input.put(MESSAGE.getBytes(UTF_8));
                        input.flip();
                        int inputNum = 0;
                        while (inputNum < MESSAGE_LENGTH) {
                            inputNum += sc1.write(input);
                        }
                        sc1.shutdownInput();
                        assertInputShutdown(sc1);
                        sc1.shutdownOutput();
                        assertOutputShutdown(sc1);
                    } catch (IOException e) {
                        throw new UncheckedIOException(e);
                    }
                }
            };
            t.start();

            try (SocketChannel sc2 = ssc.accept()) {
                ByteBuffer output = ByteBuffer.allocate(MESSAGE_LENGTH + 1);
                while (sc2.read(output) != -1);
                output.flip();

                String result = UTF_8.decode(output).toString();
                if (!result.equals(MESSAGE)) {
                    String msg = format("Expected [%s], received [%s]", MESSAGE, result);
                    throw new RuntimeException("Test Failed! " + msg);
                }
                sc2.shutdownInput();
                assertInputShutdown(sc2);
                sc2.shutdownOutput();
                assertOutputShutdown(sc2);
            }
            t.join();
            out.printf("passed%n");
        }
    }

    // Tests SocketChannel.socket() and ServerSocketChannel.socket()
    static void testSocket(InetAddress iaddr, ProtocolFamily family) throws Exception {
        out.printf("--- testSocket iaddr:%s, family:%s%n", iaddr, family);

        try (ServerSocketChannel ssc = RdmaSockets.openServerSocketChannel(family)) {
            ssc.socket().bind(new InetSocketAddress(iaddr, 0));
            final int port = ssc.socket().getLocalPort();

            Thread t = new Thread("Socket") {
                public void run() {
                    try (SocketChannel sc1 = RdmaSockets.openSocketChannel(family);
                         Socket client = sc1.socket()) {
                        client.connect(new InetSocketAddress(iaddr, port));
                        if (!client.isConnected())
                            throw new RuntimeException("Test Failed!");
                        InputStream is = client.getInputStream();
                        OutputStream os = client.getOutputStream();
                        os.write(MESSAGE.getBytes(UTF_8));

                        client.shutdownInput();
                        assertInputShutdown(sc1);
                        assertInputShutdown(client, is);
                        client.shutdownOutput();
                        assertOutputShutdown(sc1);
                        assertOutputShutdown(client, os);
                    } catch (IOException e) {
                        throw new UncheckedIOException(e);
                    }
                }
            };
            t.start();

            try (Socket conn = ssc.socket().accept()) {
                InputStream is = conn.getInputStream();
                OutputStream os = conn.getOutputStream();
                byte[] buf = is.readAllBytes();

                String result = new String(buf, UTF_8);
                if (!result.equals(MESSAGE)) {
                    String msg = format("Expected [%s], received [%s]", MESSAGE, result);
                    throw new RuntimeException("Test Failed! " + msg);
                }
                conn.shutdownInput();
                assertInputShutdown(conn, is);
                conn.shutdownOutput();
                assertOutputShutdown(conn, os);
            }
            t.join();
            out.printf("passed%n");
        }
    }

    static void assertInputShutdown(SocketChannel sc) throws IOException {
        ByteBuffer bb = ByteBuffer.allocate(1);
        int r = sc.read(bb);
        if (r != -1)
            throw new RuntimeException(format("Unexpected read of %d bytes", r));

        try {
            sc.socket().getInputStream();
        } catch (IOException expected) {
            String msg = expected.getMessage();
            if (!msg.contains("input"))
                throw new RuntimeException("Expected to find \"input\" in " + expected);
            if (!msg.contains("shutdown"))
                throw new RuntimeException("Expected to find \"shutdown\" in " + expected);
        }
    }

    static void assertOutputShutdown(SocketChannel sc) throws IOException {
        ByteBuffer bb = ByteBuffer.allocate(1);
        bb.put((byte)0x05);
        bb.flip();
        try {
            sc.write(bb);
            throw new RuntimeException("Unexpected write of bytes");
        } catch (ClosedChannelException expected) { }

        try {
            sc.socket().getOutputStream();
        } catch (IOException expected) {
            String msg = expected.getMessage();
            if (!msg.contains("output"))
                throw new RuntimeException("Expected to find \"output\" in " + expected);
            if (!msg.contains("shutdown"))
                throw new RuntimeException("Expected to find \"shutdown\" in " + expected);
        }
    }

    static void assertInputShutdown(Socket s, InputStream is) throws IOException {
        if (!s.isInputShutdown()) {
            throw new RuntimeException("Unexpected open input: " + s);
        }
        int r;
        if ((r = is.read()) != -1)
            throw new RuntimeException(format("Unexpected read of %d", r));

        try {
            s.getInputStream();
        } catch (IOException expected) {
            String msg = expected.getMessage();
            if (!msg.contains("input"))
                throw new RuntimeException("Expected to find \"input\" in " + expected);
            if (!msg.contains("shutdown"))
                throw new RuntimeException("Expected to find \"shutdown\" in " + expected);
        }
    }

    static void assertOutputShutdown(Socket s, OutputStream os) throws IOException {
        if (!s.isOutputShutdown()) {
            throw new RuntimeException("Unexpected open output: " + s);
        }
        try {
            os.write((byte)0x07);
            throw new RuntimeException("Unexpected write of bytes");
        } catch (ClosedChannelException expected) { }

        try {
            s.getOutputStream();
        } catch (IOException expected) {
            String msg = expected.getMessage();
            if (!msg.contains("output"))
                throw new RuntimeException("Expected to find \"output\" in " + expected);
            if (!msg.contains("shutdown"))
                throw new RuntimeException("Expected to find \"shutdown\" in " + expected);
        }
    }
}
