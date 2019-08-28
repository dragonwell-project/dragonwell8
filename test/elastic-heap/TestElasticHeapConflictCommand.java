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
 * @summary test elastic-heap conflict commands
 * @library /testlibrary
 * @build TestElasticHeapConflictCommand
 * @run main/othervm/timeout=200
         -XX:+UseG1GC -XX:+G1ElasticHeap -Xmx1000m -Xms1000m
                -XX:ElasticHeapPeriodicYGCIntervalMillis=400
                -XX:ElasticHeapPeriodicUncommitStartupDelay=0
                -Xmn200m -XX:G1HeapRegionSize=1m -XX:+AlwaysPreTouch
                -XX:ElasticHeapYGCIntervalMinMillis=50
                -verbose:gc -XX:+PrintGCDetails -XX:+PrintGCTimeStamps
                TestElasticHeapConflictCommand
 */

public class TestElasticHeapConflictCommand {
    public static void main(String[] args) throws Exception {
        OutputAnalyzer output;
        byte[] arr = new byte[200*1024];
        // Allocate 200k per 1ms , 200M per second
        // so 2 GCs per second
        for (int i = 0; i < 1000 * 3; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        int rssFull = getRss();
        System.out.println("rssFull: " + rssFull);

        output = triggerJinfo("+ElasticHeapPeriodicUncommit");
        output.shouldHaveExitValue(0);
        // Allocate 200k per ms , 200M per second
        // so 1 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=50");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);

        output = triggerJcmd("GC.elastic_heap", "uncommit_ihop=50");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);

        output = triggerJcmd("GC.elastic_heap", "softmx_percent=50");
        System.out.println(output.getOutput());
        output.shouldContain("not in correct mode");
        output.shouldHaveExitValue(0);

        output = triggerJinfo("-ElasticHeapPeriodicUncommit");
        output.shouldHaveExitValue(0);
        // Allocate 200k per ms , 200M per second
        // so 1 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=50");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 50, uncommitted memory 104857600 B]");
        output.shouldHaveExitValue(0);

        output = triggerJinfo("+ElasticHeapPeriodicUncommit");
        System.out.println(output.getOutput());
        output.shouldContain("cannot be set because of illegal state");
        output.shouldHaveExitValue(1);

        output = triggerJcmd("GC.elastic_heap", "softmx_percent=80");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);


        // Allocate 200k per ms , 200M per second
        // so 1 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=0", "uncommit_ihop=30");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 100, uncommitted memory 0 B]");
        output.shouldHaveExitValue(0);

        // Allocate 200k per ms , 200M per second
        // so 1 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }
        output = triggerJinfo("+ElasticHeapPeriodicUncommit");
        System.out.println(output.getOutput());
        output.shouldContain("cannot be set because of illegal state");
        output.shouldHaveExitValue(1);

        output = triggerJcmd("GC.elastic_heap", "softmx_percent=80");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);

        output = triggerJcmd("GC.elastic_heap", "uncommit_ihop=0");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: inactive]");
        output.shouldHaveExitValue(0);
        // Allocate 200k per ms , 200M per second
        // so 1 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJcmd("GC.elastic_heap", "softmx_percent=80");
        System.out.println(output.getOutput());
        output.shouldContain("in softmx mode");
        output.shouldHaveExitValue(0);
        // Allocate 400k per ms , 400M per second
        // so 2 GC per second
        for (int i = 0; i < 1000 * 5; i++) {
            arr = new byte[200*1024];
            arr = new byte[200*1024];
            Thread.sleep(1);
        }

        output = triggerJinfo("+ElasticHeapPeriodicUncommit");
        System.out.println(output.getOutput());
        output.shouldContain("cannot be set because of illegal state");
        output.shouldHaveExitValue(1);

        output = triggerJcmd("GC.elastic_heap", "young_commit_percent=80");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);

        output = triggerJcmd("GC.elastic_heap", "uncommit_ihop=30");
        System.out.println(output.getOutput());
        output.shouldContain("Error: not in correct mode");
        output.shouldHaveExitValue(0);
    }

    private static OutputAnalyzer triggerJcmd(String arg1, String arg2) throws Exception {
        return triggerJcmd(arg1, arg2, null);
    }
    private static OutputAnalyzer triggerJcmd(String arg1, String arg2, String arg3) throws Exception {
        String pid = Integer.toString(ProcessTools.getProcessId());
        JDKToolLauncher jcmd = JDKToolLauncher.create("jcmd").addToolArg(pid);
        if (arg1 != null) {
            jcmd.addToolArg(arg1);
        }
        if (arg2 != null) {
            jcmd.addToolArg(arg2);
        }
        if (arg3 != null) {
            jcmd.addToolArg(arg3);
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
