/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

import com.oracle.java.testlibrary.*;

/* @test
 * @summary test elastic-heap error with jcmd
 * @library /testlibrary
 * @build TestElasticHeapJcmdError
 * @run main/othervm/timeout=100 TestElasticHeapJcmdError
 */

public class TestElasticHeapJcmdError {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server.class.getName());
        Process server = serverBuilder.start();

        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("Error: -XX:+G1ElasticHeap is not enabled");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            triggerJcmd("GC.elastic_heap", "young_commit_percent=50");
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
        private static void triggerJcmd(String arg1, String arg2) throws Exception {
            String pid = Integer.toString(ProcessTools.getProcessId());
            JDKToolLauncher jcmd = JDKToolLauncher.create("jcmd")
                                                  .addToolArg(pid);
            if (arg1 != null) {
                jcmd.addToolArg(arg1);
            }
            if (arg2 != null) {
                jcmd.addToolArg(arg2);
            }
            ProcessBuilder pb = new ProcessBuilder(jcmd.getCommand());
            OutputAnalyzer output = new OutputAnalyzer(pb.start());
            System.out.println(output.getOutput());
            output.shouldHaveExitValue(0);
        }
    }
}
