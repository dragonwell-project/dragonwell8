/*
 * Copyright (c) 2023 Alibaba Group Holding Limited. All Rights Reserved.
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
import com.alibaba.wisp.engine.WispCounter;

import javax.management.MBeanServer;
import java.io.*;
import java.lang.management.ManagementFactory;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.*;

import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;

import static jdk.testlibrary.Asserts.assertTrue;

/* @test
 * @summary WispCounterMXBean unit test for Detail profile data using the API with the specified WispCarrier
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm/timeout=2000 -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.config=/tmp/wisp.config -Dcom.alibaba.wisp.profile=true WispMonitorDataTest
 */

public class WispMonitorDataTest {

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
                synchronized (WispMonitorDataTest.class) {
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
        Thread.sleep(1_000L);

        Class<?> clazz = Class.forName("com.alibaba.wisp.engine.WispEngine");
        Field field = clazz.getDeclaredField("carrierThreads");
        field.setAccessible(true);
        Set<Thread> set = (Set<Thread>) field.get(null);
        JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();

        for (Thread thread : set) {
            WispCounter wispdata = mbean.getWispCounter(thread.getId());
            if (wispdata != null) {
                System.out.println("WispTask is " + thread.getId());
                System.out.println(wispdata.getCompletedTaskCount());
                System.out.println(wispdata.getTotalExecutionTime());
                System.out.println(wispdata.getExecutionCount());
                System.out.println(wispdata.getTotalEnqueueTime());
                System.out.println(wispdata.getEnqueueCount());
            }
        }
    }

    private static ServerSocket ss;
    private static final int PORT = 23002;
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
