
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
 * @summary Test isolation of per-tenant Java heap space
 * @library /testlibrary /testlibrary/whitebox
 * @build TestHeapIsolation
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm/timeout=600 -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+MultiTenant -XX:+TenantHeapIsolation -XX:+WhiteBoxAPI -XX:+UseG1GC -Xmx2048M -Xms1024M -XX:G1HeapRegionSize=1M TestHeapIsolation
 *
 */

import static com.oracle.java.testlibrary.Asserts.*;

import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import sun.hotspot.WhiteBox;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.stream.IntStream;

public class TestHeapIsolation {

    // GC operation types, used to trigger GC operation during testing
    enum GCType {
        NO_GC,
        YOUNG_GC,
        FULL_GC,
        SYSTEM_GC
    }

    // Allocation operation type of objects
    enum AllocType {
        AT_HUMONGOUS,
        AT_TINY,
        AT_MIXED
    }

    private static final WhiteBox wb;

    // Size of single G1 heap region
    private static final int HEAP_REGION_SIZE;

    // to access com.alibaba.TenantContainer.allocationContext
    private static Field allocationContextField;

    private static boolean verbose = "true".equalsIgnoreCase(System.getProperty("TestHeapIsolationVerbose"));

    static {
        wb = WhiteBox.getWhiteBox();

        HEAP_REGION_SIZE = wb.g1RegionSize();

        // provide accessor to TenantContainer.allocationContext
        try {
            allocationContextField = TenantContainer.class.getDeclaredField("allocationContext");
            allocationContextField.setAccessible(true);
        } catch (NoSuchFieldException e) {
            e.printStackTrace();
        }
    }

    public static void main(String args[]) throws Exception {
        new TestHeapIsolation().runAllTests();
    }

    void runAllTests() throws Exception {
        assertTrue(HEAP_REGION_SIZE > 0);

        testEmptyTenantOperation();
        testTenantOccupiedMemory();

        // test with single tenant for sanity
        testTenantAllocation(1, GCType.NO_GC, AllocType.AT_TINY);
        testTenantAllocation(1, GCType.YOUNG_GC, AllocType.AT_TINY);
        testTenantAllocation(1, GCType.FULL_GC, AllocType.AT_TINY);
        testTenantAllocation(1, GCType.SYSTEM_GC, AllocType.AT_TINY);

        // test with two tenants
        testTenantAllocation(2, GCType.NO_GC, AllocType.AT_TINY);
        testTenantAllocation(2, GCType.YOUNG_GC, AllocType.AT_TINY);
        testTenantAllocation(2, GCType.FULL_GC, AllocType.AT_TINY);
        testTenantAllocation(2, GCType.SYSTEM_GC, AllocType.AT_TINY);

        // test with multiple tenants, a little pressure
        testTenantAllocation(128, GCType.NO_GC, AllocType.AT_TINY);
        testTenantAllocation(128, GCType.YOUNG_GC, AllocType.AT_TINY);
        testTenantAllocation(128, GCType.FULL_GC, AllocType.AT_TINY);
        testTenantAllocation(128, GCType.SYSTEM_GC, AllocType.AT_TINY);

        // test with mixed allocation
        testTenantAllocation(32, GCType.NO_GC, AllocType.AT_MIXED);
        testTenantAllocation(32, GCType.YOUNG_GC, AllocType.AT_MIXED);
        testTenantAllocation(32, GCType.FULL_GC, AllocType.AT_MIXED);
        testTenantAllocation(32, GCType.SYSTEM_GC, AllocType.AT_MIXED);
    }

