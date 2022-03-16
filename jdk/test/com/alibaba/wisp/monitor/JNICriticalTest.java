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
 * @summary Test unpark in JNI critical case
 * @requires os.family == "linux"
 * @run main/othervm   -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true JNICriticalTest
 */


import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.stream.IntStream;
import java.util.zip.Deflater;

public class JNICriticalTest {
    public static void main(String[] args) throws Exception {
        CountDownLatch latch = new CountDownLatch(16);
        AtomicInteger id = new AtomicInteger();
        ExecutorService es = Executors.newCachedThreadPool(r -> {
            Thread t = new Thread(r);
            t.setName("JNICriticalTest" + id.getAndIncrement());
            return t;
        });
        IntStream.range(0, 4).forEach(ign -> es.execute(() -> {
            Deflater d = new Deflater();
            AtomicInteger n = new AtomicInteger();
            for (int i = 0; i < 4; i++) {
                WispEngine.dispatch(() -> {
                    while (n.get() < 1_000_000) {
                        jniCritical(d);
                        if (n.incrementAndGet() % 100000 == 0) {
                            System.out.println(SharedSecrets.getJavaLangAccess().currentThread0().getName() + "/" + n.get() / 1000 +"k");
                        }
                    }
                    latch.countDown();
                });
            }
        }));
        latch.await();
    }


    private static void jniCritical(Deflater d) {
        d.reset();
        d.setInput(bs);
        d.finish();
        byte[] out = new byte[4096 * 4];

        d.deflate(out); // Enter the JNI critical block here.
    }

    static byte[] bs = new byte[12];
}
