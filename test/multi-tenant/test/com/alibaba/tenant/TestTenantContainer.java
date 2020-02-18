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

package test.com.alibaba.tenant;

import com.alibaba.tenant.*;
import org.junit.Test;
import static org.junit.Assert.*;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicLong;
import java.util.stream.IntStream;

/* @test
 * @summary unit tests for com.alibaba.tenant.TenantContainer
 * @library /lib/testlibrary
 * @compile TestTenantContainer.java
 * @run junit/othervm/timeout=300  -XX:+MultiTenant -XX:+UseG1GC -Xmx600m  -Xms200m
 *    -Dcom.alibaba.tenant.test.prop=root test.com.alibaba.tenant.TestTenantContainer
 */
public class TestTenantContainer {
    static private final int MAP_SIZE = 128;
    static private final int MAP_ARRAY_LENGTH = 40;

    private Map<Integer, String> populateMap(int size) {
        Map<Integer, String> map = new HashMap<Integer, String>();
        for (int i = 0; i < size; i += 1) {
            String valStr = "value is [" + i + "]";
            map.put(i, valStr);
        }
        return map;
    }

    @Test
    public void testRunInRootTenant() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            assertSame(TenantContainer.current(), tenant);
            TenantContainer.primitiveRunInRoot(() -> {
                //should be in root tenant.
                assertNull(TenantContainer.current());
            });
        });
    }

    @Test
    public void testTenantSystemProperty() {
        String value = System.getProperty("com.alibaba.tenant.enableMultiTenant");
        assertTrue(value != null && "true".equalsIgnoreCase(value));
    }

    @Test
    public void testCurrent() throws TenantException {
        // should be in the root tenant at the begging
        assertNull(TenantContainer.current());

        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            // run in 'current' tenant
            assertSame(TenantContainer.current(), tenant);
            System.out.println("testCurrent: thread [" + Thread.currentThread().getName() + "] is running in tenant: "
                    + TenantContainer.current().getTenantId());
        });

        // switch back to root tenant.
        assertNull(TenantContainer.current());
        tenant.destroy();
    }

    @Test
    public void testCurrentWithGC() throws TenantException {
        // should be in the root tenant at the begging
        assertNull(TenantContainer.current());

        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            int arrayIndex = 0;
            Object[] array = new Object[MAP_ARRAY_LENGTH];
            while (arrayIndex < MAP_ARRAY_LENGTH * 20) {
                // run in 'current' tenant
                assertSame(TenantContainer.current(), tenant);
                Map<Integer, String> map = populateMap(MAP_SIZE);
                array[arrayIndex % MAP_ARRAY_LENGTH] = map;
                arrayIndex++;

                Thread.yield();
                System.gc();
            }

            System.out.println("testCurrent: thread [" + Thread.currentThread().getName() + "] is running in tenant: "
                    + TenantContainer.current().getTenantId());

        });

        // switch back to root tenant.
        assertNull(TenantContainer.current());
        tenant.destroy();
    }

    @Test
    public void testGetTenantID() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            assertEquals(TenantContainer.current().getTenantId(), tenant.getTenantId());
            System.out.println("testGetTenantID: thread [" + Thread.currentThread().getName()
                    + "] is running in tenant: " + TenantContainer.current().getTenantId());
        });
        tenant.destroy();
    }

    @Test
    public void testGetState() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        assertTrue(TenantState.STARTING == tenant.getState());
        tenant.run(() -> {
            assertTrue(TenantState.RUNNING == tenant.getState());
            System.out.println("testGetState: thread [" + Thread.currentThread().getName() + "] is running in tenant: "
                    + TenantContainer.current().getTenantId());
        });
        tenant.destroy();
    }

    @Test
    public void testRun() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        final TenantContainer tenant2 = TenantContainer.create(tconfig);
        try {
            tenant.run(() -> {
                // it is not allowed to run into tenant 2 from tenant
                try {
                    tenant2.run(() -> {
                        // should not reach here.
                        fail();
                    });
                } catch (TenantException e) {
                    // Expected
                }
                // it is allowed to run into same tenant
                try {
                    tenant.run(() -> {
                        assertEquals(TenantContainer.current().getTenantId(), tenant.getTenantId());
                        System.out.println("testRun: thread [" + Thread.currentThread().getName()
                                + "] is running in tenant: " + TenantContainer.current().getTenantId());
                    });
                } catch (TenantException e) {
                    fail();
                }

            });
        } catch (TenantException e) {
            fail();
        }
        tenant.destroy();
        tenant2.destroy();
    }

    @Test
    public void testGetAttachedThreads() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            Thread[] threads = tenant.getAttachedThreads();
            assertEquals(threads.length, 1);
            assertEquals(threads[0].getId(), Thread.currentThread().getId());
            System.out.println("testGetAttachedThreads: thread [" + Thread.currentThread().getName()
                    + "] is running in tenant: " + TenantContainer.current().getTenantId());
        });
        tenant.destroy();
    }

    @Test
    public void testTenantInheritance() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            assertSame(TenantContainer.current(), tenant);
            Thread thread = new Thread(() -> {
                TenantContainer tc = TenantContainer.current();
                assertSame(tc, tenant);
                Thread[] threads = tc.getAttachedThreads();
                assertEquals(threads.length, 2);
                assertTrue(Thread.currentThread().getId() == threads[0].getId() ||
                           Thread.currentThread().getId() == threads[1].getId());

                System.out.println("testTenantInheritance: thread [" + Thread.currentThread().getName()
                        + "] is running in tenant: " + TenantContainer.current().getTenantId());
            });

            thread.start();
            try {
                thread.join();
            } catch (Exception e) {
                e.printStackTrace();
                fail();
            }
            Thread[] threads = tenant.getAttachedThreads();
            assertEquals(threads.length, 1);
            assertEquals(threads[0].getId(), Thread.currentThread().getId());
        });
        tenant.destroy();
    }

    @Test
    public void testTenantGetAllocatedMemory() throws TenantException, InterruptedException {
        TenantConfiguration tconfig  = new TenantConfiguration();
        final TenantContainer tenant = TenantContainer.create(tconfig);

        CountDownLatch p1 = new CountDownLatch(1);
        CountDownLatch p2 = new CountDownLatch(1);
        CountDownLatch p3 = new CountDownLatch(1);
        CountDownLatch p4 = new CountDownLatch(1);
        CountDownLatch startTest = new CountDownLatch(2);
        CountDownLatch endTest = new CountDownLatch(1);

        long allocatedMem0 = 0;
        long allocatedMem1 = 0;
        long allocatedMem2 = 0;
        long allocatedMem3 = 0;

        assertTrue(0 == tenant.getAllocatedMemory());
        tenant.run(() -> {
            Thread thread1 = new Thread(() -> {
                assertTrue(TenantContainer.current() == tenant);
                System.out.println("thread1 started");
                try {
                    byte[] byteArray0 = new byte[10240];
                    startTest.countDown();
                    p1.await();
                    byte[] byteArray1 = new byte[10240];
                    endTest.await();
                } catch (InterruptedException ee) {
                    ee.printStackTrace();
                    fail();
                } finally {
                    System.out.println("thread1 ended");
                }
            });
            thread1.start();
        });

        Thread thread2 = new Thread(() -> {
            System.out.println("thread2 started");
            try {
                startTest.countDown();

                p2.await();

                tenant.run(()->{
                    byte[] byteArray0 = new byte[10240];
                });

                p3.await();

                byte[] byteArray1 = new byte[10240];

                tenant.run(()->{
                    byte[] byteArray0 = new byte[10240];
                });
                byte[] byteArray2 = new byte[10240];
                endTest.await();

            } catch (InterruptedException | TenantException ee) {
                ee.printStackTrace();
                fail();
            } finally {
                System.out.println("thread2 ended");
            }
        });
        thread2.start();

        startTest.await();

        allocatedMem0 = tenant.getAllocatedMemory();
        System.out.println("allocatedMem0 = " + allocatedMem0);
        assertTrue(allocatedMem0 >= 10240);

        p1.countDown();
        Thread.sleep(1000);
        allocatedMem1 = tenant.getAllocatedMemory();
        System.out.println("allocatedMem1 = " + allocatedMem1);
        assertTrue((allocatedMem1 - allocatedMem0) >= 10240 && (allocatedMem1 - allocatedMem0) < 11240);

        p2.countDown();
        Thread.sleep(1000);
        allocatedMem2 = tenant.getAllocatedMemory();
        System.out.println("allocatedMem2 = " + allocatedMem2);
        assertTrue((allocatedMem2 - allocatedMem1) >= 10240 && (allocatedMem2 - allocatedMem1) < 11240);

        p3.countDown();
        Thread.sleep(1000);
        allocatedMem3 = tenant.getAllocatedMemory();
        System.out.println("allocatedMem3 = " + allocatedMem3);
        assertTrue((allocatedMem3 - allocatedMem2) >= 10240 && (allocatedMem3 - allocatedMem2) < 11240);

        p4.countDown();
        Thread.sleep(1000);
        tenant.destroy();

        endTest.countDown();
        System.out.println("testTenantGetAllocatedMemory passed!");
    }

    public static void main(String[] args) throws Exception {
        TestTenantContainer test = new TestTenantContainer();
        test.testCurrent();
        test.testGetTenantID();
        test.testGetState();
        test.testGetAttachedThreads();
        test.testRun();
        test.testCurrentWithGC();
        test.testTenantInheritance();
        test.testRunInRootTenant();
        test.testTenantGetAllocatedMemory();
    }
}
