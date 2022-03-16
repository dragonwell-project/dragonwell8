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
 * @summary test all thread as wisp black list
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 ThreadAsWispBlackListTest
 */

import sun.misc.SharedSecrets;

import java.lang.reflect.Method;
import java.util.concurrent.Executors;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.FutureTask;

import static jdk.testlibrary.Asserts.assertFalse;
import static jdk.testlibrary.Asserts.assertTrue;

public class ThreadAsWispBlackListTest {
    public static void main(String[] args) throws Exception {
        assertFalse(Executors.newSingleThreadExecutor().submit(ThreadAsWispBlackListTest::isRealThread).get());
        byClass();
        byPackage();
        byName();
        testCommonPool();
        testMultiEntry();
        wildcardTest();
    }

    private static void byPackage() throws Exception {
        setBlackList("package:java.util.concurrent");
        assertTrue(Executors.newSingleThreadExecutor().submit(ThreadAsWispBlackListTest::isRealThread).get());
    }

    private static void byClass() throws Exception {
        FutureTask<Boolean> future = new FutureTask<>(ThreadAsWispBlackListTest::isRealThread);
        class T1 extends Thread {
            @Override
            public void run() {
                future.run();
            }
        }
        setBlackList("class:" + T1.class.getName());
        new T1().start();
        assertTrue(future.get());
    }

    private static void byName() throws Exception {
        wildcardTest1("n1111", "n1111");
    }

    private static void wildcardTest() throws Exception {
        wildcardTest1("a-*", "a-1");
        wildcardTest1("a-*", "a-2");
        wildcardTest1("a-*", "a-4999");
        wildcardTest1("a-*", "a-abc");

        wildcardTest1("a-*-b", "a-1-b");
        wildcardTest1("a-*-b", "a-2-b");
        wildcardTest1("a-*-b", "a-4999-b");
        wildcardTest1("a-*-b", "a-abc-b");

        wildcardTest1("a-*z", "a-1-bz");
        wildcardTest1("a-*z", "a-2-bz");
        wildcardTest1("a-*z", "a-4999-bz");
        wildcardTest1("a-*z", "a-abc-bz");

        wildcardTest1("*z", "a-1-bz");
        wildcardTest1("*z", "a-2-bz");
        wildcardTest1("*z", "a-4999-bz");
        wildcardTest1("*z", "a-abc-bz");

        wildcardTest1("?z", "zz");
        wildcardTest1("a?z", "agz");
        wildcardTest1("a?", "a1");
    }

    private static void wildcardTest1(String pattern, String name) throws Exception {
        setBlackList("name:" + pattern);
        FutureTask<Boolean> future = new FutureTask<>(ThreadAsWispBlackListTest::isRealThread);
        new Thread(future, name).start();
        assertTrue(future.get());
    }

    private static void testCommonPool() throws Exception {
        setBlackList("name:ForkJoinPool.commonPool-worker-*");
        FutureTask<Boolean> future = new FutureTask<>(ThreadAsWispBlackListTest::isRealThread);
        ForkJoinPool.commonPool().execute(future);
        assertTrue(future.get());
    }

    private static void testMultiEntry() throws Exception {
        setBlackList("name:m1111;name:m2222");
        FutureTask<Boolean> future = new FutureTask<>(ThreadAsWispBlackListTest::isRealThread);
        new Thread(future, "m1111").start();
        assertTrue(future.get());
        future = new FutureTask<>(ThreadAsWispBlackListTest::isRealThread);
        new Thread(future, "m2222").start();
        assertTrue(future.get());
    }

    private static boolean isRealThread() {
        return SharedSecrets.getJavaLangAccess().currentThread0() == Thread.currentThread();
    }

    private static void setBlackList(String list) throws Exception {
        System.setProperty("com.alibaba.wisp.threadAsWisp.black", list);
        Method m = Class.forName("com.alibaba.wisp.engine.WispConfiguration").getDeclaredMethod("loadBizConfig");
        m.setAccessible(true);
        m.invoke(null);
    }
}
