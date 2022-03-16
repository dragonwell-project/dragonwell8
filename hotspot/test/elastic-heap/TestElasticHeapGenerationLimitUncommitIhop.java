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
 * @build TestElasticHeapGenerationLimitUncommitIhop
 * @run main/othervm/timeout=200 -XX:+UseG1GC -XX:+G1ElasticHeap -Xmx1000m -Xms1000m -XX:MaxNewSize=400m -XX:G1HeapRegionSize=1m -XX:ElasticHeapEagerMixedGCIntervalMillis=1000 -XX:ElasticHeapInitialMarkIntervalMinMillis=0 -XX:+AlwaysPreTouch -verbose:gc -XX:+PrintGCDetails -XX:+PrintGCTimeStamps -XX:+PrintGCDateStamps -XX:ElasticHeapYGCIntervalMinMillis=10 TestElasticHeapGenerationLimitUncommitIhop
 */

public class TestElasticHeapGenerationLimitUncommitIhop {
    public static void main(String[] args) throws Exception {
        byte[] arr = new byte[200*1024];
        OutputAnalyzer output;
        // Promote 480M into old gen
        Object[] root = new Object[5 * 1024];
        for (int i = 0; i < 1000 * 5 - 200; i++) {
            root[i] = new byte[100*1024];
            Thread.sleep(1);
        }
        System.gc();
        int rssFull = getRss();
        System.out.println("Full rss: " + rssFull);
        root = null;
        output = triggerJcmd("GC.elastic_heap", "uncommit_ihop=30", "young_commit_percent=50");
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap: young generation commit percent 50, uncommitted memory 209715200 B]");
        output.shouldHaveExitValue(0);
        // Guaranteed gc will happen and mixed gc will finish
        for (int i = 0; i < 1000 * 10; i++) {
            arr = new byte[200];
            Thread.sleep(1);
        }
        int rssLess = getRss();
        System.out.println("Less rss: " + rssLess);
        Asserts.assertTrue(rssLess < rssFull);
        Asserts.assertTrue(Math.abs(rssFull - rssLess) > 350 * 1024);
    }

    private static OutputAnalyzer triggerJcmd(String arg1, String arg2, String arg3) throws Exception {
        String pid = Integer.toString(ProcessTools.getProcessId());
        JDKToolLauncher jcmd = JDKToolLauncher.create("jcmd")
                                              .addToolArg(pid);
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
