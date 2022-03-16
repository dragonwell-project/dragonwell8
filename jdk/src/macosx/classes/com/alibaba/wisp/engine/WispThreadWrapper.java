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


import sun.reflect.CallerSensitive;

import java.dyn.CoroutineSupport;

class WispThreadWrapper extends Thread {

    @Override
    public CoroutineSupport getCoroutineSupport() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void start() {
        throw new UnsupportedOperationException();
    }


    @Override
    @Deprecated
    public void destroy() {
        throw new UnsupportedOperationException();
    }

    @Override
    @Deprecated
    public int countStackFrames() {
        throw new UnsupportedOperationException();
    }

    @Override
    @CallerSensitive
    public ClassLoader getContextClassLoader() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setContextClassLoader(ClassLoader cl) {
        throw new UnsupportedOperationException();
    }

    @Override
    public StackTraceElement[] getStackTrace() {
        throw new UnsupportedOperationException();
    }

    @Override
    public long getId() {
        throw new UnsupportedOperationException();
    }

    @Override
    public State getState() {
        throw new UnsupportedOperationException();
    }

    @Override
    public UncaughtExceptionHandler getUncaughtExceptionHandler() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setUncaughtExceptionHandler(UncaughtExceptionHandler eh) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String toString() {
        throw new UnsupportedOperationException();
    }

}
