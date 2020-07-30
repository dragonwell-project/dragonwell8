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
 * @summary Test ReentrantLock in coroutine environment
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine  -Dcom.alibaba.wisp.transparentWispSwitch=true LockTest
 */


import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class LockTest {
    static Lock lock = new ReentrantLock();
    static Condition cond = lock.newCondition();

    static CountDownLatch latch = new CountDownLatch(2);

    public static void main(String[] args) throws Exception {
        WispEngine.dispatch(LockTest::p1);
        WispEngine.dispatch(LockTest::p2);

        latch.await(2, TimeUnit.SECONDS);
    }

    static void assertInterval(long start, int diff, int bias) {
        if (Math.abs(System.currentTimeMillis() - start - diff) > bias)
            throw new Error("not wakeup expected");
    }

    private static void p1() {
        lock.lock();
        SharedSecrets.getWispEngineAccess().sleep(100);
        try {
            long start = System.currentTimeMillis();
            cond.await();
            assertInterval(start, 100, 5);

        } catch (InterruptedException e) {
            throw new Error();
        } finally {
            lock.unlock();
        }
        latch.countDown();
    }

    private static void p2() {
        long start = System.currentTimeMillis();
        lock.lock();
        try {
            assertInterval(start, 100, 5);
            SharedSecrets.getWispEngineAccess().sleep(100);
            cond.signal();
        } finally {
            lock.unlock();
        }
        latch.countDown();
    }


}
