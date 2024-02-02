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
import java.util.*;
import jdk.test.lib.*;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import static jdk.test.lib.Asserts.*;

/* @test
 * @summary test elastic-heap MX bean
 * @library /lib /test/lib
 * @compile TestElasticHeapMXBean.java
 * @run main/othervm/timeout=100 TestElasticHeapMXBean
 */

public class TestElasticHeapMXBean {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;

        if (System.getProperty("java.vm.version").toLowerCase().contains("debug")) {
            serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                    "-XX:+G1ElasticHeap", "-Xmx1000m", "-Xms1000m",
                    "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                    "-XX:ElasticHeapYGCIntervalMinMillis=50",
                    "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                    Server.class.getName());
            Process server = serverBuilder.start();

            OutputAnalyzer output = new OutputAnalyzer(server);
            System.out.println(output.getOutput());
            output.shouldContain("Set young percent 50: 50");
            output.shouldContain("Set freed bytes 50M: 52428800");
            output.shouldContain("Set young percent 100: 100");
            output.shouldContain("Set freed bytes 0M: 0");
            output.shouldContain("Set softmx percent 70: 70");
            output.shouldContain("Set freed bytes 300M: 314572800");
            output.shouldContain("Set softmx percent 50: 50");
            output.shouldContain("Set freed bytes 500M: 524288000");
            output.shouldContain("Evaluation mode: inactive");
            assertTrue(output.getExitValue() == 0);
        }
        else {
            // Release build uses JMX remote access
            Random rand = new Random(25);
            int i;
            i = rand.nextInt(10000);
            int port = 10000 + i;
            serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
                "-XX:+G1ElasticHeap", "-Xmx1000m", "-Xms1000m",
                "-Xmn100m", "-XX:G1HeapRegionSize=1m",
                "-Dcom.sun.management.jmxremote.port=" + port,
                "-Dcom.sun.management.jmxremote.ssl=false",
                "-Dcom.sun.management.jmxremote.authenticate=false",
                "-XX:ElasticHeapYGCIntervalMinMillis=50",
                "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
                "-Xloggc:gc.log",
                ServerJMX.class.getName());
            Process server = serverBuilder.start();
            Thread.sleep(5000);
            ProcessBuilder clientBuilder;
            clientBuilder = ProcessTools.createJavaProcessBuilder(Client.class.getName(), port + "");
            OutputAnalyzer output = new OutputAnalyzer(clientBuilder.start());
            OutputAnalyzer outputServer = new OutputAnalyzer(server);
            System.out.println(outputServer.getOutput());
            output.shouldContain("Set young percent 50: 50");
            output.shouldContain("Set freed bytes 50M: 52428800");
            output.shouldContain("Set young percent 100: 100");
            output.shouldContain("Set freed bytes 0M: 0");
            output.shouldContain("Set heap percent 70: 70");
            output.shouldContain("Set freed bytes 300M: 314572800");
            output.shouldContain("Set heap percent 50: 50");
            output.shouldContain("Set freed bytes 500M: 524288000");
            output.shouldContain("Evaluation mode: inactive");

            assertTrue(output.getExitValue() == 0);
            assertTrue(outputServer.getExitValue() == 0);
        }
    }

    private static class Client {
        public static void main(String[] args) {
            try {
                JMXServiceURL url = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://" + "127.0.0.1:" + args[0] + "/jmxrmi");
                MBeanServerConnection connection = JMXConnectorFactory.connect(url, null).getMBeanServerConnection();
                ElasticHeapMXBean elasticHeapMXBean = ManagementFactory.newPlatformMXBeanProxy(connection,
                    "com.alibaba.management:type=ElasticHeap",
                    ElasticHeapMXBean.class);
                elasticHeapMXBean.setYoungGenCommitPercent(50);
                System.out.println("Set young percent 50: " + elasticHeapMXBean.getYoungGenCommitPercent());
                System.out.println("Set freed bytes 50M: " + elasticHeapMXBean.getTotalYoungUncommittedBytes());
                Thread.sleep(3000);
                elasticHeapMXBean.setYoungGenCommitPercent(100);
                System.out.println("Set young percent 100: " + elasticHeapMXBean.getYoungGenCommitPercent());
                System.out.println("Set freed bytes 0M: " + elasticHeapMXBean.getTotalYoungUncommittedBytes());

                elasticHeapMXBean.setYoungGenCommitPercent(0);
                Thread.sleep(3000);

                elasticHeapMXBean.setSoftmxPercent(70);
                Thread.sleep(3000);
                System.out.println("Set heap percent 70: " + elasticHeapMXBean.getSoftmxPercent());
                System.out.println("Set freed bytes 300M: " + elasticHeapMXBean.getTotalUncommittedBytes());
                Thread.sleep(3000);
                elasticHeapMXBean.setSoftmxPercent(50);
                Thread.sleep(3000);
                System.out.println("Set heap percent 50: " + elasticHeapMXBean.getSoftmxPercent());
                System.out.println("Set freed bytes 500M: " + elasticHeapMXBean.getTotalUncommittedBytes());
                Thread.sleep(3000);
                elasticHeapMXBean.setSoftmxPercent(0);
                Thread.sleep(3000);
                System.out.println("Evaluation mode: " + (elasticHeapMXBean.getEvaluationMode() == ElasticHeapEvaluationMode.INACTIVE? "inactive" : "unexpected"));

            } catch (Exception e) {
                System.out.println("Error: "+ e.getMessage());
            }
        }
    }

    private static class ServerJMX {
        public static void main(String[] args) throws Exception {
            byte[] arr = new byte[200*1024];
            // Allocate 200k per 1ms, 200M per second
            // so 2 GCs per second
            for (int i = 0; i < 1000 * 40; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
        }
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
            for (int i = 0; i < 1000 * 3; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            elasticHeapMXBean.setYoungGenCommitPercent(50);
            System.out.println("Set young percent 50: " + elasticHeapMXBean.getYoungGenCommitPercent());
            System.out.println("Set freed bytes 50M: " + elasticHeapMXBean.getTotalYoungUncommittedBytes());
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            elasticHeapMXBean.setYoungGenCommitPercent(100);
            System.out.println("Set young percent 100: " + elasticHeapMXBean.getYoungGenCommitPercent());
            System.out.println("Set freed bytes 0M: " + elasticHeapMXBean.getTotalYoungUncommittedBytes());
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            elasticHeapMXBean.setYoungGenCommitPercent(0);
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }

            elasticHeapMXBean.setSoftmxPercent(70);
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            System.out.println("Set softmx percent 70: " + elasticHeapMXBean.getSoftmxPercent());
            System.out.println("Set freed bytes 300M: " + elasticHeapMXBean.getTotalUncommittedBytes());
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            elasticHeapMXBean.setSoftmxPercent(50);
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            System.out.println("Set softmx percent 50: " + elasticHeapMXBean.getSoftmxPercent());
            System.out.println("Set freed bytes 500M: " + elasticHeapMXBean.getTotalUncommittedBytes());
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            elasticHeapMXBean.setSoftmxPercent(0);
            for (int i = 0; i < 1000 * 5; i++) {
                arr = new byte[200*1024];
                Thread.sleep(1);
            }
            System.out.println("Evaluation mode: " + (elasticHeapMXBean.getEvaluationMode() == ElasticHeapEvaluationMode.INACTIVE? "inactive" : "unexpected"));

        }
    }
}
