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
 * @summary Test invoke dynamic class for lambda with interrupt
 * @library /testlibrary
 * @run main/othervm -XX:+EnableCoroutine  -Dcom.alibaba.wisp.transparentWispSwitch=true -XX:+UseWispMonitor -Dcom.alibaba.wisp.enableHandOff=false -XX:+AllowParallelDefineClass TestInvokeDynamicInterruption
*/
import com.alibaba.wisp.engine.WispEngine;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

import static com.oracle.java.testlibrary.Asserts.*;

public class TestInvokeDynamicInterruption {
    private final static AtomicInteger idGenerator = new AtomicInteger();
    static WispEngine executor;
    static WispEngine executor2;
    static Thread[] threads;
    static CountDownLatch[] finishLatchs;
    static CountDownLatch finishLatch;
    static boolean is_interrupted = false;

    public static void main(String[] args) throws Exception {
        testInvokeDynamic();
    }

    private static void testInvokeDynamic() throws Exception {
        threads = new Thread[2];
        executor = WispEngine.createEngine(4, new ThreadFactory() {
            AtomicInteger seq = new AtomicInteger();

            @Override
            public Thread newThread(Runnable r) {
                int index = seq.getAndIncrement();
                Thread t = new Thread(r, "Wisp2-Group-Test-Carrier-" + index);
                t.setDaemon(true);
                return t;
            }
        });

        executor2 = WispEngine.createEngine(4, new ThreadFactory() {
            AtomicInteger seq = new AtomicInteger();

            @Override
            public Thread newThread(Runnable r) {
            Thread t = new Thread(r, "Wisp2-Group-Test2-Carrier-" + seq.getAndIncrement());
                t.setDaemon(true);
                return t;
            }
        });

        finishLatchs = new CountDownLatch[2];
        for (int j = 0; j < 2; ++j) {
            finishLatchs[j] = new CountDownLatch(1);
        }
        finishLatch = new CountDownLatch(2);

        Thread.sleep(100);
        for (int j = 0; j < 2; ++j) {
            executor.execute(() -> {
                try {
                    int id = idGenerator.getAndIncrement();
                    System.out.println("id = " + id);
                    threads[id] = Thread.currentThread();
                    finishLatchs[id].countDown();
                    executor.execute(() -> {
                        try {
                            List<Integer> list = Arrays.asList(1,2,3,4,5,6,7);
                            int sum = list.stream().map(x -> x*x).reduce((x,y) -> x + y).get();
                            System.out.println("after invoke " + sum);
                        } catch (Exception e) {
                            e.printStackTrace();
                            is_interrupted = true;
                        } finally {
                            finishLatch.countDown();
                        }
                    });
                } catch (Exception e) {
                    e.printStackTrace();
                    System.exit(-1);
                }
            });
        }
        for (int j = 0; j < 2; ++j) {
            final int index = 1 - j;
            executor2.execute(() -> {
                try {
                    finishLatchs[index].await();
                    for (int i = 0; i < 10; ++i) {
                       decIt(i);
                    }

                    threads[index].interrupt();
                    System.out.println("interrupt thread " + index);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            });
        }

        finishLatch.await();
        assertFalse(is_interrupted);
    }

    private static void decIt(long num) {
        while (0 != num--);
    }
}
