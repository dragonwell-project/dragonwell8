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
 * @summary test elastic-heap with young-gen old-gen overlap
 * @library /testlibrary
 * @build TestElasticHeapYoungOldGenOverlap
 * @run main/othervm/timeout=200 TestElasticHeapYoungOldGenOverlap
 */

public class TestElasticHeapYoungOldGenOverlap {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        Process server;
        OutputAnalyzer output;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1000m", "-Xms1000m",
                "-XX:+ElasticHeapPeriodicUncommit",
                "-XX:ElasticHeapPeriodicYGCIntervalMillis=400",
                "-XX:ElasticHeapPeriodicUncommitStartupDelay=0",
                "-XX:MaxNewSize=400m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
                "-XX:InitiatingHeapOccupancyPercent=90",
                "-XX:ElasticHeapYGCIntervalMinMillis=100",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("Elastic Heap concurrent thread: commit");
        output.shouldNotContain("Full GC");
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1000m", "-Xms1000m",
                "-XX:ElasticHeapPeriodicYGCIntervalMillis=400",
                "-XX:ElasticHeapPeriodicUncommitStartupDelay=0",
                "-XX:MaxNewSize=400m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
                "-XX:InitiatingHeapOccupancyPercent=90",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server2.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1000m", "-Xms1000m",
                "-XX:ElasticHeapPeriodicYGCIntervalMillis=400",
                "-XX:ElasticHeapPeriodicUncommitStartupDelay=0",
                "-XX:MaxNewSize=400m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
                "-XX:InitiatingHeapOccupancyPercent=50",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server3.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            OutputAnalyzer output;
            byte[] arr = new byte[200*1024];
            // Allocate 400k per 1ms , 400M per second
            // so 1 GC per second
            for (int i = 0; i < 1000 * 15; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: young generation commit percent 50, uncommitted memory 209715200 B]");
            output.shouldHaveExitValue(0);

            // Promote 800m to old gen
            Object[] root = new Object[8000];
            for (int i = 0; i < 8000; i += 4) {
                root[i] = new byte[100*1024];
                root[i+1] = new byte[100*1024];
                root[i+2] = new byte[100*1024];
                root[i+3] = new byte[100*1024];
                Thread.sleep(1);
            }
            for (int i = 0; i < 1000 * 1; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }

            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: young generation commit percent 100, uncommitted memory 0 B]");
            output.shouldHaveExitValue(0);

        }

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

        private static OutputAnalyzer triggerJinfo(String arg) throws Exception {
            String pid = Integer.toString(ProcessTools.getProcessId());
            JDKToolLauncher jcmd = JDKToolLauncher.create("jinfo")
                                                  .addToolArg("-flag")
                                                  .addToolArg(arg)
                                                  .addToolArg(pid);
            ProcessBuilder pb = new ProcessBuilder(jcmd.getCommand());
            return new OutputAnalyzer(pb.start());
        }
        private static int getRss() throws Exception {
            String pid = Integer.toString(ProcessTools.getProcessId());
            int rss = 0;
            Process ps = Runtime.getRuntime().exec("cat /proc/"+pid+"/status");
            ps.waitFor();
            BufferedReader br = new BufferedReader(new InputStreamReader(ps.getInputStream()));
            String line;
            while (( line = br.readLine()) != null ) {
                if (line.startsWith("VmRSS:") ) {
                    int numEnd = line.length() - 3;
                    int numBegin = line.lastIndexOf(" ", numEnd - 1) + 1;
                    rss = Integer.parseInt(line.substring(numBegin, numEnd));
                    break;
                }
            }
            return rss;
        }
    }

    private static class Server2 extends Server {
        public static void main(String[] args) throws Exception {
            OutputAnalyzer output;
            byte[] arr = new byte[200*1024];
            // Allocate 400k per 1ms , 400M per second
            // so 1 GC per second
            for (int i = 0; i < 1000 * 15; i++) {
                arr = new byte[200*1024];
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            output = Server.triggerJcmd("GC.elastic_heap", "young_commit_percent=20");
            System.out.println(output.getOutput());
            output.shouldContain("Error: command fails");
            output.shouldHaveExitValue(0);
        }
    }
    private static class Server3 extends Server {
        public static void main(String[] args) throws Exception {
            OutputAnalyzer output;
            byte[] arr = new byte[200*1024];
            // Allocate 100k per 1ms , 100M per second
            for (int i = 0; i < 1000 * 15; i++) {
                arr = new byte[100*1024];
                Thread.sleep(1);
            }
            output = Server.triggerJcmd("GC.elastic_heap", "young_commit_percent=20");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: young generation commit percent 20");
            output.shouldHaveExitValue(0);
        }
    }

}
