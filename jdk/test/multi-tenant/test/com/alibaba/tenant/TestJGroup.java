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
package com.alibaba.tenant;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

import static java.nio.file.StandardOpenOption.WRITE;

/*
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary Test JGroup
 * @library /lib/testlibrary
 * @run main/othervm/bootclasspath/manual -XX:+MultiTenant -XX:+TenantCpuAccounting -XX:+TenantCpuThrottling -XX:+UseG1GC -Xmx200m -Xms200m -Dcom.alibaba.tenant.DebugJGroup=true com.alibaba.tenant.TestJGroup
 */

class JGroupWorker implements Runnable {
    private long id;
    public long count = 0;
    public JGroup group;

    JGroupWorker(long id) {
        this.id = id;
    }

    public void run() {
        TenantContainer tenant = TenantContainer.create(
                new TenantConfiguration()
                        .limitCpuShares((int) id * 512 + 512)
                        .limitCpuSet("0")); // limit all tasks to one CPU to forcefully create contention
        group = new JGroup(tenant.resourceContainer);
        group.attach();
        long msPre = System.currentTimeMillis();
        while (System.currentTimeMillis() - msPre < (20000 - 1000 * id)) {
            count++;
        }
    }
}

class JGroupsWorker implements Runnable {
    private JGroup t1;
    private JGroup t2;
    private JGroup t3;
    public long count1 = 0;
    public long count2 = 0;
    public long count3 = 0;

    JGroupsWorker(JGroup t1, JGroup t2, JGroup t3) {
        this.t1 = t1;
        this.t2 = t2;
        this.t3 = t3;
    }

    public void run() {
        t1.attach();
        long msPre = System.currentTimeMillis();
        while (System.currentTimeMillis() - msPre < 5000) {
            count1++;
        }
        t1.detach();

        t2.attach();
        msPre = System.currentTimeMillis();
        while (System.currentTimeMillis() - msPre < 4000) {
            count2++;
        }
        t2.detach();

        t3.attach();
        msPre = System.currentTimeMillis();
        while (System.currentTimeMillis() - msPre < 3000) {
            count3++;
        }
        t3.detach();
    }
}


public class TestJGroup {

    private static void setUp() {
        JGroup.jvmGroup().setValue("cpuset.cpus", "0");
    }

    static void mySleep(int t) {
        try {
            Thread.sleep(t);
        } catch (InterruptedException e) {
            System.out.println("Interreupted...");
        }
    }

    public static void main(String[] args) throws Exception {
        setUp();

        long usage1 = 0;
        long usage2 = 0;
        long usage3 = 0;
        JGroupWorker jw1 = new JGroupWorker(0);
        JGroupWorker jw2 = new JGroupWorker(1);
        JGroupWorker jw3 = new JGroupWorker(2);
        Thread t1 = new Thread(jw1);
        Thread t2 = new Thread(jw2);
        Thread t3 = new Thread(jw3);
        t1.start();
        t2.start();
        t3.start();

        mySleep(1000);

        JGroupsWorker js = new JGroupsWorker(jw1.group,
                                       jw2.group,
                                       jw3.group);
        Thread t4 =new Thread(js);
        t4.start();
        try {
            t4.join();
        } catch (InterruptedException e) {
            System.out.println("Interreupted...");
        }

        try {
            t1.join();
        } catch (InterruptedException e) {
            System.out.println("Interreupted...");
        }
        usage1 = jw1.group.getCpuTime();
        jw1.group.destory();

        if(usage1 <= 0 ) {
            throw new Exception("Invalid cpu usage, usage1: "+ usage1);
        }
        try {
            t2.join();
        } catch (InterruptedException e) {
            System.out.println("Interreupted...");
        }
        usage2 = jw2.group.getCpuTime();
        jw2.group.destory();

        if(usage2 <= 0 ) {
            throw new Exception("Invalid cpu usage, usage2: "+ usage2);
        }
        try {
            t3.join();
        } catch (InterruptedException e) {
            System.out.println("Interreupted...");
        }
        usage3 = jw3.group.getCpuTime();
        jw3.group.destory();

        if(usage3 <= 0 ) {
            throw new Exception("Invalid cpu usage, usage3: "+ usage3);
        }

        if ((js.count3 < js.count2) || (js.count2 < js.count1)) {
            throw new Exception("com.alibaba.tenant.JGroupsWorker test failed!"
                                + " count1 = " + js.count1
                                + " count2 = " + js.count2
                                + " count3 = " + js.count3);
        }

        if ((jw3.count < jw2.count) || (jw2.count < jw1.count)) {
            throw new Exception("Three com.alibaba.tenant.JGroupWorker test failed!"
                                + " jw1 count = " + jw1.count
                                + " jw2 count = " + jw2.count
                                + " jw3 count = " + jw3.count);
        }

        if ((usage3 < usage2) || (usage2 < usage1)) {
            throw new Exception("Three com.alibaba.tenant.JGroupWorker test failed!"
                                + " jw1 usage = " + usage1
                                + " jw2 usage = " + usage2
                                + " jw3 usage = " + usage3);
        }
    }
}
