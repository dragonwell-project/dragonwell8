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
 * @summary test elastic-heap
 * @library /testlibrary
 * @build TestElasticHeapMisc
 * @run main/othervm/timeout=100 TestElasticHeapMisc
 */

public class TestElasticHeapMisc {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m", "-XX:SurvivorRatio=1",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-XX:InitiatingHeapOccupancyPercent=80",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                GenerationLimitLargerSurvivor.class.getName());
        Process server = serverBuilder.start();

        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        // Will uncommit 70M in first time
        output.shouldContain("[Elastic Heap concurrent thread: uncommit 71680K");
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-XX:ElasticHeapPeriodicYGCIntervalMillis=200",
                "-XX:ElasticHeapPeriodicInitialMarkIntervalMillis=5000",
                "-XX:InitiatingHeapOccupancyPercent=80",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                NoElasticHeapGC.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldNotContain("Elastic Heap triggered GC");
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m", "-XX:SurvivorRatio=1",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-XX:InitiatingHeapOccupancyPercent=80",
                "-verbose:gc", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                GenerationLimitLargerSurvivor.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        // Both -XX:+PrintGCDetails and -XX:G1ElasticHeap are necessary
        // for printing elastic heap related logs.
        output.shouldNotContain("Elastic Heap concurrent thread");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class NoElasticHeapGC {
        public static void main(String[] args) throws Exception {
            byte[] arr;
            for (int i = 0; i < 1000 * 15; i++) {
                arr = new byte[20*1024];
                Thread.sleep(1);
            }
        }
    }

    private static class GenerationLimitLargerSurvivor {
        public static void main(String[] args) throws Exception {
            Object[] root = new Object[1024*1024];
            int rootIndex = 0;
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms , 200M per second
            // so 2 GCs per second
            // Make 10% survive
            for (int i = 0; i < 1000 * 5; i++) {
                if (i % 10 != 0) {
                    arr = new byte[200*1024];
                }
                else {
                    root[rootIndex++] = new byte[200*1024];
                }
                Thread.sleep(1);
            }
            triggerJcmd("GC.elastic_heap", "young_commit_percent=30");
            for (int i = 0; i < 1000 * 7; i++) {
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
