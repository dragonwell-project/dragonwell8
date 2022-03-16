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
 * @summary Test WispCounter removing during the shutdown of Wisp2Group
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine  -Dcom.alibaba.wisp.transparentWispSwitch=true  -Dcom.alibaba.wisp.version=2 -XX:+UseWispMonitor -Dcom.alibaba.wisp.enableHandOff=false  Wisp2GroupTest
 */

import com.alibaba.management.WispCounterMXBean;
import com.alibaba.wisp.engine.WispEngine;

import javax.management.MBeanServer;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.util.List;
import java.util.concurrent.AbstractExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static jdk.testlibrary.Asserts.assertTrue;

public class Wisp2GroupTest {
    static WispMultiThreadExecutor executor;
    static WispCounterMXBean mbean;

    public static void main(String[] args) throws Exception {

        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        try {
            mbean = ManagementFactory.newPlatformMXBeanProxy(mbs,
                    "com.alibaba.management:type=WispCounter", WispCounterMXBean.class);
        } catch (IOException e) {
            e.printStackTrace();
        }

        testWisp2Group();
    }

    private static void testWisp2Group() throws Exception {
        executor = new WispMultiThreadExecutor(4, new ThreadFactory() {
            AtomicInteger seq = new AtomicInteger();

            @Override
            public Thread newThread(Runnable r) {
                Thread t = new Thread(r, "Wisp2-Group-Test-Carrier-" + seq.getAndIncrement());
                t.setDaemon(true);
                return t;
            }
        });
        Thread.sleep(500);
        for (int j = 0; j < 10; ++j) {
            executor.execute(RUN_COMPILED_BUSY_LOOP);
        }
        Thread.sleep(500);
        List<Boolean> list = mbean.getRunningStates();
        System.out.println(list);
        int size1 = list.size();
        executor.shutdown();
        executor.awaitTermination(5, TimeUnit.SECONDS);
        list = mbean.getRunningStates();
        System.out.println(list);
        int size2 = list.size();
        assertTrue((size1 - size2) == 4);
    }

    // task running in compiled code
    private static void decIt(long num) {
        while (0 != num--) ;
    }

    private static final Runnable RUN_COMPILED_BUSY_LOOP = () -> {
        // warmup
        for (int i = 0; i < 5000; ++i) {
            decIt(i);
        }
        while (true) {
            decIt(0xFFFFFFFFl);
        }
    };

    static class WispMultiThreadExecutor extends AbstractExecutorService {
        private final WispEngine delegated;

        public WispMultiThreadExecutor(int threadCount, ThreadFactory threadFactory) {
            delegated = WispEngine.createEngine(threadCount, threadFactory);
        }

        @Override
        public void execute(Runnable command) {
            delegated.execute(command);
        }

        @Override
        public void shutdown() {
            delegated.shutdown();
        }

        @Override
        public List<Runnable> shutdownNow() {
            return null;
        }

        @Override
        public boolean isShutdown() {
            return false;
        }

        @Override
        public boolean isTerminated() {
            return false;
        }

        @Override
        public boolean awaitTermination(long timeout, TimeUnit unit) throws InterruptedException {
            return delegated.awaitTermination(timeout, unit);
        }
    }
}
