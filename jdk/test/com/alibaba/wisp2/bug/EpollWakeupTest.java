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
 * @summary test selector.wakeup() dispatched
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -verbose:class  EpollWakeupTest 3000
 */

import java.nio.channels.Selector;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;

/**
 * Sleep random us before wakeup/select,
 * To help us coverage all branches, including pre-wakeup, normal-wakeup, concurrent-wakeup
 */
public class EpollWakeupTest {
    public static void main(String[] args) throws Exception {
        Selector selector = Selector.open();
        CyclicBarrier barrier = new CyclicBarrier(2);
        Random random = new Random();
        final Object lock = new Object();
        boolean[] wakened = new boolean[1];
        int[] cnt = new int[1];

        Thread io = new Thread(() -> {
            try {
                while (true) {
                    barrier.await();
                    new CountDownLatch(1).await(random.nextInt(10), TimeUnit.MICROSECONDS);
                    selector.select();
                    synchronized (lock) {
                        cnt[0]++;
                        wakened[0] = true;
                        lock.notifyAll();
                    }
                }
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }, "IO thread");

        Thread biz = new Thread(() -> {
            try {
                while (true) {
                    selector.selectNow(); // clean up
                    wakened[0] = false;
                    barrier.await();
                    new CountDownLatch(1).await(random.nextInt(10), TimeUnit.MICROSECONDS);
                    selector.wakeup();
                    synchronized (lock) {
                        while (!wakened[0]) {
                            lock.wait();
                        }
                    }
                }
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }, "biz thread");
        io.setDaemon(true);
        io.start();
        biz.setDaemon(true);
        biz.start();

        Executors.newScheduledThreadPool(1).scheduleAtFixedRate(() -> {
            System.out.println(cnt[0]);
            synchronized (lock) {
                cnt[0] = 0;
            }
        }, 1, 1, TimeUnit.SECONDS);

        Thread.sleep(Integer.valueOf(args[0]));
        long deadline = System.currentTimeMillis() + 1000;
        synchronized (lock) {
            while (!wakened[0]) {
                long timeout = deadline - System.currentTimeMillis();
                assertTrue(timeout > 0, "io thread hang...");
                lock.wait(timeout);
            }
        }
    }
}
