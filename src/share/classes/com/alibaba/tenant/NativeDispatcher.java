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
    }
}
