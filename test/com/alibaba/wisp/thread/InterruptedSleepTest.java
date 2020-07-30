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
 * @summary test InterruptedException was thrown by sleep()
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true InterruptedSleepTest
 */

import com.alibaba.wisp.engine.WispEngine;

import static jdk.testlibrary.Asserts.assertFalse;
import static jdk.testlibrary.Asserts.assertLessThan;
import static jdk.testlibrary.Asserts.assertTrue;

public class InterruptedSleepTest {
    public static void main(String[] args) {
        Thread mainCoro = Thread.currentThread();
        WispEngine.dispatch(() -> {
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
            }
            mainCoro.interrupt();
        });
        long start = System.currentTimeMillis();
        boolean ie = false;
        try {
            Thread.sleep(1000L);
        } catch (InterruptedException e) {
            ie = true;
        }
        assertLessThan((int) (System.currentTimeMillis() - start), 1000);
        assertTrue(ie);
        assertFalse(mainCoro.isInterrupted());
    }
}
