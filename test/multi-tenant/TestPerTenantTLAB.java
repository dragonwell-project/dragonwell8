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
 *
 */

/*
 * @test
 * @summary Test retain and reuse of TLAB
 * @library /testlibrary /testlibrary/whitebox
 * @build TestPerTenantTLAB
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:+WhiteBoxAPI -XX:+UsePerTenantTLAB -XX:+TenantHeapIsolation -XX:+UseG1GC -XX:+UseTLAB -XX:TLABSize=65535 -Xmx1024M -Xms512M -XX:G1HeapRegionSize=1M TestPerTenantTLAB
 *
 */

import static com.oracle.java.testlibrary.Asserts.*;

import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import com.alibaba.tenant.TenantState;
import sun.hotspot.WhiteBox;

import java.util.concurrent.CountDownLatch;

public class TestPerTenantTLAB {

    private static final WhiteBox WB = WhiteBox.getWhiteBox();

    private static final int G1_HEAP_REGION_SIZE = WB.g1RegionSize();

    private static final int G1_HEAP_REGION_MASK = (0xFFFFFFFF << Integer.numberOfTrailingZeros(G1_HEAP_REGION_SIZE));

    // non-adaptive TLAB size, see -XX:TLABSize from command line options
    private static final int TLAB_SIZE = 65535;

    private void testRetainReuseTLABBasic() {
        TenantContainer tenant = TenantContainer.create(new TenantConfiguration().limitHeap(64 * 1024 * 1024));
        Object[] refs = new Object[16];

        WB.fullGC();
        // after full GC, below allocation request will start from the beginning (almost) of new EDEN regions.

        try {
            refs[0] = new Object();
            refs[1] = new Object();
            assertInCurrentTLAB(refs[0], refs[1]);

            tenant.run(()->{
                refs[2] = new Object();
                refs[3] = new Object();
                assertInCurrentTLAB(refs[2], refs[3]);
                assertNotInCurrentTLAB(refs[0], refs[1]);
            });

            assertNotInCurrentTLAB(refs[2], refs[3]);
            assertInCurrentTLAB(refs[0], refs[1]);
            assertNotInSameRegion(refs[1], refs[2]);

            refs[4] = new Object();
            refs[5] = new Object();

            assertInCurrentTLAB(refs[4], refs[5]);
            assertNotInSameRegion(refs[3], refs[4]);
            assertInCurrentTLAB(refs[0], refs[1], refs[4], refs[5]);

            tenant.run(()->{
                refs[6] = new Object();
                refs[7] = new Object();
                assertInCurrentTLAB(refs[2], refs[3], refs[6], refs[7]);
                assertNotInSameRegion(refs[4], refs[7]);
                assertNotInSameRegion(refs[5], refs[6]);
            });

            refs[8] = new Object();
            refs[9] = new Object();
            assertInCurrentTLAB(refs[0], refs[1], refs[4], refs[5], refs[8], refs[9]);
        } catch (TenantException e) {
            throw new RuntimeException(e);
        } finally {
            tenant.destroy();
        }
    }

