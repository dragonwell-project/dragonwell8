
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
 * @summary Test heap limit on tenant
 * @library /testlibrary /testlibrary/whitebox
 * @build TestTenantHeapLimit
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+MultiTenant -XX:+TenantHeapThrottling -XX:+WhiteBoxAPI -XX:+UseG1GC -Xmx1024M -Xms512M -XX:G1HeapRegionSize=1M TestTenantHeapLimit
 */

import static com.oracle.java.testlibrary.Asserts.*;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.Platform;
import com.oracle.java.testlibrary.ProcessTools;
import sun.hotspot.WhiteBox;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class TestTenantHeapLimit {

    private static final WhiteBox wb = WhiteBox.getWhiteBox();

    private static final int HEAP_REGION_SIZE = wb.g1RegionSize();

    // convenient sizing constants
    private static final int K = 1024;
    private static final int M = K * 1024;
    private static final int G = M * 1024;
    // object header size
    private static final int objHeaderSize = Platform.is64bit() ? 16 : 8;

    public static void main(String[] args) throws Exception {
        testExpectedOOM(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 9));
        testExpectedOOM(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 7));
        testExpectedOOM(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 4));
        testExpectedOOM(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 1));

        testThrowAwayGarbage(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 9), 5000L /* ms */);
        testThrowAwayGarbage(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 7), 5000L /* ms */);
        testThrowAwayGarbage(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 4), 5000L /* ms */);
        testThrowAwayGarbage(new TenantConfiguration().limitHeap(HEAP_REGION_SIZE << 1), 5000L /* ms */);

        testSmallTenantHeapLimit();
        testHumongousLimit();
    }

    private static void testHumongousLimit() {
        TenantConfiguration config = new TenantConfiguration().limitHeap(HEAP_REGION_SIZE * 100);
        TenantContainer tenant = TenantContainer.create(config);

        System.gc();

        try {
            tenant.run(()->{
                try {
                    // create a humongous object whose size > tenant heap limit
                    Object obj = new byte[(int) (config.getMaxHeap() * 2)];
                    // should throw OOM
                    fail();
                } catch (OutOfMemoryError oom) {
                    // expected!
                } catch (Throwable e) {
                    System.err.println("Unexpected exception!");
                    e.printStackTrace();
                    fail();
                }
            });
        } catch (TenantException e) {
            fail();
        }
    }

    //
    // create a tenant which has heap limit far less than current young gen size,
    // after allocating object across tenant limit, YGC should be triggered.
    //
    private static void testSmallTenantHeapLimit() {
        OutputAnalyzer output = null;
        try {
            ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                    "-XX:+UseG1GC",
                    "-Xmx1024m",
                    "-Xms1024m",
                    "-XX:G1HeapRegionSize=1M",
                    "-XX:+PrintGCDetails",
                    "-Xbootclasspath/a:.",
                    "-XX:+UnlockDiagnosticVMOptions",
                    "-XX:+WhiteBoxAPI",
                    "-XX:+PrintGC",
                    "-XX:+MultiTenant",
                    "-XX:+TenantHeapThrottling",
                    "-XX:+UnlockDiagnosticVMOptions",
                    "-XX:+UnlockExperimentalVMOptions",
                    "-XX:G1NewSizePercent=50", /* young gen: 512MB ~= 1024MB * 50% */
                    HeapConsumer.class.getName());
            output = ProcessTools.executeProcess(pb);

            // expect normal exit, no OOM error
            assertEquals(output.getExitValue(), 0);
            // should not have full GC
            assertFalse(output.getStdout().contains("[Full GC (Allocation Failure)"));
            // must have triggerred YGC
            assertTrue(output.getStdout().contains("[GC pause (G1 Evacuation Pause) (young)"));
        } catch (Exception e) {
            e.printStackTrace();
            fail();
        } finally {
            if (output != null) {
                System.out.println("STDOUT of child process:");
                System.out.println(output.getStdout());
                System.out.println("STDOUT END");
                System.err.println("STDERR of child process:");
                System.err.println(output.getStderr());
                System.err.println("STDERR END");
            }
        }
    }

    public static class HeapConsumer {
        public static void main(String[] args) {
            // tenant heap limit is 40M, much smaller than young gen capacity 512M.
            // so GC should be triggered right after reaching tenant heap limit.
            TenantConfiguration config = new TenantConfiguration().limitHeap(40 * M);
            TenantContainer tenant = TenantContainer.create(config);
            try {
                tenant.run(()-> {
                    int allocationGrain = 64; /*bytes per allocation*/
                    long unitSize = allocationGrain + objHeaderSize;
                    // to keep track of total allocated object size
                    long totalAllocatedBytes = 0;
                    // total limit is far more large that tenant limit
                    long allocateLimit = 200 * M;
                    // keep liveReferencePercent of tenant limit as strongly reachable
                    float liveReferencePercent = 0.5f;
                    int slots = (int) (config.getMaxHeap() / unitSize * liveReferencePercent);
                    // strong references to prevent some of the object from being GCed
                    List<Object> holder = new ArrayList<>(slots);
                    int nextIndex = 0;

                    // allocate objects
                    while (totalAllocatedBytes < allocateLimit) {
                        // allocate object
                        byte[] arr = new byte[allocationGrain];

                        // build necessary references
                        totalAllocatedBytes += (arr.length);
                        if (holder.size() < slots) {
                            holder.add(arr);
                        } else {
                            holder.set((nextIndex++) % slots, arr);
                        }
                    }
                    System.out.println("finished allocation! total " + totalAllocatedBytes + " bytes");
                });
            } catch (TenantException e) {
                e.printStackTrace();
                fail();
            } finally {
                tenant.destroy();
            }
        }
    }

    private static void testThrowAwayGarbage(TenantConfiguration config, long millis) {
        System.out.println("Start testThrowAwayGarbage with heapLimit=" + config.getMaxHeap() + "bytes");
        final AtomicBoolean shouldEnd = new AtomicBoolean(false);
        // below well-behaved task never holds memory exceeds rebel heap limit
        Thread conformist = new Thread() {
            @Override
            public void run() {
                TenantContainer slave = TenantContainer.create(config);
                try {
                    slave.run(() -> {
                        while (!shouldEnd.get()) {
                            // allocate and throw away
                            Object o = new byte[4];
                            o = new byte[HEAP_REGION_SIZE >> 2];
                        }
                    });
                } catch (TenantException e) {
                    fail();
                } finally {
                    slave.destroy();
                }
            }
        };
        conformist.start();

        // wait for the conformist to end
        try {
            Thread.sleep(millis);
            shouldEnd.set(true);
            conformist.join();
        } catch (InterruptedException e) {
            fail();
        }
        System.out.println("testThrowAwayGarbage finished normally");
    }

    private static void testExpectedOOM(TenantConfiguration config) {
        // below is a bad tenant task, which will hold memory more that limit,
        // thus should throw OOM
        System.out.println("Start testExpectedOOM with heapLimit=" + config.getMaxHeap() + "bytes");
        TenantContainer rebel = TenantContainer.create(config);
        try {
            rebel.run(() -> {
                long total = 0, limit = (config.getMaxHeap() << 1);
                int unit = (HEAP_REGION_SIZE >> 3);
                List<Object> refs = new ArrayList();

                while (total < limit) {
                    refs.add(new byte[unit]);
                    total += unit;
                }
            });
            fail();
        } catch (TenantException e) {
            fail();
        } catch (OutOfMemoryError oom) {
            // expected!
            System.out.println("Expected OOM!");
        } catch (Throwable t) {
            // for other exception, just error
            fail();
        } finally {
            rebel.destroy();
        }
        System.out.println("testExpectedOOM finished normally");
    }

    private static void fail() {
        assertTrue(false, "Failed!");
    }
}
