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
 * @summary test elastic-heap setting softmx percent with RSS check
 * @library /testlibrary /testlibrary/whitebox
 * @build TestElasticHeapSoftmxPercent
 * @run main/othervm/timeout=200 TestElasticHeapSoftmxPercent
 */

public class TestElasticHeapSoftmxPercent {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx1000m", "-Xms1000m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server.class.getName());
        Process server = serverBuilder.start();
        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        // Should not have regular initial-mark
        output.shouldNotContain("[GC pause (G1 Evacuation Pause) (young) (initial-mark)");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            // Promote 300M into old gen
            Object[] root = new Object[3 * 1024];
            for (int i = 0; i < 3 * 1024; i++) {
                root[i] = new byte[100*1024];
            }

            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }

            System.gc();
            int fullRss = getRss();
            System.out.println("Full rss: " + fullRss);
            Asserts.assertTrue(fullRss > 1000 * 1024);
            OutputAnalyzer output = triggerJcmd("GC.elastic_heap", "softmx_percent=60");
            System.out.println(output.getOutput());
            // Allocate 400k per 1ms, 400M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[400*1024];
                Thread.sleep(1);
            }

            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 60, uncommitted memory 419430400 B]");
            int rss60 = getRss();
            System.out.println("60% rss: " + rss60);
            Asserts.assertTrue(Math.abs(fullRss - rss60) > 350 * 1024);

            System.gc();

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=80");
            System.out.println(output.getOutput());
            // Allocate 400k per 1ms, 400M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[400*1024];
                Thread.sleep(1);
            }

            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 80, uncommitted memory 209715200 B]");
            int rss80 = getRss();
            System.out.println("80% rss: " + rss80);
            Asserts.assertTrue(Math.abs(fullRss - rss80) > 150 * 1024);
            Asserts.assertTrue(Math.abs(rss80 - rss60) > 150 * 1024);

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=0");
            System.out.println(output.getOutput());
            // Allocate 400k per 1ms, 400M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[400*1024];
                Thread.sleep(1);
            }

            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: inactive]");
            int rss100 = getRss();
            System.out.println("100% rss: " + rss100);
            Asserts.assertTrue(Math.abs(fullRss - rss100) < 100 * 1024);

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=70");
            System.out.println(output.getOutput());
            // Allocate 400k per 1ms, 400M per second
            for (int i = 0; i < 1000 * 10; i++) {
                arr = new byte[400*1024];
                Thread.sleep(1);
            }
            System.gc();

            output = triggerJcmd("GC.elastic_heap", null);
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 70, uncommitted memory 314572800 B]");
            int rss70 = getRss();
            System.out.println("70% rss: " + rss70);
            Asserts.assertTrue(Math.abs(fullRss - rss70) > 250 * 1024);
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
}
