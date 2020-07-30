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
 * @summary verify queue length not growth infinity
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 DisableStealBugTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;

import static jdk.testlibrary.Asserts.assertLT;

public class SchedulerQLBugTest {
    public static void main(String[] args) throws Exception {
        WispEngine g = WispEngine.createEngine(2, Thread::new);
        CountDownLatch latch = new CountDownLatch(1);
        g.execute(() -> {
            DisableStealBugTest.setOrGetStealEnable(SharedSecrets.getWispEngineAccess().getCurrentTask(), true, false);
            latch.countDown();
            while (true) {
                try {
                    Thread.sleep(1);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        });
        latch.await();
        for (int i = 0; i < 10; i++) { // trigger steal
            g.execute(() -> { /**/});
        }

        Thread.sleep(100);
        for (int i = 0; i < 10; i++) {
            int ql = g.submit(() -> {
                try {
                    Method getCurrent = WispCarrier.class.getDeclaredMethod("current");
                    getCurrent.setAccessible(true);

                    Method m = WispCarrier.class.getDeclaredMethod("getTaskQueueLength");
                    m.setAccessible(true);
                    return (int) m.invoke(getCurrent.invoke(null));
                } catch (ReflectiveOperationException e) {
                    throw new Error(e);
                }
            }).get();
            assertLT(ql, 100);
        }
    }
}
