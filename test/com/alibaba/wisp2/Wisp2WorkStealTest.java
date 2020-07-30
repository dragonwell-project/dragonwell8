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
 * @summary verification of work stealing really happened
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.schedule.stealRetry=100 -Dcom.alibaba.wisp.schedule.helpStealRetry=100 Wisp2WorkStealTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertNE;

public class Wisp2WorkStealTest {
    public static void main(String[] args) {
        Object lock = new Object();
        AtomicReference<Thread> t = new AtomicReference<>();
        AtomicReference<Thread> t2 = new AtomicReference<>();
        WispEngine.dispatch(() -> {
            t.set(SharedSecrets.getJavaLangAccess().currentThread0());
            synchronized (lock) {
                try {
                    lock.wait();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            t2.set(SharedSecrets.getJavaLangAccess().currentThread0());
        });

        // letting a coroutine occupy the carrier
        while (true) {
            AtomicReference<Boolean> found = new AtomicReference<>(null);
            WispEngine.dispatch(() -> {
                if (SharedSecrets.getJavaLangAccess().currentThread0() == t.get()) {
                    found.set(true);
                    long start = System.currentTimeMillis();
                    while (System.currentTimeMillis() - start < 1000) {
                        // occupy the carrier
                    }
                } else {
                    found.set(false);
                }
            });
            while (found.get() == null) {
            }
            if (found.get()) {
                break;
            }
        }

        synchronized (lock) {
            lock.notify();
        }

        while (t2.get() == null) {
        }

        assertNE(t.get(), t2.get());
    }
}
