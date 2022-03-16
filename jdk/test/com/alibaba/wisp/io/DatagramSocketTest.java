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
 * @summary Test WispCarrier's DatagramSocket, InitialDirContext use dup socket to query dns.
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true DatagramSocketTest
 */


import com.alibaba.wisp.engine.WispEngine;

import javax.naming.directory.InitialDirContext;
import java.io.IOException;
import java.net.*;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.*;

public class DatagramSocketTest {

    static CountDownLatch latch = new CountDownLatch(10);

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 10; i++) {
            WispEngine.current().execute(DatagramSocketTest::foo);
        }

        latch.await(5, TimeUnit.SECONDS);
        testSendAndReceive();
    }

    static public void foo() {
        try {
            InitialDirContext dirCtx = new InitialDirContext();
            System.out.println(dirCtx.getAttributes("dns:/www.tmall.com"));
        } catch (Throwable e) {
            throw new Error("query dns error");
        }
        latch.countDown();
    }

    static public void testSendAndReceive() throws Exception {
        CountDownLatch count = new CountDownLatch(1);
        InetAddress host = InetAddress.getByName("localhost");
        int port = 9527;
        DatagramSocket so = new DatagramSocket(port);
        so.setSoTimeout(1_000);
        final int loop = 1024;
        Thread receiver = new Thread(() -> {
            int received = 0;
            try {
                byte[] buf = new byte[1024 * 32];
                DatagramPacket dp = new DatagramPacket(buf, buf.length);
                count.countDown();
                for (int i = 0; i < loop; i++) {
                    received++;
                    so.receive(dp);
                }
            } catch (SocketTimeoutException e) {
                e.printStackTrace();
                System.out.println("received packets: " + received);
                // We at least received 64 packets before timeout
                assertTrue(received > 64);
            } catch (IOException e) {
                throw new Error(e);
            }
        });
        receiver.start();
        count.await();
        for (int i = 0; i < loop; i++) {
            byte[] buf = new byte[1024 * 32];
            DatagramPacket dp = new DatagramPacket(buf, buf.length, host, port);
            so.send(dp);
        }
        receiver.join();
    }
}
