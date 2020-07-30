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
 * @summary test bug fix of thread object leak in thread group
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 Wisp2ThreadObjLeakInThreadGroupTest
 */

import java.util.concurrent.CountDownLatch;

import static jdk.testlibrary.Asserts.assertEQ;

public class Wisp2ThreadObjLeakInThreadGroupTest {
    public static void main(String[] args) throws Exception {
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        int count = tg.activeCount();
        CountDownLatch c1 = new CountDownLatch(1), c2 = new CountDownLatch(1);
        Thread thread = new Thread(tg, () -> {
            c1.countDown();
            try {
                c2.await();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }, Wisp2ThreadObjLeakInThreadGroupTest.class.getSimpleName());
        thread.start();
        c1.await(); // wait start
        assertEQ(tg.activeCount(), count + 1);
        c2.countDown(); // notify finish
        thread.join();
        assertEQ(tg.activeCount(), count);
    }
}
