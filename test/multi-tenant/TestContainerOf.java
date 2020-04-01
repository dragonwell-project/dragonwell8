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
 * @summary Test TenantContainer.containOf() to retrieve tenant container of a Java object
 * @library /testlibrary
 * @build TestContainerOf
 * @run main/othervm -XX:+MultiTenant -XX:+TenantHeapIsolation -XX:+UseG1GC -Xmx1024M -Xms512M
 *                   -XX:G1HeapRegionSize=1M TestContainerOf
 */

import static com.oracle.java.testlibrary.Asserts.*;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import java.util.Arrays;

public class TestContainerOf {

    public static void main(String[] args) throws Exception {
        new TestContainerOf().runAllTests();
    }

    void runAllTests() throws Exception {
        testTenantContainerOf(1);
        testTenantContainerOf(10);
        testTenantContainerOf(80);
        testRunInRootTenant();
    }

    // test TenantContainer.containerOf()
    private void testTenantContainerOf(int count) throws Exception {
        System.out.println(">> Begin TEST testTenantContainerOf: count=" + count);

        Object[] objects = new Object[count];
        TenantContainer[] tenants = new TenantContainer[count];
        Object objInRoot = new Object();
        TenantConfiguration config = new TenantConfiguration().limitHeap(32 * 1024 * 1024 /* 32 MB heap */);

        assertNull(TenantContainer.containerOf(objInRoot));

        for (int i = 0; i < count; ++i) {
            tenants[i]= TenantContainer.create(config);
            final int idx = i;
            final TenantContainer thisContainer = tenants[i];
            thisContainer.run(() -> {
                objects[idx] = new Object();

                TenantContainer current = TenantContainer.current();

                assertNotNull(current);
                assertTrue(current == thisContainer);
                assertNotNull(TenantContainer.containerOf(objects[idx]));
            });
        }

        for (int i = 0; i < count; ++i) {
            TenantContainer containerGet = TenantContainer.containerOf(objects[i]);
            assertNotNull(containerGet);
            long idGet = containerGet.getTenantId();
            long idCur = tenants[i].getTenantId();
            assertEquals(idGet, idCur);
            // assertTrue(containerGet.getTenantId() == tenants[i].getTenantId());
            assertTrue(tenants[i] == containerGet);
        }

        Arrays.stream(tenants).forEach(t -> t.destroy());

        Arrays.stream(objects).forEach(
                obj -> assertNull(TenantContainer.containerOf(obj), "Should be owned by ROOT tenant"));

        System.out.println("<<End TEST testTenantContainerOf\n");
    }

    public void testRunInRootTenant() throws TenantException {
        TenantConfiguration tconfig = new TenantConfiguration().limitHeap(100 * 1024 * 1024);
        final TenantContainer tenant = TenantContainer.create(tconfig);
        tenant.run(() -> {
            assertTrue(TenantContainer.current() == tenant);
            Object obj = TenantContainer.primitiveRunInRoot(()->{
                //should be in root tenant.
                assertTrue(TenantContainer.current() == null);
                return new Object();
            });
            //obj should be allocated in root tenant.
            assertTrue(TenantContainer.containerOf(obj) == null);
        });
    }
}
