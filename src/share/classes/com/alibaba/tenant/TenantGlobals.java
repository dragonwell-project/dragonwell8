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

/**
 * This class defines the constants used by multi-tenant JDK
 */
public class TenantGlobals {
    /**
     * Retrieves the flags used by multi-tenant module.
     * @return the flags
     */
    private native static int getTenantFlags();

    private final static int flags = getTenantFlags();

    /**** Be careful: the following bit definitions must be consistent with
     **** the ones defined in prims/tenantenv.cpp **/

    /**
     * Bit to indicate that if the multi-tenant feature is enabled.
     */
    public static final int TENANT_FLAG_MULTI_TENANT_ENABLED      = 0x1;

    /**
     * Bit to indicate that if heap throttling feature is enabled
     */
    public static final int TENANT_FLAG_HEAP_THROTTLING_ENABLED   = 0x2;

    /**
     * Bit to indicate that if cpu throttling feature is enabled
     */
    public static final int TENANT_FLAG_CPU_THROTTLING_ENABLED    = 0x4;

    /**
     * Bit to indicate that if cpu accounting feature is enabled
     */
    public static final int TENANT_FLAG_CPU_ACCOUNTING_ENABLED    = 0x40;

    /**
     * Bit to indicate that if heap isolation feature is enabled
     */
    public static final int TENANT_FLAG_HEAP_ISOLATION_ENABLED    = 0x80;

    private TenantGlobals() { }

    /**
     * Test if multi-tenant feature is enabled.
     * @return true if enabled otherwise false
     */
    public static boolean isTenantEnabled() {
       return 0 != (flags & TENANT_FLAG_MULTI_TENANT_ENABLED);
    }

    /**
     * Test if heap throttling feature is enabled.
     * @return true if enabled otherwise false
     */
    public static boolean isHeapThrottlingEnabled() {
        return 0 != (flags & TENANT_FLAG_HEAP_THROTTLING_ENABLED);
    }

    /**
     * Test if heap isolation feature is enabled.
     * @return true if enabled otherwise false
     */
    public static boolean isHeapIsolationEnabled() {
        return 0 != (flags & TENANT_FLAG_HEAP_ISOLATION_ENABLED);
    }


    /**
     * Test if cpu throttling feature is enabled.
     * @return true if enabled otherwise false
     */
    public static boolean isCpuThrottlingEnabled() {
        return 0 != (flags & TENANT_FLAG_CPU_THROTTLING_ENABLED);
    }

    /**
     * Test if cpu accounting feature is enabled.
     * @return true if enabled otherwise false
     */
    public static boolean isCpuAccountingEnabled() {
        return 0 != (flags & TENANT_FLAG_CPU_ACCOUNTING_ENABLED);
    }
}
