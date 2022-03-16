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
 * @summary Test timer implement
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine TimerTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.highPrecisionTimer=true TimerTest
 */


import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;


/**
 * test the time out implement
 */
public class TimerTest {
    public static void main(String... arg) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        WispEngine.dispatch(() -> {
            long ts = System.currentTimeMillis();
            for (int i = 0; i < 10; i++) {
                SharedSecrets.getWispEngineAccess().sleep(100);
                long now = System.currentTimeMillis();
                if (Math.abs(now - ts - 100) > 10)
                    throw new Error();
                ts = now;
            }
            latch.countDown();
        });
        latch.await(5, TimeUnit.SECONDS);
    }
}
