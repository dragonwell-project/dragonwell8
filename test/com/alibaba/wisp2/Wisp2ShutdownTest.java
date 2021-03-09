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
 * @summary Wisp2ShutdownTest
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 Wisp2ShutdownTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;

import java.lang.reflect.Field;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static jdk.testlibrary.Asserts.assertEQ;
import static jdk.testlibrary.Asserts.assertTrue;

public class Wisp2ShutdownTest {
    public static void main(String[] args) throws Exception {
        int beg = getAllTasksCount();
        basicShutDownTest();
        multiGroupsShutDownTest();
        int end = getAllTasksCount();
        assertTrue(end - beg < 10);
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private static void basicShutDownTest() throws Exception {
        AtomicInteger n = new AtomicInteger();

        WispEngine g = WispEngine.createEngine(4, Thread::new);

        for (int i = 0; i < 888; i++) {
            g.execute(() -> {
                n.incrementAndGet();
                try {
                    while (true) {
                        sleep(1000000);
                    }
                } finally {
                    n.decrementAndGet();
                }
            });
        }

        while (n.get() != 888) {
            n.get();
        }

        long start = System.currentTimeMillis();

        g.shutdown();

        assertTrue(g.awaitTermination(3, TimeUnit.SECONDS));

        System.out.println(System.currentTimeMillis() - start + "ms");
        Thread.sleep(200);

        assertEQ(n.get(), 0);
    }

    private static void multiGroupsShutDownTest() throws Exception {
        CountDownLatch latch = new CountDownLatch(10);
        for (int i = 0; i < 10; i++) {
            WispEngine.dispatch(() -> {
                try {
                    basicShutDownTest();
                    latch.countDown();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            });
        }
        assertTrue(latch.await(5, TimeUnit.SECONDS));
    }

    private static int getAllTasksCount() {
        try {
            Field f = WispTask.class.getDeclaredField("id2Task");
            Map m = (Map) f.get(null);
            return m.size();
        } catch (Exception e) {
            return 0;
        }
    }
}
