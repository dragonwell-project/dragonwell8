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
 * @summary Test for basic functionality for RdmaSocket and RdmaServerSocket
 * @requires (os.family == "linux")
 * @library .. /test/lib
 * @build RsocketTest
 * @run main/othervm BasicSocketTest
 */

import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.StandardProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import jdk.net.RdmaSockets;

import jtreg.SkippedException;

public class BasicSocketTest implements Runnable {
    static ServerSocket ss;
    static Socket s1, s2;
    static InetAddress iaddr;
    static int port = 0;
    static ServerSocketChannel ssc;
    static SocketChannel sc1, sc2;
    static String message = "This is a message!";
    static int length = -1;

    public static void main(String args[]) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        iaddr = InetAddress.getLocalHost();

        try {
            ss = RdmaSockets.openServerSocket(StandardProtocolFamily.INET);
            s1 = RdmaSockets.openSocket(StandardProtocolFamily.INET);
            ss.bind(new InetSocketAddress(iaddr, port));

            Thread t = new Thread(new BasicSocketTest());
            t.start();
            s2 = ss.accept();

            InputStream is = s2.getInputStream();
            length = message.length();

            int num = 0;
            byte[] buf = new byte[length];
            while (num < length) {
                int l = is.read(buf);
                num += l;
            }

            String result = new String(buf);
            if(!result.equals(message))
                throw new RuntimeException("Test Failed!");

        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed!");
        } finally {
            ss.close();
            s1.close();
            s2.close();
        }
    }

    public void run() {
        try {
            s1.connect(new InetSocketAddress(iaddr, ss.getLocalPort()));

            OutputStream os = s1.getOutputStream();
            os.write(message.getBytes());
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed!");
        }
    }
}
