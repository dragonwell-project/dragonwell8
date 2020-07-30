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
 * @summary PreserveFramePointer for coroutine stack backtrace test
 * @requires os.family == "linux"
 * @run main/othervm -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 -Xcomp -XX:TieredStopAtLevel=1 -XX:+PreserveFramePointer TestPreserveFramePointer
 * @run main/othervm -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 -Xcomp -XX:TieredStopAtLevel=1 -XX:-PreserveFramePointer TestPreserveFramePointer
 * @run main/othervm -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 -Xcomp -XX:-TieredCompilation -XX:+PreserveFramePointer TestPreserveFramePointer
 * @run main/othervm -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 -Xcomp -XX:-TieredCompilation -XX:-PreserveFramePointer TestPreserveFramePointer
 */


import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;

public class TestPreserveFramePointer {

    private static Object lock = new Object();

    private static final int COROUTINE_NUM = 100;

    private static CountDownLatch cdl = new CountDownLatch(1);
    private static CountDownLatch mainLock = new CountDownLatch(COROUTINE_NUM);

    public static void main(String[] args) throws InterruptedException {

        new Thread(() -> {                    // the lock holder thread to make lock contend
            synchronized (lock) {
                cdl.countDown();              // when we hold the lock, others can go on
                try {
                    Thread.sleep(100);  // hold the lock for 100ms so others contends
                    System.gc();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            while (true) {}
        }).start();

        new Thread(() -> {
            for (int i = 0; i < 100; i++) {
                WispEngine.dispatch(() -> {
                    try {
                        cdl.await();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    mainLock.countDown();
                });
            }
        }).start();

        mainLock.await();
    }

}
