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
 * @summary T12212948
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true LazyUnparkBugTest
 */

public class LazyUnparkBugTest {
    private static volatile Object thunk = null;

    public static void test() throws Exception {
        thunk = null;
        Thread t1 = new Thread(() -> {
            try {
                synchronized (LazyUnparkBugTest.class) {
                    Thread.sleep(1_000L);
                }
                Object o;
                do {
                    o = thunk;
                } while (o == null);
            } catch (Exception e) {
                e.printStackTrace();
            }
        });

        Thread t2 = new Thread(() -> {
            try {
                Thread.sleep(5_0L);
                synchronized (LazyUnparkBugTest.class) {
                    System.out.println("in t2");
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            thunk = new Object();
        });
        t1.start();
        t2.start();
        t1.join();
        t2.join();
    }

    public static void main(String[] args) throws Exception {
        long begin = System.currentTimeMillis();
        test();
        long end = System.currentTimeMillis();
        System.out.println("cost : " + (end - begin) + " ms");
        if ((end - begin) > 2000) {
            throw new Error("this is bug " + (end - begin));
        }
    }

}
