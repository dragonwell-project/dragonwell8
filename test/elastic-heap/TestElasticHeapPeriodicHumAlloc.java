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
 * @summary test elastic-heap periodic adjust with humongous allocation
 * @library /testlibrary
 * @build TestElasticHeapPeriodicHumAlloc
 * @run main/othervm/timeout=300 TestElasticHeapPeriodicHumAlloc
 */

public class TestElasticHeapPeriodicHumAlloc {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        serverBuilder = ProcessTools.createJavaProcessBuilder(
            "-XX:ElasticHeapPeriodicInitialMarkIntervalMillis=60000",
            "-XX:ElasticHeapPeriodicMinYoungCommitPercent=10",
            "-XX:+ElasticHeapPeriodicUncommit",
            "-XX:ElasticHeapPeriodicUncommitStartupDelay=1",
            "-XX:ElasticHeapPeriodicYGCIntervalMillis=5000",
            "-XX:ElasticHeapYGCIntervalMinMillis=2000",
            "-Xmx1g", "-Xms1g", "-XX:MaxNewSize=350m", "-XX:+PrintGC",
            "-XX:+PrintGCDateStamps", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-XX:+UseG1GC", "-XX:+G1ElasticHeap",
            Server.class.getName());
        Process server = serverBuilder.start();

        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("G1 Humongous Allocation");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            for (int i = 0; i < 2; i++) {
                testloop();
            }
        }

        public static void testloop() throws Exception {
            Object[] root = new Object[40 * 1024];
            for (int i = 0; i < 40 * 1024; i++) {
                root[i] = new byte[10*1024];
                if (i % 10 == 0) {
                    Thread.sleep(1);
                }
            }

            byte[] array;
            for (int i = 0; i < 1000 * 20; i++) {
                array = new byte[2*1024];
                if ((i % 10)== 0) {
                    array = new byte[600*1024];
                }
                Thread.sleep(1);
            }
            for (int i = 0; i < 1000 * 20; i++) {
                array = new byte[100*1024];
                array = new byte[100*1024];
                array = new byte[100*1024];
                if ((i % 10)== 0) {
                    array = new byte[600*1024];
                }
                Thread.sleep(1);
            }
            root = null;
            for (int i = 0; i < 1000 * 20; i++) {
                array = new byte[2*1024];
                if ((i % 10)== 0) {
                    array = new byte[600*1024];
                }
                Thread.sleep(1);
            }
            for (int i = 0; i < 1000 * 20; i++) {
                array = new byte[100*1024];
                array = new byte[100*1024];
                array = new byte[100*1024];
                if ((i % 10)== 0) {
                    array = new byte[600*1024];
                }
                Thread.sleep(1);
            }
        }
    }
}
