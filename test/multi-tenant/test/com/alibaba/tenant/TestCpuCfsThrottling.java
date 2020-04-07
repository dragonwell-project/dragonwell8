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

import jdk.testlibrary.TestUtils;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import static jdk.testlibrary.Asserts.*;

/*
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary test.com.alibaba.tenant.TestCpuCfsThrottling
 * @library /lib/testlibrary
 * @run main/othervm/bootclasspath -Xint -XX:+MultiTenant -XX:+TenantCpuThrottling -XX:+TenantCpuAccounting
 *                                 -Xmx200m -Xms200m com.alibaba.tenant.TestCpuCfsThrottling
 */

public class TestCpuCfsThrottling {

    private void testCpuCfsQuotas() {
        // limit whole JVM process to be running on one CPU core
        Path jvmCpusetCpusPath = Paths.get(
                JGroup.rootPathOf("cpuset"),
                JGroup.JVM_GROUP_PATH,
                "cpuset.cpus");
        try {
            Files.write(jvmCpusetCpusPath, "0".getBytes() /* limit jvm process to CPU core-#0 */);
        } catch (IOException e) {
            e.printStackTrace();
            fail();
        }

        // create two tenants and start two threads running on same core, and fight for CPU resources
        int cfsPeriods[] = {1_000_000, 800_000};
        int cfsQuotas[] = {1_000_000, 300_000};
                TenantConfiguration configs[] = {
                        new TenantConfiguration().limitCpuCfs(cfsPeriods[0], cfsQuotas[0]).limitCpuSet("0"),
                        new TenantConfiguration().limitCpuCfs(cfsPeriods[1], cfsQuotas[1]).limitCpuSet("0")
        };
        TenantContainer tenants[] = new TenantContainer[configs.length];
        for (int i = 0; i < configs.length; ++i) {
            tenants[i] = TenantContainer.create(configs[i]);
        }
        long counters[] = new long[configs.length];

        // verify CGroup configurations
        for (int i = 0; i < configs.length; ++i) {
            int actual = Integer.parseInt(getConfig(tenants[i], "cpu.cfs_period_us"));
            assertEquals(cfsPeriods[i], actual);

            actual = Integer.parseInt(getConfig(tenants[i], "cpu.cfs_quota_us"));
            assertEquals(cfsQuotas[i], actual);
        }

        // Start two counter threads in different TenantContainers
        runSerializedCounters(tenants, counters, 10_000);

        // check results
        assertGreaterThan(counters[0], 0L);
        assertGreaterThan(counters[1], 0L);
        assertGreaterThan(counters[0], counters[1]);
        assertGreaterThan(counters[0], counters[1] * 2);
    }

    private void testAdjustCpuCfsQuotas() {
        // limit whole JVM process to be running on one CPU core
        Path jvmCpusetCpusPath = Paths.get(
                JGroup.rootPathOf("cpuset"),
                JGroup.JVM_GROUP_PATH,
                "cpuset.cpus");
        try {
            Files.write(jvmCpusetCpusPath, "0".getBytes() /* limit jvm process to CPU core-#0 */);
        } catch (IOException e) {
            e.printStackTrace();
            fail();
        }

        // create two tenants and start two threads running on same core, and fight for CPU resources
        TenantConfiguration configs[] = {
                new TenantConfiguration().limitCpuCfs(1_000_000, 800_000).limitCpuSet("0"),
                new TenantConfiguration().limitCpuCfs(1_000_000, 300_000).limitCpuSet("0")
        };
        TenantContainer tenants[] = new TenantContainer[configs.length];
        for (int i = 0; i < configs.length; ++i) {
            tenants[i] = TenantContainer.create(configs[i]);
        }
        long counters[] = new long[configs.length];

        // Start two counter threads in different TenantContainers
        runSerializedCounters(tenants, counters, 10_000);

        // check results
        assertGreaterThan(counters[0], 0L);
        assertGreaterThan(counters[1], 0L);
        assertGreaterThan(counters[0], counters[1]);
        assertGreaterThan(counters[0], counters[1] * 2);

        // cfs limitation adjustment needs time to take effect, here we sleep for a while!
        try {
            Thread.sleep(10_000);
        } catch (InterruptedException e) {
            e.printStackTrace();
            fail();
        }

        // ======== round 2 testing, after modify resouce limitations ==========
        tenants[0].update(configs[0].limitCpuCfs(1_000_000, 300_000));
        tenants[1].update(configs[1].limitCpuCfs(1_000_000, 800_000));
        counters[0] = 0L;
        counters[1] = 0L;
        runSerializedCounters(tenants, counters, 10_000);
        // check results
        assertGreaterThan(counters[0], 0L);
        assertGreaterThan(counters[1], 0L);
        assertGreaterThan(counters[1], counters[0]);
        assertGreaterThan(counters[1], counters[0] * 2);
    }

