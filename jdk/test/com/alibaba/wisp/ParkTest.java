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
 * @summary Test Wisp engine park / unpark
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 ParkTest
 */


import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;

public class ParkTest {
    public static void main(String[] args) throws Exception {
        WispEngineAccess access = SharedSecrets.getWispEngineAccess();

        WispTask[] task = new WispTask[1];

        FutureTask<Boolean> ft = new FutureTask<>(() -> {
            task[0] = access.getCurrentTask();
            long start, diff;

            start = System.currentTimeMillis();
            access.park(0);

            diff = System.currentTimeMillis() - start;
            if (diff < 200 || diff > 220)
                throw new Error("error test unpark by other thread");


            start = start + diff;
            access.park(TimeUnit.MILLISECONDS.toNanos(200));
            diff = System.currentTimeMillis() - start;

            if (diff < 200 || diff > 220)
                throw new Error("error test timed park");


            start = start + diff;
            access.unpark(access.getCurrentTask());
            access.park(0);
            diff = System.currentTimeMillis() - start;
            if (diff > 20)
                throw new Error("error test permitted park");

            return true;
        });

        WispEngine.current().execute(ft);

        Thread unparkThread = new Thread(() -> {
            access.sleep(200);
            access.unpark(task[0]);
        });
        unparkThread.start();

        assertTrue(ft.get(5, TimeUnit.SECONDS));
    }
}
