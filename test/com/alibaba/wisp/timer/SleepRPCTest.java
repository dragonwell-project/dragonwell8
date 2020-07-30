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
 * @summary test use sleep in RPC senorina
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true SleepRPCTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.highPrecisionTimer=true  SleepRPCTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.List;
import java.util.Random;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import static jdk.testlibrary.Asserts.assertTrue;

public class SleepRPCTest {
    public static void main(String[] args) {

        List<Future<Boolean>> rpcs = IntStream.range(0, 50).mapToObj(i -> {
            AtomicBoolean done = new AtomicBoolean(false);
            FutureTask<Boolean> ft = new FutureTask<>(() -> {
                for (int n = 0; !done.get() && n < 40; n++) {
                    Thread.sleep(5);
                } // 200 ms timeout
                return done.get();
            });
            WispEngine.dispatch(ft);
            WispEngine.dispatch(new FutureTask<>(() -> {
                Thread.sleep(new Random().nextInt(100) + 1);
                done.set(true);
                return 0;
            }));

            return ft;
        }).collect(Collectors.toList());

        assertTrue(rpcs.stream().allMatch(ft -> {
            try {
                return ft.get(1, TimeUnit.SECONDS);
            } catch (Throwable e) {
                return false;
            }
        }));
    }
}
