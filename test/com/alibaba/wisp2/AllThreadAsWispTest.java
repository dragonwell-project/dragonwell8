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
 * @summary convert all thread to wisp
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine  -Dcom.alibaba.wisp.transparentWispSwitch=true  -Dcom.alibaba.wisp.version=2 -XX:+UseWispMonitor -Dcom.alibaba.wisp.enableThreadAsWisp=true -Dcom.alibaba.wisp.allThreadAsWisp=true AllThreadAsWispTest
 */

import sun.misc.SharedSecrets;

import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;

public class AllThreadAsWispTest {
    public static void main(String[] args) throws Exception {
        CountDownLatch done = new CountDownLatch(60);

        Runnable r = () -> {
            if (SharedSecrets.getJavaLangAccess().currentThread0() != Thread.currentThread()) {
                done.countDown();
            }
        };

        for (int i = 0; i < 10; i++) {
            Executors.newSingleThreadExecutor().submit(r);
            Executors.newWorkStealingPool().submit(r);
            Executors.newScheduledThreadPool(100).submit(r);
            new Thread(r).start();
            new Timer().schedule(new TimerTask() {
                @Override
                public void run() {
                    r.run();
                }
            }, 0);
            new Thread() {
                @Override
                public void run() {
                    r.run();
                }
            }.start();
        }

        assertTrue(done.await(10, TimeUnit.SECONDS));
    }
}
