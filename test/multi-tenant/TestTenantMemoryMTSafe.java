
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
 * @summary TestTenantMemoryMTSafe
 * @library /testlibrary /testlibrary/whitebox
 * @build TestTenantMemoryMTSafe sun.hotspot.WhiteBox
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions
 * -XX:+MultiTenant -XX:+TenantHeapThrottling -XX:+WhiteBoxAPI -XX:+UseG1GC -Xmx1g -Xms1g
 * -XX:G1HeapRegionSize=1M -XX:+PrintGCDetails -XX:+PrintGCDateStamps TestTenantMemoryMTSafe
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

public class TestTenantMemoryMTSafe {

    private static final WhiteBox WB = WhiteBox.getWhiteBox();

    private static final int HEAP_REGION_SIZE = WB.g1RegionSize();

    public static void main(String[] args) throws Exception {
        TenantConfiguration config = new TenantConfiguration().limitHeap(HEAP_REGION_SIZE * 1024);
        TenantContainer tenant = TenantContainer.create(config);

        System.gc();

        try {
            tenant.run(()->{
                try {
                    Object[] array = new Object[3000];
                    for (int i = 0; i < 3000; i++) {
                        array[i] = new byte[100*1024];
                    }
                    WB.fullGC();
                    for (int i = 0; i < 3000; i++) {
                        array[i] = null;
                    }
                    WB.g1StartConcMarkCycle();
                    while (WB.g1InConcurrentMark()) {
                        Thread.sleep(100);
                    }
                    WB.youngGC();
                    WB.youngGC();
                    WB.youngGC();
                    WB.youngGC();
                    WB.youngGC();
                    assertTrue(tenant.getOccupiedMemory() < 1024*1024*5,"Must be less than 5m");
                } catch (Exception e) {
                    System.out.println("Error: " + e.getMessage());
                    fail();
                }
            });
        } catch (TenantException e) {
            fail();
        }
    }

    private static void fail() {
        assertTrue(false, "Failed!");
    }
}
