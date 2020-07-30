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

/*
 * @test
 * @library /lib/testlibrary
 * @summary test after long running or blocking syscall task could be retaken, the new carrier thread can be profiled.
 * @requires os.family == "linux"
 * @run main/othervm -Dcom.alibaba.wisp.carrierEngines=1 -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true  -Dcom.alibaba.wisp.version=2 -Dcom.alibaba.wisp.enableHandOff=true -Dcom.alibaba.wisp.handoffPolicy=ADAPTIVE -Dcom.alibaba.wisp.enablePerfLog=true -Dcom.alibaba.wisp.logTimeInternalMillis=1000 ProfileWithHandOffTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.management.WispCounterMXBean;
import sun.misc.SharedSecrets;

import javax.management.MBeanServer;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static jdk.testlibrary.Asserts.assertTrue;

public class ProfileWithHandOffTest {
    public static void main(String[] args) throws Exception {
        CountDownLatch cl = new CountDownLatch(10);
        AtomicInteger cnt = new AtomicInteger();
        for (int i = 0; i < 10; i++) {
            WispEngine.dispatch(() -> {
                try {
                    Thread.sleep(1000);
                    cnt.incrementAndGet();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                cl.countDown();
            });
        }

        WispEngine.dispatch(() -> {
            try {
                for(int j = 0; j < 30; j++) {
                    int i = System.in.read();
                    System.out.println("your input: " + i);
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        });

        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        WispCounterMXBean mbean = null;
        try {
            mbean = ManagementFactory.newPlatformMXBeanProxy(mbs,
                    "com.alibaba.management:type=WispCounter", WispCounterMXBean.class);
        } catch (IOException e) {
            e.printStackTrace();
        }

        System.out.println(mbean.getRunningStates());
        System.out.println(mbean.getQueueLength());
        System.out.println(mbean.getNumberOfRunningTasks());
        assertTrue(cl.await(5, TimeUnit.SECONDS));

        System.out.println(mbean.getCreateTaskCount());
        System.out.println(mbean.getCompleteTaskCount());
    }
}
