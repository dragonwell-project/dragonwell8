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
 * @summary Verify wisp internal logic can not be preempted
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main PreemptWispInternalBugTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.FutureTask;
import static jdk.testlibrary.Asserts.assertTrue;



public class PreemptWispInternalBugTest {
    private static final int timeOut = 1000;
    private static final int iteration = 10;
    private static String[] tasks = new String[] {"toString", "addTimer"};

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            for (String task : tasks) {
                int i;
                for (i = 0; i < iteration; i++) {
                    ProcessBuilder pb = jdk.testlibrary.ProcessTools.createJavaProcessBuilder(
                            "-XX:+UnlockExperimentalVMOptions",
                            "-XX:+UseWisp2", "-XX:+UnlockDiagnosticVMOptions", "-XX:+VerboseWisp", "-XX:-Inline",
                            "-Xcomp", "-Dcom.alibaba.wisp.sysmonTickUs=100000",
                            "-cp", System.getProperty("java.class.path"),
                            PreemptWispInternalBugTest.class.getName(), task);
                    jdk.testlibrary.OutputAnalyzer output = new jdk.testlibrary.OutputAnalyzer(pb.start());
                    if (output.getStdout().contains("[WISP] preempt was blocked, because wisp internal method on the stack")) {
                        break;
                    }
                }
                assertTrue(i < iteration, "all iteration failed to preempt");
            }
            return;
        }
        FutureTask future = getTask(args[0]);
        WispEngine.dispatch(future);
        future.get();
    }

    private static FutureTask getFutureTask(Runnable runnable) {
        return new FutureTask<>(() -> {
            long start = System.currentTimeMillis();
            while (System.currentTimeMillis() - start < timeOut) {
                runnable.run();
            }
            return null;
        });
    }

    private static FutureTask getTask(String taskName) {
        switch (taskName) {
            case "toString":
                return getFutureTask(() -> SharedSecrets.getWispEngineAccess().getCurrentTask().toString());
            case "addTimer":
                return getFutureTask(() -> {
                    SharedSecrets.getWispEngineAccess().addTimer(System.nanoTime() + 1008611);
                    SharedSecrets.getWispEngineAccess().cancelTimer();
                });
            default:
                throw new IllegalArgumentException();
        }
    }
}