    /*
     *
     * The test tries to enforce:
     * 1, Object allocation should happen in the correct tenant container
     * 2, Object copy operations during GC should not violate tenant container boundary
     *
     * @param count     Create {@count} non-root tenant containers in the test
     * @param gcType    Trigger GC during doing tenant container allocations
     * @param allocType Allocated object size type
     */
    void testTenantAllocation(int count, GCType gcType, AllocType allocType) throws Exception {
        System.out.println(">> Begin TEST testTenantAllocation ("
                + count + " containers, gcType = " + gcType.name() + ", allocType=" + allocType.name());

        assertTrue(count > 0, "Cannot test with " + count + " tenants");

        // local array holds objects to prevent them from being potentially reclaimed by GC,
        // NOTE: there are cross-tenant references in this array
        Object[] objHolder = new Object[count];
        Object[] objHolder2 = new Object[count];

        final Object objRootBefore = allocateObject(allocType);
        TenantContainer curTenant = TenantContainer.containerOf(objRootBefore);
        assertNull(curTenant, "Object was allocated in wrong container " + allocationContextStringOf(curTenant));

        // the configuration here is
        TenantConfiguration config = new TenantConfiguration().limitHeap(32 * 1024 * 1024 /* 32MB heap */);

        // to prevent TenantContainer from being GCed
        TenantContainer[] tenants = new TenantContainer[count];

        // if multiple tenant containers (>8), we'll run them in different threads
        int threadRunLimit = 8;
        if (count > threadRunLimit) {
            int taskPerThread = count / threadRunLimit;
            Thread[] thread = new Thread[threadRunLimit];

            for (int i = 0; i < threadRunLimit; ++i) {
                final int thrdIndex = i;

                thread[i] = new Thread() {

                    public void run() {
                        for (int j = 0; j < taskPerThread; ++j) {
                            // index in the tenant contaner array, and obj holder arrays
                            final int idx = thrdIndex * taskPerThread + j;

                            // quit loop if we exceeded limit of 'count'
                            if (idx >= count) break;

                            tenants[idx] = TenantContainer.create(config);
                            try {
                                tenants[idx].run(() -> {
                                    objHolder[idx] = allocateObject(allocType);

                                    assert(null != TenantContainer.containerOf(objHolder[idx]));
                                    assert(tenants[idx] == TenantContainer.containerOf(objHolder[idx]));

                                    IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder[" + idx + "]@0x"
                                            + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
                                    IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2[" + idx + "]@0x"
                                            + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

                                    triggerGC(gcType);

                                    IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder[" + idx + "]@0x"
                                            + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
                                    IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2[" + idx + "]@0x"
                                            + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

                                    objHolder2[idx] = allocateObject(allocType);
                                    assert(null != TenantContainer.containerOf(objHolder2[idx]));
                                    assert(tenants[idx] == TenantContainer.containerOf(objHolder2[idx]));

                                    assertEquals(tenants[idx], TenantContainer.containerOf(objHolder[idx]));
                                    assertEquals(tenants[idx], TenantContainer.containerOf(objHolder2[idx]));
                                    assertInSameContainer(objHolder[idx], objHolder2[idx]);
                                });
                            } catch (TenantException e) {
                                e.printStackTrace();
                            }
                        }
                    }
                };
                thread[i].start();
            }

            for (int i = 0; i < threadRunLimit; ++i) {
                thread[i].join();
            }
        } else {
            for (int i = 0; i < count; ++i) {
                tenants[i] = TenantContainer.create(config);
                final int idx = i;
                tenants[i].run(() -> {
                    objHolder[idx] = allocateObject(allocType);

                    IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder[" + idx + "]@0x"
                            + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
                    IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2[" + idx + "]@0x"
                            + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

                    triggerGC(gcType);

                    IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder[" + idx + "]@0x"
                            + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
                    IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2[" + idx + "]@0x"
                            + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

                    objHolder2[idx] = allocateObject(allocType);
                    assert(null != TenantContainer.containerOf(objHolder2[idx]));
                    assert(tenants[idx] == TenantContainer.containerOf(objHolder2[idx]));

                    assertEquals(tenants[idx], TenantContainer.containerOf(objHolder[idx]));
                    assertEquals(tenants[idx], TenantContainer.containerOf(objHolder2[idx]));
                    assertInSameContainer(objHolder[idx], objHolder2[idx]);
                });
            }
        }

        // perform gc after tenant creation
        IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder@0x"
                + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
        IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2@0x"
                + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

        triggerGC(gcType);

        IntStream.range(0, objHolder.length).forEach(x -> verbose_cr("objHolder@0x"
                + Long.toHexString(wb.getObjectAddress(objHolder[x]))));
        IntStream.range(0, objHolder2.length).forEach(x -> verbose_cr("objHolder2@0x"
                + Long.toHexString(wb.getObjectAddress(objHolder2[x]))));

        // allocate object in root tenant after execution of tenant code
        Object objRootAfter = allocateObject(allocType);

        curTenant = TenantContainer.containerOf(objRootBefore);
        assertNull(curTenant, "Object was copied to wrong container " + allocationContextStringOf(curTenant));
        curTenant = TenantContainer.containerOf(objRootAfter);
        assertNull(curTenant, "Object was copied to wrong container " + allocationContextStringOf(curTenant));

        for (int i = 0; i < count; ++i) {
            assertNotNull(objHolder[i]);
            assertNotNull(objHolder2[i]);
            assertInSameContainer(objHolder[i], objHolder2[i]);
            curTenant = TenantContainer.containerOf(objHolder[i]);
            assertNotNull(curTenant, "Object["+i+"] @0x" + Long.toHexString(wb.getObjectAddress(objHolder[i]))+
                    " was copied to wrong container " + allocationContextStringOf(curTenant));
            curTenant = TenantContainer.containerOf(objHolder2[i]);
            assertNotNull(curTenant, "Object was copied to wrong container " + allocationContextStringOf(curTenant));

            for (int j = 0; j < count; ++j) {
                if (i != j) {
                    assertNotEquals(TenantContainer.containerOf(objHolder[i]), TenantContainer.containerOf(objHolder[j]));
                    assertNotEquals(TenantContainer.containerOf(objHolder2[i]), TenantContainer.containerOf(objHolder2[j]));
                    assertNotEquals(TenantContainer.containerOf(objHolder[i]), TenantContainer.containerOf(objHolder2[j]));
                    assertNotEquals(TenantContainer.containerOf(objHolder2[i]), TenantContainer.containerOf(objHolder[j]));
                }
            }
        }

        // destroy all native objects
        Arrays.stream(tenants).forEach(t -> t.destroy());

        // after tenant destruction, all objects should belong to ROOT tenant
        Arrays.stream(objHolder).forEach(
                obj -> assertNull(TenantContainer.containerOf(obj), "Should be owned by root tenant"));
        Arrays.stream(objHolder2).forEach(
                obj -> assertNull(TenantContainer.containerOf(obj), "Should be owned by root tenant"));

        System.out.println("<< End TEST testTenantAllocation");
    }

