/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
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

import com.alibaba.management.WispCounterMXBean;

import javax.management.MBeanServer;
import java.io.*;
import java.lang.management.ManagementFactory;
import java.lang.reflect.Method;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import static jdk.testlibrary.Asserts.assertTrue;

/* @test
 * @summary WispCounterMXBean unit test for Detail profile data
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm/timeout=2000 -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.config=/tmp/wisp.config -Dcom.alibaba.wisp.profile=true -Dcom.alibaba.wisp.enableProfileLog=true -Dcom.alibaba.wisp.logTimeInternalMillis=3000 TestWispDetailCounter
 */

public class TestWispDetailCounter {

    public static void main(String[] args) throws Exception {
        startNetServer();
        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        WispCounterMXBean mbean = null;
        try {
            mbean = ManagementFactory.newPlatformMXBeanProxy(mbs,
                    "com.alibaba.management:type=WispCounter", WispCounterMXBean.class);
        } catch (IOException e) {
            e.printStackTrace();
        }

        ExecutorService es = Executors.newFixedThreadPool(20);
        int taskTotal = 40;
        for (int i = 0; i < taskTotal; i++) {
            // submit task
            es.submit(() -> {
                // do park/unpark
                synchronized (TestWispDetailCounter.class) {
                    // do sleep
                    try {
                        Thread.sleep(10L);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                // do net IO
                doNetIO();
            });
        }

        System.out.println(mbean.getRunningStates());
        System.out.println(mbean.getQueueLength());
        System.out.println(mbean.getNumberOfRunningTasks());
        // wait task complete
        Thread.sleep(1_000L);
        System.out.println(mbean.getCreateTaskCount());
        System.out.println(mbean.getCompleteTaskCount());
        System.out.println(mbean.getUnparkCount());
        assertTrue(mbean.getUnparkCount().stream().mapToLong(Long::longValue).sum() > 0);
        System.out.println(mbean.getTotalBlockingTime());
        assertTrue(mbean.getTotalBlockingTime().stream().mapToLong(Long::longValue).sum() > 0);
        System.out.println(mbean.getWaitSocketIOCount());
        System.out.println(mbean.getTotalWaitSocketIOTime());
        System.out.println(mbean.getEnqueueCount());
        assertTrue(mbean.getEnqueueCount().stream().mapToLong(Long::longValue).sum() > 0);
        System.out.println(mbean.getTotalEnqueueTime());
        assertTrue(mbean.getTotalEnqueueTime().stream().mapToLong(Long::longValue).sum() > 0);
        System.out.println(mbean.getExecutionCount());
        assertTrue(mbean.getExecutionCount().stream().mapToLong(Long::longValue).sum() > 0);
        System.out.println(mbean.getTotalExecutionTime());
        assertTrue(mbean.getTotalExecutionTime().stream().mapToLong(Long::longValue).sum() > 0);

        es.submit(() -> {
            try {
                Thread.sleep(1000);
            } catch (Exception e) {
            }
        });
        System.out.println(mbean.getNumberOfRunningTasks());
        assertTrue(mbean.getNumberOfRunningTasks().stream().mapToLong(Long::longValue).sum() > 0);

        // check log file exist
        File file = new File("wisplog0.log");
        if (!file.exists()) {
            assertTrue(false, "log file isn't generated");
        }
    }

    private static ServerSocket ss;
    private static final int PORT = 23001;
    private static final int BUFFER_SIZE = 1024;

    private static void startNetServer() throws IOException {
        ss = new ServerSocket(PORT);
        Thread t = new Thread(() -> {
            try {
                while (true) {
                    Socket cs = ss.accept();
                    InputStream is = cs.getInputStream();
                    int r = is.read(new byte[BUFFER_SIZE]);
                    is.close();
                    cs.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        t.setDaemon(true);
        t.start();
    }

    private static void doNetIO() {
        try {
            Socket so = new Socket("localhost", PORT);
            OutputStream os = so.getOutputStream();
            os.write(new byte[BUFFER_SIZE]);
            os.flush();
            os.close();
        } catch (IOException e) {
            e.printStackTrace();
            throw new Error(e);
        }
    }
}
