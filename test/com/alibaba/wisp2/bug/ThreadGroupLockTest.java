/*
 * Copyright (c) 2021 Alibaba Group Holding Limited. All Rights Reserved.
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
 * @summary Verify that the tg lock can be released when the thread exits
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 ThreadGroupLockTest
 */

import java.util.List;
import java.util.concurrent.*;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import static jdk.testlibrary.Asserts.assertTrue;

public class ThreadGroupLockTest {
    private static final int DURATION = 2;
    private static final int CONCURRENT = Math.min(4, Runtime.getRuntime().availableProcessors());

    public static void main(String[] args) throws Exception {
        ExecutorService es = Executors.newCachedThreadPool();
        List<Future<Void>> futures = es.invokeAll(IntStream.range(0, CONCURRENT)
                        .mapToObj(i -> new Task()).collect(Collectors.toList()),
                DURATION + 1, TimeUnit.SECONDS);
        assertTrue(futures.stream().noneMatch(Future::isCancelled));
    }

    static class Task implements Callable<Void> {
        @Override
        public Void call() throws Exception {
            long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(DURATION);
            int n = 0;
            while (System.nanoTime() < deadline) {
                Thread t = new Thread(() -> {/* empty */});
                t.start();
                t.join();
                n++;
            }
            System.out.println(Thread.currentThread().getName() + ": " + n);
            return null;
        }
    }
}
