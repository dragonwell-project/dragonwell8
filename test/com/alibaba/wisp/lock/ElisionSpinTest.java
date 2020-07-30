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
 * @summary Test elision spin
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.carrierEngines=1 ElisionSpinTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

import static jdk.testlibrary.Asserts.assertFalse;
import static jdk.testlibrary.Asserts.assertTrue;

public class ElisionSpinTest {
    public static void main(String[] args) {
        assertFalse(SharedSecrets.getWispEngineAccess().hasMoreTasks());

        ReentrantLock lock = new ReentrantLock();
        Condition cond = lock.newCondition();

        WispEngine.dispatch(() -> {
            lock.lock();
            try {
                cond.awaitUninterruptibly();
            } finally {
                lock.unlock();
            }
        });

        lock.lock();
        try {
            cond.signal();
        } finally {
            lock.unlock();
        }

        assertTrue(SharedSecrets.getWispEngineAccess().hasMoreTasks());
    }
}
