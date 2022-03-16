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

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.File;
import com.oracle.java.testlibrary.*;

/* @test
 * @summary test elastic-heap setting heap commit percent fail
 * @library /testlibrary /testlibrary/whitebox
 * @build TestElasticHeapSoftmxPercentFail
 * @run main/othervm/timeout=200 TestElasticHeapSoftmxPercentFail
 */

public class TestElasticHeapSoftmxPercentFail {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        Process server;
        OutputAnalyzer output;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx600m", "-Xms600m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-Xmn300m",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server2.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("to-space exhausted");
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx1000m", "-Xms1000m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-Xmn300m",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server3.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldNotContain("to-space exhausted");
        output.shouldNotContain("softmx percent setting failed");
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx1000m", "-Xms500m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-Xmn200m",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server4.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("same to Xmx");
        Asserts.assertTrue(output.getExitValue() != 0);

    }

    private static class Server {
        private static OutputAnalyzer triggerJcmd(String arg1, String arg2) throws Exception {
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
            return new OutputAnalyzer(pb.start());
        }
    }
    private static class Server2 extends Server {
        public static void main(String[] args) throws Exception {
            // Promote 400M into old gen
            Object[] root = new Object[4 * 1024];
            for (int i = 0; i < 4 * 1024; i++) {
                root[i] = new byte[99*1024];
            }

            byte[] arr = new byte[200*1024];
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
    }

    private static class Server3 extends Server {
        public static void main(String[] args) throws Exception {
            // Promote 400M into old gen
            Object[] root = new Object[4 * 1024];
            for (int i = 0; i < 4 * 1024; i++) {
                root[i] = new byte[99*1024];
            }

            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }

            OutputAnalyzer output = Server.triggerJcmd("GC.elastic_heap", "softmx_percent=60");
            System.out.println(output.getOutput());
            // Allocate 200k per 1ms, 200M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
    }

    private static class Server4 extends Server {
        public static void main(String[] args) throws Exception {
            // Promote 200M into old gen
            Object[] root = new Object[2 * 1024];
            for (int i = 0; i < 2 * 1024; i++) {
                root[i] = new byte[99*1024];
            }

            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }

            OutputAnalyzer output = Server.triggerJcmd("GC.elastic_heap", "softmx_percent=50");
            System.out.println(output.getOutput());
            // Allocate 200k per 1ms, 200M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
    }
}
