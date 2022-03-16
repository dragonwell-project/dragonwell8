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
 * @summary verify there's no wisp class loaded after wisp initialize finished
 * @requires os.family == "linux"
 * @run main LoadClassInnerWispGuard
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.*;

import static jdk.testlibrary.Asserts.assertTrue;
import static jdk.testlibrary.Asserts.fail;

public class LoadClassInnerWispGuard {
    private static final String START = "LoadClassInnerWispGuard_START";
    private static final String END = "LoadClassInnerWispGuard_END";

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            driver();
        } else {
            doSomeWispOperation();
        }
    }

    /*
    Here is a sample output from OutputAnalyzer:

    [Loaded java.util.concurrent.ArrayBlockingQueue from: ...images/j2sdk-image/jre/lib/rt.jar]
    LoadClassInnerWispGuard_START
    [Loaded LoadClassInnerWispGuard$PingPong from file:...classes/com/alibaba/wisp/]
    LoadClassInnerWispGuard_END

    if class loading is happened during the test,
    [Loaded com.alibaba.wisp.engine.WispEngine*...] will exist between
    LoadClassInnerWispGuard_START and LoadClassInnerWispGuard_END
    and we'll fail
     */
    private static void driver() throws Exception {
        ProcessBuilder pb = jdk.testlibrary.ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockExperimentalVMOptions",
                "-XX:+UseWisp2", "-Dcom.alibaba.wisp.allThreadAsWisp=false", "-verbose:class",
                "-cp", System.getProperty("java.class.path"),
                LoadClassInnerWispGuard.class.getName(), "1");
        jdk.testlibrary.OutputAnalyzer output = new jdk.testlibrary.OutputAnalyzer(pb.start());
        String out = output.getStdout();
        int off = out.indexOf(START);
        for (String line : out.substring(off + START.length() + 1).split("\n")) {
            if (line.startsWith(END)) break;
            if (!line.startsWith("[Loaded")) continue;
            if (line.contains(LoadClassInnerWispGuard.class.getName())) continue;
            fail(line + "\n" + "wisp class loaded during runtime");
        }
    }

    private static class TF implements ThreadFactory {
        @Override
        public Thread newThread(Runnable r) {
            Thread thread = new Thread(r, "T");
            thread.setDaemon(true);
            return thread;
        }
    }

    private static void doSomeWispOperation() throws Exception {
        assertTrue(true); // preload jdk.testlibrary.Assert
        Runnable r = () -> { /**/};
        Executors.newCachedThreadPool(new TF()).submit(r).get();
        WispEngine g = WispEngine.current();
        BlockingQueue<Integer> q1 = new ArrayBlockingQueue<>(1);
        BlockingQueue<Integer> q2 = new ArrayBlockingQueue<>(1);
        CountDownLatch latch = new CountDownLatch(2);
        System.out.println(START);
        // 1. simple case
        g.submit(r).get();
        // 2. work stealing
        g.submit(new PingPong(q1, q2, latch));
        g.submit(new PingPong(q2, q1, latch));
        q1.offer(1); // start
        latch.await();
        System.out.println(END);
    }

    static class PingPong implements Runnable {
        final BlockingQueue<Integer> pingQ;
        final BlockingQueue<Integer> pongQ;
        final CountDownLatch latch;

        public PingPong(BlockingQueue<Integer> pingQ, BlockingQueue<Integer> pongQ, CountDownLatch latch) {
            this.pingQ = pingQ;
            this.pongQ = pongQ;
            this.latch = latch;
        }

        @Override
        public void run() {
            try {
                for (int i = 0; i < 100000; i++) {
                    pingQ.poll(1, TimeUnit.HOURS);
                    pongQ.offer(1);
                }
                latch.countDown();
            } catch (Exception e) {
            }
        }
    }
}