    private void testChildThread() {
        TenantContainer tenant = TenantContainer.create(new TenantConfiguration().limitHeap(64 * 1024 * 1024));
        Object[] refs = new Object[16];

        WB.fullGC();
        // after full GC, below allocation request will start from the beginning (almost) of new EDEN regions.

        try {
            tenant.run(()-> {
                refs[0] = new Object();
                refs[1] = new Object();
                assertInCurrentTLAB(refs[0], refs[1]);

                Thread t = new Thread(()->{
                    refs[2] = new Object();
                    refs[3] = new Object();
                    assertInCurrentTLAB(refs[2], refs[3]);
                    assertNotInCurrentTLAB(refs[0], refs[1]);

                    TenantContainer.primitiveRunInRoot(()-> {
                        refs[4] = new Object();
                        refs[5] = new Object();
                        assertInCurrentTLAB(refs[4], refs[5]);
                        assertNotInSameRegion(refs[2], refs[5]);
                    });
                });

                // wait child thread to end
                t.start();
                try {
                    t.join();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }

                // now newly allocated ROOT threads should be in same TLAB of previous ROOT objects
                TenantContainer.primitiveRunInRoot(()->{
                    refs[6] = new Object();
                    refs[7] = new Object();
                    assertInCurrentTLAB(refs[6], refs[7]);
                    assertInSameRegion(refs[4], refs[6]);
                    assertInSameRegion(refs[4], refs[7]);
                });
            });
        } catch (TenantException e) {
            throw new RuntimeException(e);
        }

        refs[8] = new Object();
        refs[9] = new Object();
        assertInCurrentTLAB(refs[8], refs[9], refs[6], refs[7]);

        Thread t = new Thread(()->{
            refs[10] = new Object();
            refs[11] = new Object();
            assertInCurrentTLAB(refs[10], refs[11]);
            assertNotInCurrentTLAB(refs[8], refs[9]);
            assertInSameRegion(refs[8], refs[10]);
            assertInSameRegion(refs[4], refs[11]);
            assertInSameRegion(refs[5], refs[11]);
            assertInSameRegion(refs[6], refs[11]);
        });
        t.start();
        try {
            t.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        } finally {
            tenant.destroy();
        }
    }

    private void testAfterDestroyTenant() {
        TenantContainer tenant = TenantContainer.create(new TenantConfiguration().limitHeap(64 * 1024 * 1024));
        Object[] refs = new Object[2];
        CountDownLatch cdl = new CountDownLatch(1);
        CountDownLatch started = new CountDownLatch(1);
        assertInCurrentTLAB(refs, cdl);

        System.gc();
        Thread thread = new Thread(()->{
            try {
                tenant.run(()->{
                    refs[0] = new Object();
                    assertTrue(TenantContainer.containerOf(refs[0]) == tenant);
                    assertTrue(WB.isInCurrentTLAB(refs[0]));

                    started.countDown();

                    // attach and hold
                    try {
                        cdl.await();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                        throw new RuntimeException(e);
                    }
                });
            } catch (TenantException e) {
                e.printStackTrace();
                throw new RuntimeException(e);
            }
        });
        thread.start();

        try {
            started.await();

            assertTrue(TenantContainer.containerOf(refs[0]) == tenant);

            tenant.destroy();
            assertTrue(tenant.getState() == TenantState.DEAD);

            // should have been moved to root
            assertNull(TenantContainer.containerOf(refs[0]));

            // trigger destroy
            cdl.countDown();

            thread.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public static void main(String[] args) {
        TestPerTenantTLAB test = new TestPerTenantTLAB();
        test.testRetainReuseTLABBasic();
        test.testChildThread();
        test.testAfterDestroyTenant();
    }

    private static void assertInCurrentTLAB(Object...objs) {
        for (Object o : objs) {
            assertTrue(WB.isInCurrentTLAB(o));
        }
    }

    private static void assertNotInSameTLAB(Object o1, Object o2) {
        assertGreaterThanOrEqual((int)Math.abs(WB.getObjectAddress(o1) - WB.getObjectAddress(o2)), TLAB_SIZE);
    }

    private static void assertNotInCurrentTLAB(Object... objs) {
        for (Object o : objs) {
            assertFalse(WB.isInCurrentTLAB(o));
        }
    }

    private static void assertNotInSameRegion(Object o1, Object o2) {
        int addr1 = (int)WB.getObjectAddress(o1) & G1_HEAP_REGION_MASK;
        int addr2 = (int)WB.getObjectAddress(o2) & G1_HEAP_REGION_MASK;
        assertNotEquals(addr1, addr2);
    }

    private static void assertInSameRegion(Object o1, Object o2) {
        int addr1 = (int)WB.getObjectAddress(o1) & G1_HEAP_REGION_MASK;
        int addr2 = (int)WB.getObjectAddress(o2) & G1_HEAP_REGION_MASK;
        assertEquals(addr1, addr2);
    }
}
