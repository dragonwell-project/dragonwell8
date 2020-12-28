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

import com.alibaba.management.*;
import javax.management.*;
import javax.management.remote.*;
import java.lang.management.ManagementFactory;
import java.io.File;
import jdk.test.lib.*;
import jdk.test.lib.dcmd.*;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import static jdk.test.lib.Asserts.*;

/* @test
 * @summary test elastic-heap error code
 * @library /lib /
 * @compile TestElasticHeapErrorCode.java
 * @run main/othervm/timeout=100 TestElasticHeapErrorCode
 */

public class TestElasticHeapErrorCode {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=500",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                Server.class.getName());
        Process server = serverBuilder.start();
        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("-XX:+G1ElasticHeap is not enabled");
        assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=500",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                Server.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("Error: percent should be");
        output.shouldContain("Error: gc is too frequent");
        output.shouldContain("Error: young_commit_percent should be");
        output.shouldContain("Error: command fails because gc is too frequent");
        assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            MBeanServer server = ManagementFactory.getPlatformMBeanServer();
            ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(server,
                                                      "com.alibaba.management:type=ElasticHeap",
                                                      ElasticHeapMXBean.class);
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(5);
            } catch (Exception e) {
                System.out.println("Error: "+ e.getMessage());
            }
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(50);
            } catch (Exception e) {
                System.out.println("Error: "+ e.getMessage());
            }
            PidJcmdExecutor je = new PidJcmdExecutor();
            je.execute("GC.elastic_heap young_commit_percent=5");
            je.execute("GC.elastic_heap young_commit_percent=50");
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
    }
}
