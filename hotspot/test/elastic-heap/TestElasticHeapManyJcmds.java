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
 * @summary test elastic-heap with many jcmds
 * @library /testlibrary
 * @build TestElasticHeapManyJcmds
 * @run main/othervm/timeout=300 TestElasticHeapManyJcmds
 */

class TestElasticHeapManyJcmdsSetter implements Runnable {
    private int loopNum;
    public TestElasticHeapManyJcmdsSetter(int ln) {
        loopNum = ln;
    }
    private OutputAnalyzer triggerJcmd(String arg1, String arg2) throws Exception {
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
    public void run() {
        OutputAnalyzer output;
        for (int i = 0; i < loopNum; i++) {
            int p = 50 + i % 50;
            try {
                output = triggerJcmd("GC.elastic_heap", "young_commit_percent=" + p);
                System.out.println(output.getOutput());
            } catch (Exception e) {
                System.out.println("Error: "+ e.getMessage());
            }
        }
    }
}

public class TestElasticHeapManyJcmds {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        if (Platform.isDebugBuild()) {
            serverBuilder = ProcessTools.createJavaProcessBuilder(
                "-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server.class.getName(), "10");
        } else {
            serverBuilder = ProcessTools.createJavaProcessBuilder(
                "-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                Server.class.getName(), "30");
        }
        Process server = serverBuilder.start();

        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("[GC.elastic_heap");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            int loopNum = Integer.parseInt(args[0]);
            byte[] arr = new byte[200*1024];
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            TestElasticHeapManyJcmdsSetter setter = new TestElasticHeapManyJcmdsSetter(loopNum);
            int count = Runtime.getRuntime().availableProcessors();
            Thread[] t = new Thread[count];
            for (int i = 0; i < count ; i++) {
                t[i] = new Thread(setter, "t" + i);
                t[i].start();
            }
            for (int i = 0; i < 1000 * 60; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            System.exit(0);
        }
    }
}
