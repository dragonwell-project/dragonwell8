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

import org.junit.Test;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantGlobals;
import static org.junit.Assert.*;

/* @test
 * @summary unit tests for com.alibaba.tenant.TenantGlobals
 * @library /lib/testlibrary
 * @compile TestTenantGlobals.java
 * @run junit/othervm  test.com.alibaba.tenant.TestTenantGlobals
 */
public class TestTenantGlobals {

    @Test
    public void testIfTenantIsDisabled() {
        assertFalse(TenantGlobals.isTenantEnabled());
        String value = System.getProperty("com.alibaba.tenant.enableMultiTenant");
        boolean bTenantIsEnabled = false;
        if(value != null && "true".equalsIgnoreCase(value)) {
            bTenantIsEnabled = true;
        }
        assertFalse(bTenantIsEnabled);
        TenantConfiguration tconfig = new TenantConfiguration();
        try {
            TenantContainer.create(tconfig);
            fail(); // should not reach here.
        } catch (UnsupportedOperationException exception) {
            // Expected
            System.out.println("Can not create tenant without -XX:+MultiTenant enabled.");
            exception.printStackTrace();
        }
        try {
            TenantContainer.create(tconfig);
            fail(); // should not reach here.
        } catch (UnsupportedOperationException exception) {
            // Expected
            System.out.println("Can not create tenant without -XX:+MultiTenant enabled.");
            exception.printStackTrace();
        }
        try {
            TenantContainer.current();
            fail(); // should not reach here.
        } catch (UnsupportedOperationException exception) {
            // Expected
            System.out.println("Can not get current tenant without -XX:+MultiTenant enabled.");
            exception.printStackTrace();
        }
    }

    public static void main(String[] args) {
        TestTenantGlobals test = new TestTenantGlobals();
        test.testIfTenantIsDisabled();
    }

}
