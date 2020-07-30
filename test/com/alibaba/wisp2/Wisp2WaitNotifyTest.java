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
 * @summary Test Object.wait/notify with coroutine in wisp2
 * @requires os.family == "linux"
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 Wisp2WaitNotifyTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

import static jdk.testlibrary.Asserts.assertEQ;

public class Wisp2WaitNotifyTest {

    private AtomicInteger seq = new AtomicInteger();
    private int finishCnt = 0;
    private CountDownLatch latch = new CountDownLatch(1);
    private boolean fooCond = false;

    public static void main(String[] args) throws Exception {
        Wisp2WaitNotifyTest s = new Wisp2WaitNotifyTest();
        synchronized (s) {
            WispEngine.dispatch(s::foo);
            assertEQ(s.seq.getAndIncrement(), 0);
        }

        s.latch.await();

        assertEQ(s.seq.getAndIncrement(), 5);
        synchronized (s) {
            while (s.finishCnt < 2) {
                s.wait();
            }
        }
        assertEQ(s.seq.getAndIncrement(), 6);
    }

    private synchronized void foo() {
        assertEQ(seq.getAndIncrement(), 1);

        WispEngine.dispatch(this::bar);
        assertEQ(seq.getAndIncrement(), 2);
        try {
            while (!fooCond) {
                wait();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        assertEQ(seq.getAndIncrement(), 4);

        latch.countDown();
        finishCnt++;
        notifyAll();
    }

    private void bar() {
        synchronized (this) {
            assertEQ(seq.getAndIncrement(), 3);
            fooCond = true;
            notifyAll();
        }
        synchronized (this) {
            finishCnt++;
            notifyAll();
        }
    }
}
