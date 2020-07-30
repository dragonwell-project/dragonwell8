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
 * @summary test long running or blocking syscall task could be retaken
 * @requires os.family == "linux"
 * @run main/othervm -Dcom.alibaba.wisp.carrierEngines=1 -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.handoffPolicy=ADAPTIVE HandOffWithStealTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import static jdk.testlibrary.Asserts.assertTrue;

public class HandOffWithStealTest {
    public static void main(String[] args) throws Exception {
        final int N = 100;
        CountDownLatch cl = new CountDownLatch(N);
        for (int i = 0; i < N; i++) {
            WispEngine.dispatch(() -> {
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                cl.countDown();
            });
        }

        AtomicBoolean blockingFinish = new AtomicBoolean(false);

        WispEngine.dispatch(() -> {
            try {
                String[] cmdA = { "/bin/sh", "-c", " sleep 200"};
                Process process = Runtime.getRuntime().exec(cmdA);
                process.waitFor();
                blockingFinish.set(true);
            } catch (Exception e) {
                e.printStackTrace();
            }
        });

        assertTrue(cl.await(3, TimeUnit.SECONDS) && !blockingFinish.get());
    }
}
