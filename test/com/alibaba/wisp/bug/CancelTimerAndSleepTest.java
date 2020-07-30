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
 * @summary Test sleep
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true CancelTimerAndSleepTest
 */

import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.util.concurrent.TimeUnit;

public class CancelTimerAndSleepTest {

    public static void main(String[] args) throws Exception {
        WispEngineAccess access = SharedSecrets.getWispEngineAccess();

        access.addTimer(System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(10));
        access.cancelTimer();

        long now = System.currentTimeMillis();
        Thread.sleep(200);
        long elapsed = System.currentTimeMillis() - now;
        if (Math.abs(elapsed - 200) > 10)
            throw new Error("elapsed = " + elapsed);
    }
}
