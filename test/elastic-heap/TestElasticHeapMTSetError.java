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
 * @summary test elastic-heap error with multiple threads setting
 * @library /lib /
 * @compile TestElasticHeapMTSetError.java
 * @run main/othervm/timeout=100 TestElasticHeapMTSetError
 */

class ElasticHeapMTSetErrorSetter implements Runnable {
    public void run() {
        MBeanServer server = ManagementFactory.getPlatformMBeanServer();
        try {
            ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(server,
                                                  "com.alibaba.management:type=ElasticHeap",
                                                  ElasticHeapMXBean.class);
            elasticHeapMXBean.setYoungGenCommitPercent(50);
        } catch (Exception e) {
            System.out.println("Error: "+ e.getMessage());
        }
    }
}

public class TestElasticHeapMTSetError {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap",
                "-Xmx1g", "-Xms1g",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-Dtest.jdk=" + System.getProperty("test.jdk"),
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                Server.class.getName());
        Process server = serverBuilder.start();
        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        output.shouldContain("last elastic-heap resize is still in progress");
        output.shouldContain("Elastic Heap concurrent cycle ends");

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
            ElasticHeapMTSetErrorSetter setter = new ElasticHeapMTSetErrorSetter();
            Thread t1 = new Thread(setter, "t1");
            Thread t2 = new Thread(setter, "t2");
            Thread t3 = new Thread(setter, "t3");
            Thread t4 = new Thread(setter, "t4");
            Thread t5 = new Thread(setter, "t5");
            t1.start();
            t2.start();
            t3.start();
            t4.start();
            t5.start();
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
    }
}
