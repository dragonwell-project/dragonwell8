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
 * @summary test elastic-heap with jcmd
 * @library /testlibrary
 * @build TestElasticHeapGenerationLimit
 * @run main/othervm/timeout=500
         -XX:+UseG1GC -XX:+G1ElasticHeap -Xmx1000m -Xms1000m -XX:+AlwaysPreTouch
                -XX:MaxNewSize=400m -XX:G1HeapRegionSize=1m
                -XX:ElasticHeapYGCIntervalMinMillis=500
                -verbose:gc -XX:+PrintGCDetails -XX:+PrintGCTimeStamps
                TestElasticHeapGenerationLimit
 */

public class TestElasticHeapGenerationLimit {
    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 3; i++) {
            test();
        }
    }
    public static void test() throws Exception {
        byte[] arr = new byte[200*1024];
        OutputAnalyzer output;
        // Allocate 200k per 1ms, 200M per second
        // so 0.5 GCs per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        int rssFull = getRss();
        System.out.println("Full rss: " + rssFull);
        System.gc();
        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=50");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 50, uncommitted memory 209715200 B]");
        output.shouldHaveExitValue(0);
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        int rss50 = getRss();
        System.out.println("50% rss: " + rss50);
        Asserts.assertTrue(rss50 < rssFull);
        Asserts.assertTrue(Math.abs(rssFull - rss50) > 150 * 1024);

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=100");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 100, uncommitted memory 0 B]");
        output.shouldHaveExitValue(0);
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        int rss100 = getRss();
        System.out.println("100% rss: " + rss100);
        Asserts.assertTrue(Math.abs(rss100 - rssFull) < 50 * 1024);
        Asserts.assertTrue(Math.abs(rss100 - rss50) > 150 * 1024);

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=0");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: inactive]");
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        int rss0 = getRss();
        System.out.println("Recover rss: " + rss0);
        Asserts.assertTrue(Math.abs(rss0 - rssFull) < 50 * 1024);
        Asserts.assertTrue(Math.abs(rss0 - rss50) > 150 * 1024);

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=30");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 30, uncommitted memory 293601280 B]");
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        System.gc();
        int rss30 = getRss();
        System.out.println("rss30: " + rss30);
        Asserts.assertTrue(rss30 < rss50);
        Asserts.assertTrue(Math.abs(rss30 - rssFull) > 250 * 1024);
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=20");
        System.out.println(output.getOutput());
        output.shouldContain("gc is too frequent");
        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=40");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 40, uncommitted memory 251658240 B]");
        for (int i = 0; i < 1000 * 3; i++) {
            arr = new byte[200*1024];
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=0");
        System.out.println(output.getOutput());
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
