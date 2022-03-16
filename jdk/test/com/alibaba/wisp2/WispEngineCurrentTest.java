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
 * @summary Test WispEngine semantic change after refactor
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2  WispEngineCurrentTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.*;
import java.util.concurrent.*;

import static jdk.testlibrary.Asserts.assertTrue;

public class WispEngineCurrentTest {
    public static void main(String[] args) throws Exception {
        WispEngine engine = WispEngine.createEngine(4, r -> {
            Thread t = new Thread(r);
            t.setDaemon(true);
            return t;
        });
        Set<WispEngine> wispEngineSet = Collections.newSetFromMap(new ConcurrentHashMap<>());
        CountDownLatch latch0 = new CountDownLatch(10);
        for (int i = 0; i < 10; i++) {
            engine.execute(() -> {
                wispEngineSet.add(WispEngine.current());
                latch0.countDown();
            });
        }

        assertTrue(latch0.await(1, TimeUnit.SECONDS));
        assertTrue(wispEngineSet.size() == 1);
        CountDownLatch latch1 = new CountDownLatch(1);
        WispEngine.dispatch(() -> {
            wispEngineSet.add(WispEngine.current());
            latch1.countDown();
        });
        assertTrue(latch1.await(1, TimeUnit.SECONDS));
        assertTrue(wispEngineSet.size() == 2);
    }
}
