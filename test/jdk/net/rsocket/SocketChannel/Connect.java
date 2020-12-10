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
 * @summary Unit test for socket channels
 * @requires (os.family == "linux")
 * @library .. /test/lib
 * @build jdk.test.lib.Utils TestServers
 * @build RsocketTest
 * @run main/othervm Connect
 */

import java.net.ConnectException;
import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.nio.ByteBuffer;
import java.nio.channels.Selector;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.util.Set;
import jdk.net.RdmaSockets;

import jtreg.SkippedException;

public class Connect {

    private static final long INCREMENTAL_DELAY = 30L * 1000L;

    public static void main(String args[]) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        try (TestServers.EchoServer echoServer
                = TestServers.EchoServer.startNewServer(1000)) {
            test1(echoServer);
        }
        try {
            test1(TestServers.RefusingServer.newRefusingServer());
            throw new Exception("Refused connection throws no exception");
        } catch (ConnectException ce) {
            // Correct result
        }
    }

    static void test1(TestServers.AbstractServer server) throws Exception {
        Selector selector;
        SocketChannel sc;
        SelectionKey sk;
        InetSocketAddress isa = new InetSocketAddress(
            server.getAddress(), server.getPort());
        sc = RdmaSockets.openSocketChannel(StandardProtocolFamily.INET);
        sc.configureBlocking(false);

        selector = RdmaSockets.openSelector();
        sk = sc.register(selector, SelectionKey.OP_CONNECT);
        if (sc.connect(isa)) {
            System.err.println("Connected immediately!");
            sc.close();
            selector.close();
            return;
        } else {
            ByteBuffer buf = ByteBuffer.allocateDirect(100);
            buf.asCharBuffer().put(new String(
                "The quick brown fox jumped over the lazy dog."
                ).toCharArray());
            buf.flip();
            long startTime = System.currentTimeMillis();
            while(true) {
                selector.select(INCREMENTAL_DELAY);
                Set selectedKeys = selector.selectedKeys();

                if(selectedKeys.isEmpty()) {
                    System.err.println("Elapsed time without response: " +
                                       (System.currentTimeMillis() -
                                        startTime) / 1000L + " seconds.");
                }
                else if(!selectedKeys.contains(sk))
                {
                    System.err.println("Got wrong event about selection key.");
                } else {
                    System.err.println("Got event for our selection key.");
                    if(sk.isConnectable()) {
                        if(sc.finishConnect()) {
                            if(sc.isConnected()) {
                                System.err.println("Successful connect.");
                                sk.interestOps(SelectionKey.OP_WRITE);
                                sc.write(buf);
                            } else {
                                System.err.println(
                                      "Finish connect completed incorrectly.");
                            }
                        } else {
                            System.err.println(
                     "key incorrectly indicated socket channel connectable.");
                        }
                    }
                    if(sk.isWritable() && (buf.remaining() > 0)) {
                        sc.write(buf);
                    }
                    if(buf.remaining() == 0) {
                        System.err.println(
                            "SUCCESS! buffer contents were sent.");
                        sc.close();
                        selector.close();
                        return;
                    }
                }
                selectedKeys.clear();
            }
        }
    }
}
