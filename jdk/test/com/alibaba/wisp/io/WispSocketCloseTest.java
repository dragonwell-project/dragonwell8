/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @library /lib/testlibrary
 * @summary test close will wake up blocking wispTask
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true WispSocketCloseTest
 */

import java.io.IOException;
import java.net.*;

import static jdk.testlibrary.Asserts.assertTrue;


public class WispSocketCloseTest {

    class Reaper extends Thread {
        Socket s;
        int timeout;

        Reaper(Socket s, int timeout) {
            this.s = s;
            this.timeout = timeout;
        }

        public void run() {
            try {
                Thread.sleep(timeout);
                s.close();
            } catch (Exception e) {
            }
        }
    }

    WispSocketCloseTest() throws Exception {
        ServerSocket ss = new ServerSocket(0);
        ss.setSoTimeout(1000);

        InetAddress ia = InetAddress.getLocalHost();
        InetSocketAddress isa =
                new InetSocketAddress(ia, ss.getLocalPort());

        // client establishes the connection
        Socket s1 = new Socket();
        s1.connect(isa);

        // receive the connection
        Socket s2 = ss.accept();

        // schedule reaper to close the socket in 5 seconds
        Reaper r = new Reaper(s2, 5000);
        r.start();

        boolean readTimedOut = false;
        try {
            s2.getInputStream().read();
        } catch (IOException e) {
            assertTrue (e instanceof SocketException);
            assertTrue ("Socket is closed".equals(e.getMessage()));
        }

        s1.close();
        ss.close();

        if (readTimedOut) {
            throw new Exception("Unexpected SocketTimeoutException throw!");
        }
    }

    public static void main(String args[]) throws Exception {
        new WispSocketCloseTest();
    }
}
