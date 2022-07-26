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
 * @summary test blocking accept
 * @requires os.family == "linux"
 * @run main/othervm -Dcom.alibaba.wisp.carrierEngines=1 -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 BlockingAccept2Test
 */

import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.channels.ServerSocketChannel;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class BlockingAccept2Test {
    public static void main(String[] args) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        CountDownLatch latch2 = new CountDownLatch(1);
        Thread t = new Thread(() -> {
            try {
                latch.await(1, TimeUnit.SECONDS);
                Thread.sleep(200);
                Socket s = new Socket();
                s.connect(new InetSocketAddress(12389));
                latch2.await(1, TimeUnit.SECONDS);
                s.close();
                Thread.sleep(200);
                s = new Socket();
                s.connect(new InetSocketAddress(12389));
            } catch (Exception e) {
            }
        });
        t.start();
        ServerSocketChannel ssc = ServerSocketChannel.open();
        ssc.bind(new InetSocketAddress(12389));
        latch.countDown();
        ssc.accept();
        latch2.countDown();
        ssc.accept();
    }
}