    // invoke GC on Java heap
    private static void triggerGC(GCType gcType) {
        switch (gcType) {
            case FULL_GC:
                wb.fullGC();
                break;
            case YOUNG_GC:
                wb.youngGC();
                break;
            case SYSTEM_GC:
                System.gc();
                break;
            case NO_GC:
            default:
                // nothing to do
        }
    }

    void testTenantOccupiedMemory() {

        // clean up heap space
        System.gc();

        TenantConfiguration config = new TenantConfiguration().limitHeap(64 * 1024 * 1024);
        TenantContainer tenant = TenantContainer.create(config);
        assertTrue(0L == tenant.getOccupiedMemory());
        try {
            // something will be allocated anyway
            tenant.run(()->{
                assertEquals(0, tenant.getOccupiedMemory());
                int i = 1;
                assertEquals(0, tenant.getOccupiedMemory());
            });
            // detach() will allocate some garbage anyway, but still no cross-tenant references
            assertEquals(HEAP_REGION_SIZE, tenant.getOccupiedMemory());

            // slight allocation, occupied size should be 1 * RegionSize;
            tenant.run(()->{
                Object o = new byte[1];
            });
            assertEquals(HEAP_REGION_SIZE, tenant.getOccupiedMemory());

            // allocate a humongous object, size should be 2 * regionsize;
            Object[] strongRefs = new Object[1];
            tenant.run(()->{
                for (int i = 0; i < 4; ++i) {
                    strongRefs[0] = new byte[HEAP_REGION_SIZE / 4];
                }
            });
            assertEquals(HEAP_REGION_SIZE * 2, tenant.getOccupiedMemory());

            // do full gc to release above newly created garbage
            System.gc();
            assertEquals(HEAP_REGION_SIZE, tenant.getOccupiedMemory());

            strongRefs[0] = null;

            System.gc();
            assertEquals(0, tenant.getOccupiedMemory());

            // scenario that live region survived full GC
            Object refs[] = new Object[1];
            tenant.run(()->{
                refs[0] = new byte[1];
                System.gc();
                refs[0] = new byte[1];
            });
            assertEquals(HEAP_REGION_SIZE * 2, // 1 old, 1 eden
                    tenant.getOccupiedMemory());
            System.gc(); // will still left one region
            assertEquals(HEAP_REGION_SIZE, // 1 old
                    tenant.getOccupiedMemory());
        } catch (TenantException e) {
            throw new RuntimeException(e);
        }
    }

