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
 * @summary test for adjusting carrier number at runtime
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.growCarrierTickUs=200000  AdjustCarrierTest
 */


import com.alibaba.wisp.engine.WispEngine;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;

import static jdk.testlibrary.Asserts.assertTrue;

public class AdjustCarrierTest {
    public static void main(String[] args) throws Exception {
        Thread.currentThread().setName("Wisp-Sysmon");
        Class<?> claz = Class.forName("com.alibaba.wisp.engine.WispScheduler");
        Method method = claz.getDeclaredMethod("checkAndGrowWorkers", int.class);
        method.setAccessible(true);
        ExecutorService g = WispEngine.createEngine(4, Thread::new);

        CountDownLatch latch = new CountDownLatch(100);
        CountDownLatch grow = new CountDownLatch(1);

        for (int i = 0; i < 50; i++) {
            g.execute(() -> {
                try {
                    grow.await();
                    latch.countDown();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            });
        }

        Field scheduler = Class.forName("com.alibaba.wisp.engine.WispEngine").getDeclaredField("scheduler");
        scheduler.setAccessible(true);
        method.invoke(scheduler.get(g), 100);
        grow.countDown();

        for (int i = 0; i < 50; i++) {
            g.execute(latch::countDown);
        }

        latch.await();
        Field f = claz.getDeclaredField("workers");
        f.setAccessible(true);
        System.out.println(f.get(scheduler.get(g)).getClass().toString());
        assertTrue(((Object[]) (f.get(scheduler.get(g)))).length == 100);
        Field name = Class.forName("com.alibaba.wisp.engine.WispSysmon").getDeclaredField("WISP_SYSMON_NAME");
        name.setAccessible(true);
        assertTrue(name.get(null).equals("Wisp-Sysmon"));
    }
}
