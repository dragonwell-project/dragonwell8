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
 * @summary test sleep
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true SleepTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.highPrecisionTimer=true SleepTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;

import static jdk.testlibrary.Asserts.assertTrue;

public class SleepTest {
    public static void main(String[] args) {
        assertTrue(IntStream.range(0, 100).parallel().allMatch(ms -> {
            long start = System.currentTimeMillis();
            FutureTask<Integer> ft = new FutureTask<>(() -> {
                Thread.sleep(ms);
                return 0;
            });
            WispEngine.dispatch(ft);
            try {
                ft.get(200, TimeUnit.MILLISECONDS);
            } catch (Throwable e) {
                throw new Error(e);
            }

            long interval = System.currentTimeMillis() - start;

            if (Math.abs(interval - ms) < 100) {
                return true;
            } else {
                System.out.println("ms = " + ms);
                System.out.println("interval = " + interval);
                return false;
            }
        }));
    }
}
