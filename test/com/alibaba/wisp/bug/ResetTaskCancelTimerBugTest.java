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
 * @summary test reset task doesn't cancel the current task's timer unexpectedly.
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true ResetTaskCancelTimerBugTest
 */



import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertTrue;


public class ResetTaskCancelTimerBugTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<WispEngine> executor = new AtomicReference<>();
        AtomicBoolean submitDone = new AtomicBoolean();
        CountDownLatch latch = new CountDownLatch(1);

        new Thread(() -> {
            executor.set(WispEngine.current());
            while (!submitDone.get()) {}
            // 4. go sleep, cause there's a pending external WispTask create operation,
            // we should create the task fist.
            // WispCarrier.runTaskInternal() -> WispTask.reset() -> WispCarrier.cancelTimer()
            // will remove current task's timer, the sleep() could never be wakened.
            try {
                Thread.sleep(500);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            latch.countDown();
        }).start();
        while (executor.get() == null) {}

        // 1. create a thread(also created WispCarrier A)
        executor.get().execute(() -> {});
        // 2. request create WispTask in the WispCarrier A
        submitDone.set(true);
        // 3. telling WispCarrier A's it's time to sleep

        // 5. wait sleep done. If latch.countDown() is not invoked inner 10 seconds, TEST FAILURE.
        assertTrue(latch.await(10, TimeUnit.SECONDS));
    }
}
