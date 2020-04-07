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

/* @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary test TenantConfiguration facilities
 * @library /lib/testlibrary
 * @run main/othervm/bootclasspath -XX:+MultiTenant -XX:+UseG1GC -XX:+TenantCpuThrottling
 *                                 com.alibaba.tenant.TestTenantConfiguration
 */

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceType;
import jdk.testlibrary.TestUtils;
import java.util.LinkedList;
import java.util.List;
import static jdk.testlibrary.Asserts.*;

public class TestTenantConfiguration {

    private void testBasic() {
        try {
            TenantConfiguration config = new TenantConfiguration()
                    .limitCpuCfs(1_000_000, 500_000)
                    .limitCpuShares(1024)
                    .limitHeap(64 * 1024 * 1024);
            assertNotNull(config);
            TenantContainer tenant = TenantContainer.create(config);
            assertNotNull(tenant);
            tenant.destroy();
        } catch (Throwable e) {
            e.printStackTrace();
            fail();
        }
    }

    private void testIllegalLimits() {
        Runnable illegalActions[] = {
                ()->{ new TenantConfiguration().limitCpuSet(""); },
                ()->{ new TenantConfiguration().limitCpuSet(null); },
                ()->{ new TenantConfiguration().limitCpuShares(-128); },
                ()->{ new TenantConfiguration().limitHeap(-64 * 1024 * 1024); },
                ()->{ new TenantConfiguration().limitCpuCfs(-123, 10_000_000); },
                ()->{ new TenantConfiguration().limitCpuCfs(1_000_000, -2); },
                ()->{ new TenantConfiguration().limitCpuCfs(999, -1); }, // lower bound of period
                ()->{ new TenantConfiguration().limitCpuCfs(1_000_001, -1); }, // upper bound of period
                ()->{ new TenantConfiguration().limitCpuCfs(10_000, 999); }, // lower bound of quota
                ()->{ new TenantConfiguration(-1024 * 1024 * 1024); },
                ()->{ new TenantConfiguration(-12, 64 * 1024 * 1024); },
        };
        for (Runnable action : illegalActions) {
            try {
                action.run();
                fail("should throw IllegalArgumentException");
            } catch (IllegalArgumentException e) {
                // expected
            } catch (Throwable e) {
                e.printStackTrace();
                fail("Should not throw any other exceptions");
            }
        }

    }

    private void testValidLimits() {
        Runnable validActions[] = {
                ()->{ new TenantConfiguration().limitCpuCfs(1_000_000, 2_000_000); },
                ()->{ new TenantConfiguration().limitCpuCfs(100_000, 10_000); },
                ()->{ new TenantConfiguration().limitCpuCfs(100_000, -1); }
        };
        for (Runnable action : validActions) {
            try {
                action.run();
            } catch (Throwable e) {
                e.printStackTrace();
                fail("Should not throw any exceptions");
            }
        }
    }

    private void testGetSetLimits() {
        TenantConfiguration config = new TenantConfiguration();
        assertNotNull(config.getAllConstraints());
        assertTrue(config.getAllConstraints().isEmpty());

        long maxHeap = 64 * 1024 * 1024;

        config.limitHeap(maxHeap);
        List<Constraint> constraints = new LinkedList<>();
        config.getAllConstraints().forEach(c -> constraints.add(c));

        assertNotNull(config.getAllConstraints());
        assertTrue(config.getAllConstraints().size() == 1);
        Constraint constraint = constraints.get(0);
        assertEquals(constraint.getResourceType(), ResourceType.HEAP_RETAINED);
        assertTrue(constraint.getResourceType() instanceof ResourceType);
        assertEquals(constraint.getValues()[0], maxHeap);
        assertEquals(constraints.size(), 1);

        config.limitCpuShares(1024);
        constraints.clear();
        config.getAllConstraints().forEach(c -> constraints.add(c));
        assertTrue(constraints.stream().anyMatch(c -> c.getResourceType() == TenantResourceType.CPU_SHARES));
        int nofCPUShares = (int) constraints.stream().filter(c -> c.getResourceType() == TenantResourceType.CPU_SHARES)
                .peek(c -> assertEquals(c.getValues()[0], 1024L))
                .count();
        assertEquals(nofCPUShares, 1);
    }

    private void testUniqueConfigWhenCreatingContainer() {
        TenantConfiguration config = new TenantConfiguration().limitCpuShares(1024).limitHeap(64 * 1024 * 1024);
        TenantContainer tenant = TenantContainer.create(config);
        assertTrue(tenant.getConfiguration() == config);
    }

    public static void main(String[] args) {
        TestUtils.runWithPrefix("test",
                TestTenantConfiguration.class,
                new TestTenantConfiguration());
    }
}
