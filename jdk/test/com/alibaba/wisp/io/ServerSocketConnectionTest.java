/*
 * Copyright (c) 2022 Alibaba Group Holding Limited. All Rights Reserved.
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
 * @summary Test ServerSocket isConnected() method
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true ServerSocketConnectionTest
 */

import java.io.IOException;
import java.net.*;
import static jdk.testlibrary.Asserts.assertTrue;
import static jdk.testlibrary.Asserts.assertFalse;


public class ServerSocketConnectionTest {

    private static void testIsConnectedAfterClosing() throws Exception {
        ServerSocket ss = null;
        Socket s1 = null;
        InetAddress ia = InetAddress.getLocalHost();
        try {
            ss = new ServerSocket(0);
            ss.setSoTimeout(1000);

            InetSocketAddress isa = new InetSocketAddress(ia, ss.getLocalPort());
            s1 = new Socket();
            s1.connect(isa);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed!");
        } finally {
            // close this socket
            if (s1 != null) {
                s1.close();
                // We should return true even after the socket is closed, to keep align with OpenJDK.
                assertTrue(s1.isConnected());
            }

            if (ss != null)  ss.close();
        }
    }

    private static void testIsConnectedAfterConnectionFailure() throws Exception {
        ServerSocket ss = null;
        Socket s1 = null;
        InetAddress ia = InetAddress.getLocalHost();
        try {
            ss = new ServerSocket(0);
            ss.setSoTimeout(1000);

            InetSocketAddress isa = new InetSocketAddress(ia, ss.getLocalPort());
            s1 = new Socket();
            s1.connect(isa, -1);  // exception here
        } catch (IOException e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed!");
        } catch (IllegalArgumentException e) {
            // we shall go here.
            assertFalse(s1.isConnected());
        } finally {
            if (ss != null)  ss.close();
            if (s1 != null)  s1.close();
        }
    }

    public static void main(String args[]) throws Exception {
        testIsConnectedAfterClosing();
        testIsConnectedAfterConnectionFailure();
    }
}
