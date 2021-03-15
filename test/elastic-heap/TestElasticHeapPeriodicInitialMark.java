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
 * @summary test elastic-heap periodic old gc uncommit with RSS check
 * @library /testlibrary /testlibrary/whitebox
 * @build TestElasticHeapPeriodicInitialMark
 * @run main/othervm/timeout=200 TestElasticHeapPeriodicInitialMark
 */

public class TestElasticHeapPeriodicInitialMark {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap", "-XX:+ElasticHeapPeriodicUncommit",
            "-XX:ElasticHeapPeriodicInitialMarkIntervalMillis=5000",
            "-XX:ElasticHeapEagerMixedGCIntervalMillis=1000",
            "-XX:ElasticHeapPeriodicMinYoungCommitPercent=100",
            "-XX:ElasticHeapPeriodicUncommitStartupDelay=0",
            "-Xmx1g", "-Xms1g", "-Xmn200m", "-XX:G1HeapRegionSize=1m",
            "-XX:ElasticHeapYGCIntervalMinMillis=50",
            "-XX:ElasticHeapInitialMarkIntervalMinMillis=0",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-XX:+PrintGCDateStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server.class.getName());
        Process server = serverBuilder.start();
        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        Asserts.assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap", "-XX:+ElasticHeapPeriodicUncommit",
            "-XX:ElasticHeapPeriodicInitialMarkIntervalMillis=5000",
            "-XX:ElasticHeapPeriodicMinYoungCommitPercent=100",
            "-XX:ElasticHeapPeriodicUncommitStartupDelay=0",
            "-Xmx1g", "-Xms1g", "-Xmn200m", "-XX:G1HeapRegionSize=1m",
            "-XX:ElasticHeapYGCIntervalMinMillis=50",
            "-XX:ElasticHeapInitialMarkIntervalMinMillis=0",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-XX:+PrintGCDateStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server2.class.getName());
        server = serverBuilder.start();
        output = new OutputAnalyzer(server);
        output.shouldContain("initial-mark");
        output.shouldNotContain("(Elastic Heap Initiated GC) (young),");
        output.shouldMatch("(Elastic Heap Initiated GC).*initial-mark");
        System.out.println(output.getOutput());
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server2 {
        public static void main(String[] args) throws Exception {
            byte[] arr = new byte[200*1024];
            // Guaranteed gc will happen and mixed gc will finish
            for (int i = 0; i < 1000 * 20; i++) {
                arr = new byte[100 * 1024];
                arr = new byte[100 * 1024];
                Thread.sleep(1);
            }
        }
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            // Pretouch young gen
            byte[] arr = new byte[200*1024];
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
            }

            // Promote 500M into old gen
            Object[] root = new Object[5 * 1024];
            for (int i = 0; i < 5 * 1024; i++) {
                root[i] = new byte[100*1024];
            }
            System.gc();
            root = null;
            int fullRss = getRss();

            // Guaranteed gc will happen and mixed gc will finish
            for (int i = 0; i < 1000 * 15; i++) {
                arr = new byte[2*1024];
                Thread.sleep(1);
            }
            int lessRss = getRss();
            Asserts.assertTrue((fullRss > lessRss) && (Math.abs(fullRss - lessRss) > 300 * 1024));
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
