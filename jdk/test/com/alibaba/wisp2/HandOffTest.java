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
 * @summary test long running or blocking syscall task could be retaken
 * @requires os.family == "linux"
 * @run main/othervm -Dcom.alibaba.wisp.carrierEngines=1 -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true  -Dcom.alibaba.wisp.version=2 -Dcom.alibaba.wisp.enableHandOff=true -Dcom.alibaba.wisp.handoffPolicy=ADAPTIVE -Dcom.alibaba.wisp.sysmonTickUs=100000 HandOffTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.io.IOException;
import java.lang.reflect.Field;
import java.net.InetSocketAddress;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SocketChannel;
import java.nio.ByteBuffer;
import java.util.Properties;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import static jdk.testlibrary.Asserts.assertTrue;

public class HandOffTest {
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    static Properties p;
    static String socketAddr;
    static {
        p = java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Properties>() {
                    public Properties run() {
                        return System.getProperties();
                    }
                }
        );
        socketAddr = (String)p.get("test.wisp.socketAddress");
        if (socketAddr == null) {
            socketAddr = "www.example.com";
        }
    }

    public static void main(String[] args) throws Exception {
        CountDownLatch cl = new CountDownLatch(10);
        for (int i = 0; i < 10; i++) {
            WispEngine.dispatch(() -> {
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                cl.countDown();
            });
        }

        AtomicBoolean blockingFinish = new AtomicBoolean(false);
        SocketChannel ch[] = new SocketChannel[1];

        WispEngine.dispatch(() -> {
            try {
                ch[0] = SocketChannel.open(new InetSocketAddress(socketAddr, 80));
                ch[0].read(ByteBuffer.allocate(4096));
                blockingFinish.set(true);
            } catch (IOException e) {
                if (! (e instanceof ClosedChannelException)) {
                    e.printStackTrace();
                }
            }
        });

        AtomicInteger integer = new AtomicInteger(-2);
        // test we won't lose wisptasks;
        CountDownLatch latch = new CountDownLatch(1);
        WispEngine.dispatch(() ->{
            try {
                latch.await();
            } catch (Exception e) {
                e.printStackTrace();
            }
            integer.incrementAndGet();
        });


        WispEngine.dispatch(() -> {
            try {
                Thread.sleep(999999999);
            } catch (Exception e) {
                e.printStackTrace();
            }
            integer.set(-99);
        });


        WispEngine.dispatch(() -> {
            try {
                Thread.sleep(5000);
            } catch (Exception e) {
                e.printStackTrace();
            }
            integer.incrementAndGet();
        });

        assertTrue(cl.await(5, TimeUnit.SECONDS));
        assertTrue(!blockingFinish.get());

        latch.countDown();
        Thread.sleep(100);
        while (integer.get() != 0) {
      //      System.out.println("spin....");
        }

        ch[0].close();
        // wait until hand off carrier die
        // TODO: enable handoff exit
//        Field field = WispEngine.class.getDeclaredField("carrierThreads");
//        field.setAccessible(true);
//        Set<Thread> set = (Set<Thread>)field.get(null);
//        Thread.sleep(200);
//        assertTrue(set.size() == 1);
    }
}
