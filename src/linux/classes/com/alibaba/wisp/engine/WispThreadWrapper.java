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

import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;
import sun.reflect.CallerSensitive;

import java.dyn.CoroutineSupport;

/**
 * An wrapper of {@link Thread} let every {@link WispTask} get different thread
 * object from {@link Thread#currentThread()}.
 * In this way, we make the listed class (not only but include) behave expected without
 * changing their code.
 * <p>
 * 1. {@link ThreadLocal}
 * 2. {@link java.util.concurrent.locks.AbstractQueuedSynchronizer} based synchronizer
 * 3. Netty's judgment of weather we are running in it's worker thread.
 */
class WispThreadWrapper extends Thread {

    WispThreadWrapper(WispTask task) {
        JLA.setWispTask(this, task);
        setName(task.getName());
    }

    @Override
    public void start() {
        throw new UnsupportedOperationException();
    }

    @Override
    @Deprecated
    public void destroy() {
    }

    @Override
    @Deprecated
    public int countStackFrames() {
        return JLA.getWispTask(this).carrier.thread.countStackFrames();
    }

    @Override
    @CallerSensitive
    public ClassLoader getContextClassLoader() {
        return JLA.getWispTask(this).ctxClassLoader;
    }

    @Override
    public void setContextClassLoader(ClassLoader cl) {
        JLA.getWispTask(this).ctxClassLoader = cl;
    }

    @Override
    public StackTraceElement[] getStackTrace() {
        return super.getStackTrace();
    }

    @Override
    public long getId() {
        return super.getId();
    }

    @Override
    public State getState() {
        return JLA.getWispTask(this).carrier.thread.getState();
    }

    @Override
    public UncaughtExceptionHandler getUncaughtExceptionHandler() {
        return JLA.getWispTask(this).carrier.thread.getUncaughtExceptionHandler();
    }

    @Override
    public void setUncaughtExceptionHandler(UncaughtExceptionHandler eh) {
        JLA.getWispTask(this).carrier.thread.setUncaughtExceptionHandler(eh);
    }

    @Override
    public String toString() {
        return "WispThreadWrapper{" +
                "wispTask=" + JLA.getWispTask(this) +
                '}';
    }

    private static final JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();

}
