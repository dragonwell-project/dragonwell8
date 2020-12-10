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
 * @summary Test the java.net.socket.GetLocalAddress method
 * @requires (os.family == "linux")
 * @library .. /test/lib
 * @build RsocketTest
 * @run main/othervm GetLocalAddress
 */

import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import jdk.net.RdmaSockets;

import jtreg.SkippedException;

public class GetLocalAddress implements Runnable {
    static Socket s;
    static ServerSocket ss;
    static InetAddress addr;
    static int port;

    public static void main(String args[]) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        testBindNull();

        boolean error = true;
        int value = 0;
        addr = InetAddress.getLocalHost();

        ss = RdmaSockets.openServerSocket(StandardProtocolFamily.INET);
        s = RdmaSockets.openSocket(StandardProtocolFamily.INET);
        addr = InetAddress.getLocalHost();
        ss.bind(new InetSocketAddress(addr, 0));

        port = ss.getLocalPort();

        Thread t = new Thread(new GetLocalAddress());
        t.start();

        Socket soc = ss.accept();

        if(addr.equals(soc.getLocalAddress())) {
            error = false;
        }
        if (error)
            throw new RuntimeException("Socket.GetLocalAddress failed.");
        soc.close();
    }

    public void run() {
        try {
            s.connect(new InetSocketAddress(addr, port));
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Socket.GetLocalAddress failed.");
        }
    }

    static void testBindNull() throws Exception {
        try {
            Socket soc = RdmaSockets.openSocket(StandardProtocolFamily.INET);
            soc.bind(null);
            if (!soc.isBound())
                throw new RuntimeException(
                    "should be bound after bind(null)");
            if (soc.getLocalPort() <= 0)
                throw new RuntimeException(
                   "bind(null) failed, local port: " + soc.getLocalPort());
            if (soc.getLocalAddress() == null)
                 throw new RuntimeException(
                   "bind(null) failed, local address is null");
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Socket.GetLocalAddress failed.");
        }
    }
}