    // run several counters in concurrent threads
    private static void runConcurrentCounters(TenantContainer[] tenants,
                                    long[] counters,
                                    long milliLimit) {
        if (tenants == null || counters == null) {
            throw new IllegalArgumentException("Bad args");
        }
        // Start two counter threads in different TenantContainers
        CountDownLatch startCounting = new CountDownLatch(1);
        AtomicBoolean stopCounting = new AtomicBoolean(false);
        Thread threadRefs[] = new Thread[2];
        for (int i = 0; i < tenants.length; ++i) {
            TenantContainer tenant = tenants[i];
            int idx = i;
            try {
                // start a single non-root tenant thread to execute counter
                tenant.run(()-> {
                    threadRefs[idx] = new Thread(()->{
                        try {
                            startCounting.await();
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                            fail();
                        }
                        while (!stopCounting.get()) {
                            ++counters[idx];
                        }
                    });
                    threadRefs[idx].start();
                });
            } catch (TenantException e) {
                e.printStackTrace();
                fail();
            }
        }

        // ROOT tenant thread will serve as monitor & controller
        startCounting.countDown();
        try {
            // the absolute exec time limits for two tenant threads are same
            Thread.sleep(milliLimit);
            stopCounting.set(true);
            for (Thread t : threadRefs) {
                t.join();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
            fail();
        }
    }

    // execute an action after certain milli seconds, clock start ticking after notified by startLatch
    private static void runTimedTask(long afterMillis, CountDownLatch startLatch, Runnable action) {
        Thread timer = new Thread(() -> {
            if (startLatch != null) {
                try {
                    startLatch.await();
                } catch (InterruptedException e) {
                    // ignore
                }
            }

            long start = System.currentTimeMillis();
            long passed = 0;
            while (passed < afterMillis) {
                try {
                    Thread.sleep(afterMillis - passed);
                } catch (InterruptedException e) {
                    //ignore
                } finally {
                    passed = System.currentTimeMillis() - start;
                }
            }
            // performan action after specific time
            action.run();
        });
        timer.start();
    }

    private static void runSerializedCounters(TenantContainer[] tenants,
                                    long[] counters,
                                    long milliLimit) {
        if (tenants == null || counters == null) {
            throw new IllegalArgumentException("Bad args");
        }

        AtomicBoolean stopCounting = new AtomicBoolean(false);

        for (int i = 0; i < tenants.length; ++i) {
            int idx = i;
            stopCounting.set(false);
            CountDownLatch startCounting = new CountDownLatch(1);
            TenantContainer tenant = tenants[i];
            runTimedTask(milliLimit, startCounting, ()-> {
                stopCounting.set(true);
            });
            try {
                // start a single non-root tenant thread to execute counter
                tenant.run(() -> {
                    startCounting.countDown();
                    while (!stopCounting.get()) {
                        ++counters[idx];
                    }
                });
            } catch (TenantException e) {
                e.printStackTrace();
                fail();
            }
        }
    }

    public static void main(String[] args) {
        TestUtils.runWithPrefix("test",
                TestCpuCfsThrottling.class,
                new TestCpuCfsThrottling());
    }

    //------ utililties
    private String getConfig(TenantContainer tenant, String configName) {
        String controllerName = configName.split("\\.")[0];
        Path configPath = Paths.get(JGroup.rootPathOf(controllerName),
                JGroup.JVM_GROUP_PATH,
                "t" + tenant.getTenantId(),
                configName);
        try {
            return new String(Files.readAllBytes(configPath)).trim();
        } catch (IOException e) {
            e.printStackTrace();
            fail();
        }
        return null;
    }
}