    /*
     * Determine if memory isolation works correctly if no non-root tenant allocation happened
     *
     */
    void testEmptyTenantOperation() throws Exception {
        System.out.println(">> Begin TEST testEmptyTenantOperation");

        // the configuration here is
        TenantConfiguration config = new TenantConfiguration().limitHeap(128 * 1024 * 1024 /* 128 MB heap */);
        for (int i = 0; i < 16; ++i) {
            TenantContainer tenant = TenantContainer.create(config);
            tenant.run(() -> {
                // Empty! and no tenant allocation happened
            });
            tenant.destroy();
        }

        System.gc();

        System.out.println("<< End TEST testEmptyTenantOperation");
    }


    // allcoate one Java object whose data occupies {@code size} bytes
    private static Object allocateObjectOfSize(int size) {
        if (size > 0) {
            verbose("Allocate object of size:" + size);
            Object newObj = new byte[size];

            long ac = allocationContextOf(TenantContainer.current());
            verbose_cr(", @0x" + Long.toHexString(wb.getObjectAddress(newObj)) + ", tenant["
                    + (ac == 0l ? "ROOT" : "0x" + Long.toHexString(ac)) + "]");
            return newObj;
        }
        return null;
    }

    private static AllocType lastMixedAllocType = AllocType.AT_TINY;
    private static Object allocateObject(AllocType allocType) {
        // adjust allocation type
        if (allocType == AllocType.AT_MIXED) {
            allocType = lastMixedAllocType;
            if (lastMixedAllocType == AllocType.AT_HUMONGOUS) {
                lastMixedAllocType = AllocType.AT_TINY;
            } else if (lastMixedAllocType == AllocType.AT_TINY) {
                lastMixedAllocType = AllocType.AT_HUMONGOUS;
            }
        }

        // do allocation
        switch (allocType) {
            case AT_HUMONGOUS: return allocateObjectOfSize(HEAP_REGION_SIZE << 2);
            case AT_TINY: return allocateObjectOfSize(1);
        }
        return null;
    }

    // retrieve allocationContext field from a TenantContainer object via reflection
    private static long allocationContextOf(TenantContainer tenant) {
        if (tenant != null && allocationContextField != null) {
            try {
                return (Long)allocationContextField.get(tenant);
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            }
        }
        return 0L;
    }

    private static String allocationContextStringOf(TenantContainer tenant) {
        if (tenant != null && allocationContextField != null) {
            try {
                return "0x" + Long.toHexString((Long)allocationContextField.get(tenant));
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            }
        }
        return "NULL";
    }

    private static void verbose(String msg) {
        if (verbose) {
            System.out.print(msg);
        }
    }
    private static void verbose_cr(String msg) {
        verbose(msg + "\n");
    }

    // Assertions
    private static void assertEquals(TenantContainer c1, TenantContainer c2) {
        if (c1 != c2) {
            String msg = "TenantContainer equals failed: c1=" +
                    (c1 == null ? "NULL" : "0x" + Long.toHexString(allocationContextOf(c1)))
                    + ", c2=" +
                    (c2 == null ? "NULL" : "0x" + Long.toHexString(allocationContextOf(c2)));
            throw new RuntimeException(msg);
        }
    }

    private static void assertInSameContainer(Object o1, Object o2) {
        TenantContainer t1, t2;
        t1 = TenantContainer.containerOf(o1);
        t2 = TenantContainer.containerOf(o2);
        if (t1 != t2) {
            String msg = "o1@0x" + Long.toHexString(wb.getObjectAddress(o1)) + " from t1(" +
                    (t1 == null ? "NULL" : "0x" + Long.toHexString(allocationContextOf(t1)))
                    + "), o2@0x" + Long.toHexString(wb.getObjectAddress(o2)) + "from t2(" +
                    (t2 == null ? "NULL" : "0x" + Long.toHexString(allocationContextOf(t2))) + ")";
            throw new RuntimeException(msg);
        }
    }

    private static void assertEquals(long expected, long actual) {
        if (expected != actual) {
            throw new RuntimeException("assertEquals failed: expected = " + expected + ", actual = " + actual);
        }
    }
}
