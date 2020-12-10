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
 * @summary Test making lots of Selectors
 * @requires (os.family == "linux")
 * @library .. /test/lib
 * @build jdk.test.lib.Utils TestServers
 * @build RsocketTest
 * @run main/othervm Connect
 */

import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.Iterator;
import java.util.Set;
import jdk.net.RdmaSockets;
import jtreg.SkippedException;
import static java.net.StandardProtocolFamily.INET;

public class Connect {

    static int success = 0;
    static final int LIMIT = 30;

    public static void main(String[] args) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        try (TestServers.DayTimeServer daytimeServer
                = TestServers.DayTimeServer.startNewServer(50)) {
            scaleTest(daytimeServer);
        }
    }

    static void scaleTest(TestServers.DayTimeServer daytimeServer)
        throws Exception
    {
        InetAddress myAddress = daytimeServer.getAddress();
        InetSocketAddress isa = new InetSocketAddress(myAddress, daytimeServer.getPort());

        for (int j=0; j<LIMIT; j++) {
            SocketChannel sc = RdmaSockets.openSocketChannel(INET);
            sc.configureBlocking(false);
            boolean connected = sc.connect(isa);
            System.out.println("connected = " + connected);

            if (!connected) {
                Selector selector = RdmaSockets.openSelector();
                sc.register(selector, SelectionKey.OP_CONNECT);
                while (!connected) {
                    int keysAdded = selector.select(100);
                    if (keysAdded > 0) {
                        Set<SelectionKey> readyKeys = selector.selectedKeys();
                        Iterator<SelectionKey> i = readyKeys.iterator();
                        while (i.hasNext()) {
                            SelectionKey sk = i.next();
                            SocketChannel nextReady = (SocketChannel)sk.channel();
                            connected = nextReady.finishConnect();
                        }
                        readyKeys.clear();
                    }
                }
                selector.close();
            }
            readAndClose(sc);
        }
    }

    static void readAndClose(SocketChannel sc) throws Exception {
        ByteBuffer bb = ByteBuffer.allocateDirect(100);
        int n = 0;
        while (n == 0) // Note this is not a rigorous check for done reading
            n = sc.read(bb);
        sc.close();
        success++;
        System.out.println("success count = " + success);
    }
}
