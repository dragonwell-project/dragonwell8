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

package com.alibaba.wisp.engine;

class TaskDispatcher implements StealAwareRunnable {
    private final ClassLoader ctxClassLoader;
    private final long enqueueTime;
    private final Runnable target;
    private final String name;
    private final Thread thread;
    private final long stackSize;

    TaskDispatcher(ClassLoader ctxClassLoader, Runnable target, String name, Thread thread, long stackSize) {
        this.ctxClassLoader = ctxClassLoader;
        this.enqueueTime = WispEngine.getNanoTime();
        this.target = target;
        this.name = name;
        this.thread = thread;
        this.stackSize = stackSize;
    }

    @Override
    public void run() {
        WispCarrier current = WispCarrier.current();
        current.countEnqueueTime(enqueueTime);
        current.runTaskInternal(target, name, thread,
                ctxClassLoader == null ? current.current.ctxClassLoader : ctxClassLoader,
                this.stackSize);
    }
}
