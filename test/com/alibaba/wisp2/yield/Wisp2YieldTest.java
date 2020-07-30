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
 * @library /lib/testlibrary
 * @summary Test yield in wisp2
 * @requires os.family == "linux"
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 -Dcom.alibaba.wisp.workerEngines=1 Wisp2YieldTest
 */

import com.alibaba.wisp.engine.WispEngine;

import static jdk.testlibrary.Asserts.assertTrue;


public class Wisp2YieldTest {
    public static void main(String[] args) {
        boolean success[] = new boolean[1];

        WispEngine.dispatch(() -> {
            WispEngine.dispatch(() -> {
                success[0] = true;
            });
            long start = System.currentTimeMillis();
            while (System.currentTimeMillis() - start < 100) {
            }
            Thread.yield();
            while (true) {

            }
        });

        sleep(1000);

        assertTrue(success[0]);
    }

    private static void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}
