/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

import java.lang.reflect.Proxy;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Phaser;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * @test
 * @bug 8174729
 * @summary Proxy.getProxyClass() / Proxy.isProxyClass() race detector
 * @run main ProxyRace
 * @author plevart
 */

public class ProxyRace {

    static final int threads = 8;

    static volatile ClassLoader classLoader;
    static volatile boolean terminate;
    static final AtomicInteger racesDetected = new AtomicInteger();

    public static void main(String[] args) throws Exception {

        Phaser phaser = new Phaser(threads) {
            @Override
            protected boolean onAdvance(int phase, int registeredParties) {
                // install new ClassLoader on each advance
                classLoader = new CL();
                return terminate;
            }
        };

        ExecutorService exe = Executors.newFixedThreadPool(threads);

        for (int i = 0; i < threads; i++) {
            exe.execute(() -> {
                while (phaser.arriveAndAwaitAdvance() >= 0) {
                    Class<?> proxyClass = Proxy.getProxyClass(classLoader, Runnable.class);
                    if (!Proxy.isProxyClass(proxyClass)) {
                        racesDetected.incrementAndGet();
                    }
                }
            });
        }

        Thread.sleep(5000L);

        terminate = true;
        exe.shutdown();
        exe.awaitTermination(5L, TimeUnit.SECONDS);

        System.out.println(racesDetected.get() + " races detected");
        if (racesDetected.get() != 0) {
            throw new RuntimeException(racesDetected.get() + " races detected");
        }
    }

    static class CL extends ClassLoader {
        public CL() {
            super(ClassLoader.getSystemClassLoader());
        }
    }
}
