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

import java.security.AccessController;

/**
 * All cgroup operations are in this class
 *
 */
class NativeDispatcher {

    // ------------- CGroup constants and flags -----------
    // names of cgroup controllers
    static final String CG_CPU              = "cpu";
    static final String CG_CPU_SHARES       = "cpu.shares";
    static final String CG_CPU_CFS_QUOTA    = "cpu.cfs_quota_us";
    static final String CG_CPU_CFS_PERIOD   = "cpu.cfs_period_us";
    static final String CG_CPUSET           = "cpuset";
    static final String CG_CPUSET_CPUS      = "cpuset.cpus";
    static final String CG_CPUSET_MEMS      = "cpuset.mems";
    static final String CG_CPUACCT          = "cpuacct";
    static final String CG_TASKS            = "tasks";

    // flags to indicate if target cgroup controllers has been enabled
    // will be initialized by native code
    static boolean IS_CPU_SHARES_ENABLED  = false;
    static boolean IS_CPU_CFS_ENABLED     = false;
    static boolean IS_CPUSET_ENABLED      = false;
    static boolean IS_CPUACCT_ENABLED     = false;

    // mountpoint of each controller, will be initialized by native code
    static String CG_MP_CPU       = null;
    static String CG_MP_CPUSET    = null;
    static String CG_MP_CPUACCT   = null;

    static final boolean CGROUP_INIT_SUCCESS;

    // retrieve process ID of current JVM process
    long getProcessId() {
        String processName = java.lang.management.ManagementFactory.getRuntimeMXBean().getName();
        return Long.parseLong(processName.split("@")[0]);
    }

    // --------- CGroup wrappers --------
    private static native int init0();

    // create new cgroup path
    native int createCgroup(String groupPath);

    // move current thread to cgroup at 'groupPath'
    native int moveToCgroup(String groupPath);

    // Attach the current thread to given {@code tenant}
    native void attach(TenantContainer tenant);

    // Create and initialize native allocation context
    native void createTenantAllocationContext(TenantContainer tenant, long heapLimit);

    // Destroy native allocation context
    // NOTE: cannot be called directly in finalize() otherwise will lead to hang issue
    native void destroyTenantAllocationContext(long allocationContext);

    // Get memory size currently occupied by this allocation context
    native long getTenantOccupiedMemory(long allocationContext);

    // Gets an array containing the amount of memory allocated on the Java heap for a set of threads (in bytes)
    native void getThreadsAllocatedMemory(long[] ids, long[] memSizes);

    // Gets the TenantContainer object whose memory space contains <code>obj</code>, or null if ROOT tenant container
    native TenantContainer containerOf(Object obj);

    private static native void registerNatives0();

    static {
        AccessController.doPrivileged(
            new java.security.PrivilegedAction<Void>() {
                public Void run() {
                    System.loadLibrary("java");
                    return null;
                }
            });
        registerNatives0();

        boolean initSuccess = false;
        if (TenantGlobals.isCpuThrottlingEnabled() || TenantGlobals.isCpuAccountingEnabled()) {
            AccessController.doPrivileged(
                new java.security.PrivilegedAction<Void>() {
                    public Void run() {
                        System.loadLibrary("jgroup");
                        return null;
                    }
                });
            initSuccess = (0 == init0());
            if (!initSuccess) {
                throw new RuntimeException("JGroup native dispatcher initialization failed!");
            }
        }
        CGROUP_INIT_SUCCESS = initSuccess;
    }
}
