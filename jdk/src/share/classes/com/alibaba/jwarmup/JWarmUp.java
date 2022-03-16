/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

package com.alibaba.jwarmup;

public class JWarmUp {

    /* Make sure registerNatives is the first thing <clinit> does. */
    private static native void registerNatives();

    static {
        registerNatives();
    }

    private static boolean hasNotified = false;

    /**
     * Notify jvm that application startup is done.
     * <p>
     * Should be explicitly call after startup of application if
     * CompilationWarmUpOptimistic is off. Otherwise, it does
     * nothing and just prints a warning message.
     *
     * @version 1.8
     */
    public static synchronized void notifyApplicationStartUpIsDone() {
        if (!hasNotified) {
            hasNotified = true;
            notifyApplicationStartUpIsDone0();
        }
    }

    /**
     * Notify jvm to deoptimize warmup methods
     * <p>
     * Should be explicitly call after startup of application 
     * and warmup compilation is completed
     * vm option CompilationWarmUpExplicitDeopt must be on
     * Otherwise, it does nothing and just prints a warning message.
     *
     * @version 1.8
     */
    public static synchronized void notifyJVMDeoptWarmUpMethods() {
        if (hasNotified && checkIfCompilationIsComplete()) {
            notifyJVMDeoptWarmUpMethods0();
        }
    }

    /**
     * Check if the last compilation submitted by JWarmUp is complete.
     * <p>
     * call this method after <code>notifyApplicationStartUpIsDone</code>
     *
     * @return true if the last compilation task is complete.
     *
     * @version 1.8
     */
    public static synchronized boolean checkIfCompilationIsComplete() {
        if (!hasNotified) {
          throw new IllegalStateException("Must call checkIfCompilationIsComplete() after notifyApplicationStartUpIsDone()");
        } else {
          return checkIfCompilationIsComplete0();
        }
    }


    /**
     * dummy function used internal, DO NOT call this
     */
    private void dummy() {
        throw new UnsupportedOperationException("dummy function");
    }

    private native static void notifyApplicationStartUpIsDone0();

    private native static boolean checkIfCompilationIsComplete0();

    private native static void notifyJVMDeoptWarmUpMethods0();
}
