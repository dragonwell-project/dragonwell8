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
 * @summary test support fd use acorss coroutines
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true ShareFdTest
 */

import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;


import static jdk.testlibrary.Asserts.assertTrue;

public class ShareFdTest {
    private static boolean success = true;
    private static ServerSocket serverSocket = null;

    public static void main(String[] args) {
        WispEngineAccess wispEngineAccess = SharedSecrets.getWispEngineAccess();
        assert (wispEngineAccess.enableSocketLock());
        ShareFdTest shareFdTest = new ShareFdTest();
        shareFdTest.testAccept();
        assert success;
    }

    void testAccept() {
        try {
            serverSocket = new ServerSocket();
            serverSocket.bind(new InetSocketAddress(6402));

            Thread t1 = new TestThread();
            Thread t2 = new TestThread();
            t1.start();
            t2.start();

            Socket s = new Socket();
            s.connect(new InetSocketAddress(6402));

            Socket s1 = new Socket();
            s1.connect(new InetSocketAddress(6402));
            t1.join();
            t2.join();

        } catch (Exception e) {
            e.printStackTrace();
            success = false;
        }
        assertTrue(success);
    }

    class TestThread extends Thread {
        @Override
        public void run() {
            blockOnAccept(serverSocket);
        }
    }

    static void blockOnAccept(ServerSocket serverSocket) {
        try {
            serverSocket.accept();
        } catch (Exception e) {
            e.printStackTrace();
            success = false;
        }
    }
}
