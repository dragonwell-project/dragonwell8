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

/*
 * @test
 * @summary Test the fix that unpark might not be handled in WispThread::unpark due to due to WispEngine of main thread not properly been initialized in premain().
 *
 * @run shell premainWithWispMonitorTest.sh
 */

import com.alibaba.wisp.engine.WispEngine;

import java.lang.instrument.Instrumentation;

public class PremainWithWispMonitorTest {

    private static Object lock = new Object();

    public static void main(String[] args) throws Exception {
        // BUG: before fix, WispEngine is not registered to jvm data structure
        Thread t = new Thread(() -> {
            synchronized (lock) { // blocked by current main thread
            }
        });
        synchronized (lock) {
            t.start();
            Thread.sleep(100);
        }
        // unlock, if without fix, the handling for t's unpark will be missed in WispThread::unpark.
        // The reason here is WispEngine instance is not correctly initialized for current main thread
        // in premain phase, which is triggered by JvmtiExport::post_vm_initialized

        t.join(100);
        if (t.isAlive()) {
            throw new Error("lost unpark");
        }
    }

    public static void premain (String agentArgs, Instrumentation instArg) {
        // init WispEngine before initializeCoroutineSupport
        WispEngine.current();
    }
}
