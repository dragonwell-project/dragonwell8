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
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import static jdk.test.lib.Asserts.*;

/* @test
 * @summary test elastic-heap MX bean exception
 * @library /lib /test/lib
 * @compile TestElasticHeapMXBeanException.java
 * @run main/othervm/timeout=100 TestElasticHeapMXBeanException
 */

public class TestElasticHeapMXBeanException {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        Process server;
        OutputAnalyzer output;

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                ServerNotEnabled.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:+G1ElasticHeap", "-XX:+ElasticHeapPeriodicUncommit",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                ServerIncorrectMode.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        assertTrue(output.getExitValue() == 0);

        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:+G1ElasticHeap",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                ServerIncorrectMode2.class.getName());
        server = serverBuilder.start();

        output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        assertTrue(output.getExitValue() == 0);

    }

    private static class ServerNotEnabled {
        public static void main(String[] args) throws Exception {
            MBeanServer server = ManagementFactory.getPlatformMBeanServer();
            ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(server,
                                                      "com.alibaba.management:type=ElasticHeap",
                                                      ElasticHeapMXBean.class);
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            int percent = 0;
            long bytes = 0;
            ElasticHeapEvaluationMode mode;
            try {
                mode = elasticHeapMXBean.getEvaluationMode();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                percent = elasticHeapMXBean.getYoungGenCommitPercent();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setUncommitIHOP(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                percent = elasticHeapMXBean.getUncommitIHOP();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                bytes = elasticHeapMXBean.getTotalYoungUncommittedBytes();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setSoftmxPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                percent = elasticHeapMXBean.getSoftmxPercent();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
            try {
                bytes = elasticHeapMXBean.getTotalUncommittedBytes();
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("-XX:+G1ElasticHeap is not enabled")) {
                    fail();
                }
            }
        }
    }
    private static class ServerIncorrectMode {
        public static void main(String[] args) throws Exception {
            MBeanServer server = ManagementFactory.getPlatformMBeanServer();
            ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(server,
                                                      "com.alibaba.management:type=ElasticHeap",
                                                      ElasticHeapMXBean.class);
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            int percent = 0;
            long bytes = 0;
            ElasticHeapEvaluationMode mode;
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setUncommitIHOP(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setSoftmxPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
        }
    }

    private static class ServerIncorrectMode2 {
        public static void main(String[] args) throws Exception {
            MBeanServer server = ManagementFactory.getPlatformMBeanServer();
            ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(server,
                                                      "com.alibaba.management:type=ElasticHeap",
                                                      ElasticHeapMXBean.class);
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            int percent = 0;
            long bytes = 0;
            ElasticHeapEvaluationMode mode;
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(50);
            } catch (Exception e) {
                fail();
            }
            try {
                percent = elasticHeapMXBean.getSoftmxPercent();
            } catch (Exception e) {
                fail();
            }
            try {
                elasticHeapMXBean.setSoftmxPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setUncommitIHOP(50);
            } catch (Exception e) {
                fail();
            }
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(0);
            } catch (Exception e) {
                fail();
            }
            try {
                elasticHeapMXBean.setSoftmxPercent(50);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
            try {
                elasticHeapMXBean.setUncommitIHOP(0);
            } catch (Exception e) {
                fail();
            }
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            try {
                elasticHeapMXBean.setSoftmxPercent(50);
            } catch (Exception e) {
                fail();
            }
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            try {
                percent = elasticHeapMXBean.getSoftmxPercent();
            } catch (Exception e) {
                fail();
            }

            assertTrue(percent == 50);
            try {
                elasticHeapMXBean.setYoungGenCommitPercent(60);
                fail();
            } catch (Exception e) {
                if (!e.getMessage().contains("not in correct mode")) {
                    fail();
                }
            }
        }
    }
}
