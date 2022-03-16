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
 * @summary Test a WispTask will not block when it created a new one and didn't yield to its parent.
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2  RemoveWispParentTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.*;
import java.util.concurrent.*;

import static jdk.testlibrary.Asserts.assertTrue;

public class RemoveWispParentTest {
    static CountDownLatch latch = new CountDownLatch(3);

    // mainThreadCreateWisp create a new wisp task in main thread, check whether
    // main thread will be blocked.
    static void mainThreadCreateWisp() {
        WispEngine.dispatch(() -> {
            latch.countDown();
        });
    }

    // wispTaskCreateWisp create a new wisp task in wispTask, check whether
    // creater wispTask will be blocked.
    static void wispTaskCreateWisp() {
        WispEngine.dispatch(() -> {
            new Thread(() -> {
                latch.countDown();
            }).start();
            latch.countDown();
        });
    }

    public static void main(String[] args) throws Exception {
        mainThreadCreateWisp();
        wispTaskCreateWisp();
        assertTrue(latch.await(1, TimeUnit.SECONDS));
    }
}