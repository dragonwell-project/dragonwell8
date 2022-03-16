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
 * @summary test thread.interrupt() of wispTask
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true InterruptTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.CountDownLatch;

public class InterruptTest {
    public static void main(String[] args) throws Exception {
        Lock lock = new ReentrantLock();
        Condition dummy = lock.newCondition();
        Condition finish = lock.newCondition();
        Condition threadSet = lock.newCondition();
        CountDownLatch finishLatch = new CountDownLatch(1);

        AtomicBoolean interrupted = new AtomicBoolean(false);
        AtomicReference<Thread> thrd = new AtomicReference<>();

        WispEngine.dispatch(() -> {
            thrd.set(Thread.currentThread());
            lock.lock();
            finishLatch.countDown();
            threadSet.signal();
            try {
                try {
                    dummy.await(1, TimeUnit.SECONDS);
                    throw new Error("Exception not happened");
                } catch (InterruptedException e) {
                    interrupted.set(true);
                }
            } finally {
                finish.signal();
                lock.unlock();
            }
        });

        finishLatch.await();
        lock.lock();
        try {
            if (thrd.get() == null && !threadSet.await(1, TimeUnit.SECONDS))
                throw new Error("wait threadSet");
        } finally {
            lock.unlock();
        }

        if (thrd.get() == null)
            throw new Error("thread not set");

        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
        }
        thrd.get().interrupt();

        lock.lock();
        try {
            if (!finish.await(1, TimeUnit.SECONDS))
                throw new Error("wait finish");
        } finally {
            lock.unlock();
        }

        if (!interrupted.get())
            throw new Error("InterruptedException not happened");

    }
}
