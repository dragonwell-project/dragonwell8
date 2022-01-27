/*
 * Copyright (c) 2022 Alibaba Group Holding Limited. All Rights Reserved.
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
 * @summary Test object lock with coroutine
 * @requires os.family == "linux"
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.schedule.policy=PULL PassTokenTest
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.schedule.policy=PUSH PassTokenTest
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -DcheckStealEnable=true PassTokenTest
 */


import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import static jdk.testlibrary.Asserts.assertTrue;
import static jdk.testlibrary.Asserts.fail;

public class PassTokenTest {
    private final static boolean needCheckStealEnable = Boolean.getBoolean("checkStealEnable");
    final static Runner[] runners = new Runner[8];
    static boolean JUC;
    static final int N = 40000;

    public static void main(String[] args) throws Exception {
        JUC = true;
        System.out.println("JUC");
        doTest();
        JUC = false;
        System.out.println("ObjectMonitor");
        doTest();
    }

    private static void doTest() {
        for (int i = 0; i < runners.length; i++) {
            runners[i] = new Runner(i);
        }
        List<Runnable> plans = new ArrayList<>(Arrays.asList(
                () -> { // only coroutine
                    for (Runner runner1 : runners) {
                        WispEngine.dispatch(runner1);
                    }
                },
                () -> { // only thread
                    int n = 0;
                    for (Runner runner : runners) {
                        new Thread(runner, "MP-THREAD-RUNNER-" + n++).start();
                    }
                },
                () -> { //mixed
                    int n = 0;
                    for (int i = 0; i < runners.length; i += 2) {
                        final int ci = i;
                        new Thread(() -> {
                            for (int j = ci; j < ci + 2 && j < runners.length; j++) {
                                WispEngine.dispatch(runners[j]);
                            }
                        }, "MP-MIX-RUNNER-" + n++).start();
                    }
                }));
        Collections.shuffle(plans);

        plans.forEach(plan -> {
            finishLatch = new CountDownLatch(runners.length);
            current = 0;
            long start = System.currentTimeMillis();
            plan.run(); // create runners
            try {
                finishLatch.await();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            System.out.println("elapsed = " + (System.currentTimeMillis() - start) + "ms");
        });

        System.out.println("-----------");
    }


    static int current = 0;
    static final Lock lock = new ReentrantLock();
    static final Condition cond = lock.newCondition();
    static CountDownLatch finishLatch;

    static class Runner implements Runnable {

        private final int ord;

        public Runner(int ord) {
            this.ord = ord;
        }

        @Override
        public void run() {
            WispTask task = SharedSecrets.getWispEngineAccess().getCurrentTask();

            while (current < N) {
                if (JUC) {
                    lock.lock();
                    try {
                        while (current % runners.length != ord) {
                            try {
                                cond.await();
                                checkStealEnable(task);
                            } catch (InterruptedException e) {
                                fail(e.getMessage());
                            }
                        }
                        if (++current % 10000 == 0) {// pass the token
                            System.out.println(SharedSecrets.getJavaLangAccess().currentThread0().getName() + "\t" + current);
                        }
                        cond.signalAll();
                    } finally {
                        lock.unlock();
                    }
                } else {
                    synchronized (lock) {
                        while (current % runners.length != ord) {
                            try {
                                lock.wait();
                                checkStealEnable(task);
                            } catch (InterruptedException e) {
                                fail(e.getMessage());
                            }
                        }
                        if (++current % 10000 == 0) {// pass the token
                            System.out.println(SharedSecrets.getJavaLangAccess().currentThread0().getName() + "\t" + current);
                        }
                        lock.notifyAll();
                    }
                }
            }

            finishLatch.countDown();
        }

        static void checkStealEnable(WispTask task) {
            if (!needCheckStealEnable) {
                return;
            }
            try {
                Field resumeEntryField = task.getClass().getDeclaredField("resumeEntry");
                resumeEntryField.setAccessible(true);
                final Object resumeEntry = resumeEntryField.get(task);
                if (resumeEntry == null) {
                    return;
                }
                Field stealEnableField = resumeEntry.getClass().getDeclaredField("stealEnable");
                stealEnableField.setAccessible(true);
                assertTrue(stealEnableField.getBoolean(resumeEntry));
            } catch (ReflectiveOperationException e) {
                throw new Error(e);
            }
        }
    }
}
