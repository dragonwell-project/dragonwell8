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
 * @summary test of memory leak while creating and destroying coroutine/thread
 * @run main/othervm -XX:+EnableCoroutine  -Xmx10m  -Xms10m MemLeakTest
 */

import java.dyn.Coroutine;
import java.io.*;

public class MemLeakTest {
    private final static Runnable r = () -> {};

    public static void main(String[] args) throws Exception {
        testThreadCoroutineLeak();
        testUserCreatedCoroutineLeak();
    }


    /**
     * Before fix:  35128kB -> 40124kB
     * After fix :  28368kB -> 28424kB
     */
    private static void testThreadCoroutineLeak() throws Exception {
        // occupy rss
        for (int i = 0; i < 20000; i++) {
            Thread t = new Thread(r);
            t.start();
            t.join();
        }

        int rss0 = getRssInKb();
        System.out.println(rss0);

        for (int i = 0; i < 20000; i++) {
            Thread t = new Thread(r);
            t.start();
            t.join();
        }

        int rss1 = getRssInKb();
        System.out.println(rss1);

        if (rss1 - rss0 > 2048) { // 1M
            throw new Error("thread coroutine mem leak");
        }
    }

    /**
     * Before fix:  152892kB -> 280904kB
     * After fix :  25436kB -> 25572kB
     */
    private static void testUserCreatedCoroutineLeak() throws Exception {
        Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
        // occupy rss
        for (int i = 0; i < 200000; i++) {
            Coroutine target =  new Coroutine(r);
            Coroutine.yieldTo(target); // switch to new created coroutine and let it die
        }

        int rss0 = getRssInKb();
        System.out.println(rss0);

        for (int i = 0; i < 200000; i++) {
            Coroutine target =  new Coroutine(r);
            Coroutine.yieldTo(target);
        }

        int rss1 = getRssInKb();
        System.out.println(rss1);
        if (rss1 - rss0 > 2048) { // 1M
            throw new Error("user created coroutine mem leak");
        }
    }


    private static int getRssInKb() throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader("/proc/self/status"))) {
            int rss = -1;
            String line;
            while ((line = br.readLine()) != null) {
                //i.e.  VmRSS:       360 kB
                if (line.trim().startsWith("VmRSS:")) {
                    int numEnd = line.length() - 3;
                    int numBegin = line.lastIndexOf(" ", numEnd - 1) + 1;
                    rss = Integer.parseInt(line.substring(numBegin, numEnd));
                    break;
                }
            }
            return rss;
        }
    }
}
