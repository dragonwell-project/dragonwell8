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
 * @summary test elastic-heap periodic adjust young gen
 * @library /testlibrary
 * @build TestElasticHeapPeriodicYoung
 * @run main/othervm/timeout=200 TestElasticHeapPeriodicYoung
 */

public class TestElasticHeapPeriodicYoung {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-XX:+ElasticHeapPeriodicUncommit", "-XX:ElasticHeapPeriodicYGCIntervalMillis=150",
                "-XX:ElasticHeapPeriodicUncommitStartupDelay=5",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m", "-XX:SurvivorRatio=1",
                "-XX:ElasticHeapYGCIntervalMinMillis=10",
                "-XX:ElasticHeapPeriodicMinYoungCommitPercent=40",
                "-XX:InitiatingHeapOccupancyPercent=80",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server.class.getName());
        Process server = serverBuilder.start();

        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        // Will not deallocate 60M in first time because of 50M survivor size
        String s = output.firstMatch("Elastic Heap concurrent thread: uncommit \\d+");
        Asserts.assertTrue(s != null);
        Asserts.assertFalse("Elastic Heap concurrent thread: uncommit 60".equals(s));
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-XX:+ElasticHeapPeriodicUncommit", "-XX:ElasticHeapPeriodicYGCIntervalMillis=500",
                "-XX:ElasticHeapPeriodicUncommitStartupDelay=1",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=10",
                "-XX:ElasticHeapPeriodicMinYoungCommitPercent=40",
                "-XX:InitiatingHeapOccupancyPercent=80",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server2.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("Elastic Heap concurrent thread: uncommit");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server2 {
        public static void main(String[] args) throws Exception {
            byte[] arr = new byte[200*1024];
            for (int i = 0; i < 1000 * 4; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            for (int i = 0; i < 1000 * 20; i++) {
                Thread.sleep(1);
            }
        }
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            Object[] root = new Object[1024*1024];
            int rootIndex = 0;
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms , 200M per second
            // so 2 GCs per second
            // Make 10% survive
            for (int i = 0; i < 1000 * 12; i++) {
                if (i % 10 != 0) {
                    arr = new byte[200*1024];
                }
                else {
                    root[rootIndex++] = new byte[200*1024];
                }
                Thread.sleep(1);
            }
        }
    }
}
