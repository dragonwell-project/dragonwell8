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
 * @summary test basic coroutine steal mechanism
 * @library /lib/testlibrary
 * @run main/othervm -XX:+EnableCoroutine BasicStealTest
 */

import java.dyn.Coroutine;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;

import static jdk.testlibrary.Asserts.assertEQ;
import static jdk.testlibrary.Asserts.assertTrue;

public class BasicStealTest {
    public static void main(String[] args) {
        AtomicReference<Coroutine> toBeStolen = new AtomicReference<>();
        Thread main = Thread.currentThread();

        Thread t = new Thread(() -> {
            Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();

            Coroutine coro = new Coroutine(() -> {
                AtomicReference<Consumer<Integer>> foo = new AtomicReference<>();
                foo.set((x) -> {
                    if (x == 10) {
                        Coroutine.yieldTo(threadCoro);
                    } else {
                        System.out.println(x + " enter " + Thread.currentThread());
                        foo.get().accept(x + 1);
                        System.out.println(x + " exit" + Thread.currentThread());
                        assertEQ(Thread.currentThread(), main);
                    }
                });
                foo.get().accept(0);
            });

            Coroutine.yieldTo(coro);
            // switch from foo()...
            toBeStolen.set(coro);
            try {
                Thread.sleep(100000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }, "new_thread");
        t.setDaemon(true);
        t.start();

        while (toBeStolen.get() == null) {
        }

        assertEQ(toBeStolen.get().steal(false), Coroutine.StealResult.SUCCESS);
        Coroutine.yieldTo(toBeStolen.get());

    }
}
