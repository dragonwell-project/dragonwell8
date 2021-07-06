/*
 * Copyright (c) 2021, Alibaba Group Holding Limited. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * @test
 * @library /lib/testlibrary
 * @summary Test for LazySetUnparkTest
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 LazySetUnparkTest
 */

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;


// this test is designed to be a long running SVT to verify the robustness of
// wisp schedule system, we expect a continuous output from submit thread.
// When there is few threads active and most threads are waiting for a signal
// with illegal JUC state, we would know something has gone wrong!!!
public class LazySetUnparkTest {
    private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();

    private static final boolean svtRun = Boolean.parseBoolean(System.getProperty("svt", "false"));

    public static void main(String[] args) throws InterruptedException {

        ScheduledThreadPoolExecutor executor = new ScheduledThreadPoolExecutor(16, new ThreadFactory() {
            @Override
            public Thread newThread(Runnable r) {
                Thread t = new Thread(r);
                t.setDaemon(true);
                return t;
            }
        });

        for (int i = 0; i < 16; i++) {
            int finalI = i;
            executor.scheduleAtFixedRate(new Runnable() {
                int count = 0;

                @Override
                public void run() {
                    try {
                        Thread.sleep(5);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    System.out.println("thread:" + Thread.currentThread().getName() + ", schedule " + finalI + ": " + count++);
                }
            }, 100, ThreadLocalRandom.current().nextInt(5) + 10, TimeUnit.MILLISECONDS);
        }

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    try {
                        final int[] count = {0};
                        while (true) {
                            count[0]++;
                            executor.submit(new Runnable() {
                                @Override
                                public void run() {
                                    System.out.println("submit: " + count[0]);
                                }
                            });

                            Thread.sleep(1);
                        }
                    } catch (Throwable e) {
                    }
                }
            }
        });
        if (!svtRun)
            thread.setDaemon(true);

        thread.start();

        while (true) {
            long stamp = mbean.getThreadCpuTime(thread.getId());
            thread.interrupt();
            Thread.sleep(10);
            assertTrue(stamp != mbean.getThreadCpuTime(thread.getId()));
            if (!svtRun) {
                break;
            }
        }
    }
}
