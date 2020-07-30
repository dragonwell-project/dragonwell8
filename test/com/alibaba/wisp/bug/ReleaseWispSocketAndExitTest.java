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
 * @summary handle this scenario:
 *      1. task A fetch a socket S and release it.
 *      2. task B get the socket S and block on IO.
 *      3. task A exit and clean S's event, now B waiting forever...
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true ReleaseWispSocketAndExitTest
 */


import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

public class ReleaseWispSocketAndExitTest {
    static byte buf[] = new byte[1000];

    public static void main(String[] args) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        CountDownLatch listen = new CountDownLatch(1);
        CountDownLatch finish = new CountDownLatch(3);

        WispEngine.dispatch(() -> {
            try {
                ServerSocket sso = new ServerSocket(51243);
                listen.countDown();
                Socket so;
                so = sso.accept();
                InputStream is = so.getInputStream();
                OutputStream os = so.getOutputStream();
                int n;
                while ((n = is.read(buf, 0, 4)) == 4) {
                    long l = Long.valueOf(new String(buf, 0, n));
                    System.out.println("l = " + l);
                    SharedSecrets.getWispEngineAccess().sleep(l);
                    os.write(buf, 0, n);
                }
            } catch (Exception e) {
                throw new Error();
            }
            finish.countDown();
        });

        listen.await();
        Socket so = new Socket("127.0.0.1", 51243);

        WispEngine.dispatch(() -> {
            try {
                InputStream is = so.getInputStream();
                OutputStream os = so.getOutputStream();
                os.write("0000".getBytes());
                System.out.println("======");
                latch.await();
                System.out.println("======2");
            } catch (Exception e) {
                throw new Error();
            }
            finish.countDown();
        });

        WispEngine.dispatch(() -> {
            try {
                InputStream is = so.getInputStream();
                OutputStream os = so.getOutputStream();

                os.write("0100".getBytes());
                System.out.println("------");
                latch.countDown();
                is.read(buf);
                is.read(buf);// blocked
                System.out.println("------2");

                so.close();
            } catch (Exception e) {
                throw new Error();
            }
            finish.countDown();
        });

        finish.await(5, TimeUnit.SECONDS);
        System.out.println("passed");
    }

}
