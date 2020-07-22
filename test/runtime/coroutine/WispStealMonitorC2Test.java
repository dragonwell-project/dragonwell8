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
 * @summary c2 compiler monitorenter stub steal test
 * @run main/othervm/timeout=60 -XX:+UseWisp2 -Dcom.alibaba.wisp.schedule.stealRetry=100 -Dcom.alibaba.wisp.schedule.helpStealRetry=100 WispStealMonitorC2Test
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

// we need to limit steal workers count: for increasing the probability

public class WispStealMonitorC2Test {

    private static Object lock = new Object();

    private static JavaLangAccess s = SharedSecrets.getJavaLangAccess();

    private static AtomicBoolean succeeded = new AtomicBoolean(false);

    private static CountDownLatch cdl = new CountDownLatch(1);

    public static void main(String[] args) {

        new Thread(() -> {                    // the lock holder thread to make lock contend
            synchronized (lock) {
                cdl.countDown();              // when we hold the lock, others can go on
                try {
                    Thread.sleep(100);  // hold the lock for 100ms so others contends
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
                    Thread originThread = s.currentThread0();       // record the origin thread
                    while (true) {
                        synchronized (lock) {
                            Thread newThread = s.currentThread0();  // if current thread diff from origin one, then we can say it is stolen in _monitorenter
                            if (originThread != newThread) {
                                succeeded.lazySet(true);
                            }
                        }
                    }
                });
            }
        }).start();

        while (!succeeded.get()) {}

    }

}
