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

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceContainerFactory;
import com.alibaba.rcm.internal.AbstractResourceContainer;
import java.util.Collections;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import static com.alibaba.rcm.ResourceType.*;
import static jdk.testlibrary.Asserts.*;

/*
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @library /lib/testlibrary
 * @summary test RCM API based TenantContainerFactory
 * @run main/othervm/bootclasspath -XX:+MultiTenant com.alibaba.tenant.TestTenantContainerFactory
 */
public class TestTenantContainerFactory {

    private void testSingleton() {
        ResourceContainerFactory factory = TenantContainerFactory.instance();
        assertTrue(factory == TenantContainerFactory.instance());
        ResourceContainerFactory factory2 = TenantContainerFactory.instance();
        assertSame(factory, factory2);
    }

    private void testCreation() {
        try {
            ResourceContainer container = TenantContainerFactory.instance()
                    .createContainer(Collections.emptySet());
            assertNotNull(container);
            assertTrue(container instanceof TenantResourceContainer);
            assertNull(TenantContainer.current());
            assertNotNull(TenantResourceContainer.root());
        } catch (Throwable t) {
            fail();
        }
    }

    private void testCreationWithHeapLimit() {
        try {
            Iterable<Constraint> constraints = Stream.of(HEAP_RETAINED.newConstraint(32 * 1024 * 1024))
                    .collect(Collectors.toSet());
            ResourceContainer container = TenantContainerFactory.instance()
                    .createContainer(constraints);
            assertNotNull(container);
            assertTrue(container instanceof TenantResourceContainer);
            assertNull(TenantContainer.current());
            assertNotNull(TenantResourceContainer.root());
        } catch (Throwable t) {
            fail();
        }
    }

    private void testDestroy() {
        ResourceContainer container = TenantContainerFactory.instance()
                .createContainer(null);
        try {
            container.destroy();
            fail("Should throw UnsupportedException");
        } catch (UnsupportedOperationException e) {
            // expected
        }
        try {
            TenantContainer tenant = TenantContainerFactory.tenantContainerOf(container);
            tenant.destroy();
        } catch (Throwable t) {
            fail();
        }
    }

    private void testImplicitTenantContainer() {
        try {
            Iterable<Constraint> constraints = Stream.of(CPU_PERCENT.newConstraint(10))
                    .collect(Collectors.toList());
            ResourceContainer rc = TenantContainerFactory.instance().createContainer(constraints);
            TenantContainer tenant = TenantContainerFactory.tenantContainerOf(rc);
            assertNotNull(tenant);
            assertNull(TenantContainer.current());
            assertSame(AbstractResourceContainer.root(), AbstractResourceContainer.current());
            tenant.run(()->{
                System.out.println("Hello");
                assertSame(TenantContainer.current(), tenant);
                assertSame(AbstractResourceContainer.current(), rc);
            });
            tenant.destroy();
        } catch (Throwable e) {
            fail();
        }
    }

    public static void main(String[] args) {
        TestTenantContainerFactory test = new TestTenantContainerFactory();
        test.testSingleton();
        test.testCreation();
        test.testDestroy();
        test.testImplicitTenantContainer();
        if (TenantGlobals.isHeapThrottlingEnabled()) {
            test.testCreationWithHeapLimit();
        }
    }
}
