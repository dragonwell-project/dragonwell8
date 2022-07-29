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
import java.util.concurrent.CountDownLatch;
import java.security.NoSuchAlgorithmException;
import java.security.MessageDigest;

import static java.security.MessageDigest.getInstance;

import static jdk.testlibrary.Asserts.*;

/*
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary Test hierarchical tenants support
 * @library /lib/testlibrary
 * @run main/othervm/bootclasspath -XX:+MultiTenant -XX:+TenantCpuThrottling -Xmx200m -Xms200m
 *                                 com.alibaba.tenant.TestHierachicalTenants
 */

public class TestHierachicalTenants {
    private static final int LOAD_NUM = 50;

    private void workLoad(int load) throws NoSuchAlgorithmException {
        MessageDigest md5 = getInstance("MD5");
        int count = load;
        while (--count > 0) {
            md5.update("hello world!!!".getBytes());
            if (count % 20 == 0) {
                Thread.yield();
            }
        }
    }

    private void testParentLimit() throws NoSuchAlgorithmException {
        TenantConfiguration config = new TenantConfiguration()
                .limitCpuSet("0")  // forcefully bind to cpu-0
                .limitCpuCfs(200000, 100000);
        TenantContainer parent = TenantContainer.create(config);

        workLoad(1_000_000); // warm up

        long timeLimitMillis = 20_000;
        long[] counts = new long[5];
        try {
            // counts[counts.length - 1] is the baseline number
            parent.run(() -> {
                // the primary counter loop-1
                long startTime = System.currentTimeMillis();
                while (System.currentTimeMillis() - startTime < timeLimitMillis) {
                    try {
                        workLoad(LOAD_NUM);
                    } catch (NoSuchAlgorithmException e) {
                        e.printStackTrace();
                    }
                    ++counts[counts.length - 1];
                }
            });

            // create two children tenant containers, the overall limit should not exceed parent limit
            CountDownLatch start = new CountDownLatch(1);
            TenantContainer[] childrenTenants = new TenantContainer[counts.length - 1];
            Thread[] threads = new Thread[childrenTenants.length];

            for (int i = 0; i < childrenTenants.length; ++i) {
                final int idx = i;
                parent.run(() -> {
                    childrenTenants[idx] = TenantContainer.create(config);
                });
                childrenTenants[i].run(() -> {
                    threads[idx] = new Thread(() -> {
                        try {
                            start.await();
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                            fail();
                        }
                        // below counter loop must be identical to parent's primary counter loop-1
                        long startTime = System.currentTimeMillis();
                        while (System.currentTimeMillis() - startTime < timeLimitMillis) {
                            try {
                                workLoad(LOAD_NUM);
                            } catch (NoSuchAlgorithmException e) {
                                e.printStackTrace();
                            }
                            ++counts[idx];
                        }
                    });
                    threads[idx].start();
                });
            }

            // trigger children tester thread
            start.countDown();

            // join all children
            for (Thread t : threads) {
                try {
                    t.join();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                    fail();
                }
            }

            System.out.println("counts:");
            for (long cnt : counts) {
                System.out.println(cnt);
            }

            // check result
            long sum = 0;
            double acceptableDiffPercent = 0.2;
            for (int i = 0; i < counts.length - 1; ++i) {
                assertLessThan(counts[i], counts[counts.length - 1]);
                sum += counts[i];
            }

            assertLessThan(Math.abs(sum - (double) counts[counts.length - 1]), counts[counts.length - 1] * acceptableDiffPercent);

        } catch (TenantException e) {
            e.printStackTrace();
            fail();
        }
    }

    private void testParentBasic() {
        TenantConfiguration config = new TenantConfiguration().limitCpuShares(1024);
        TenantContainer parent = TenantContainer.create(config);
        assertNull(getParent(parent));
        TenantContainer children[] = new TenantContainer[1];
        try {
            parent.run(() -> {
                children[0] = TenantContainer.create(config);
            });
        } catch (TenantException e) {
            e.printStackTrace();
            fail();
        }
        assertTrue(getParent(children[0]) == parent);
        children[0].destroy();
        assertNull(getParent(children[0]));
        assertNull(getParent(parent));
        parent.destroy();
    }

    private void testNullParent() {
        TenantConfiguration config = new TenantConfiguration().limitCpuShares(1024);
        TenantContainer tenant = TenantContainer.create(config);
        assertNull(getParent(tenant));
        tenant.destroy();
    }

    public static void main(String[] args) {
        TestUtils.runWithPrefix("test",
                TestHierachicalTenants.class,
                new TestHierachicalTenants());
    }

    // -------------- utilities ---------------
    private static TenantContainer getParent(TenantContainer config) {
        if (config != null) {
            TenantResourceContainer parentRes = config.resourceContainer.getParent();
            if (parentRes != null) {
                return parentRes.getTenant();
            }
        }
        return null;
    }
}
