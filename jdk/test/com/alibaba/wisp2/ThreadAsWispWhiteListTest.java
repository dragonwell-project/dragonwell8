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
 * @summary test thread as wisp white list
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.enableThreadAsWisp=true -Dcom.alibaba.wisp.allThreadAsWisp=false -Dcom.alibaba.wisp.threadAsWisp.white=name:wisp-* ThreadAsWispWhiteListTest
 */

import sun.misc.SharedSecrets;

import java.util.concurrent.FutureTask;

import static jdk.testlibrary.Asserts.assertFalse;
import static jdk.testlibrary.Asserts.assertTrue;

public class ThreadAsWispWhiteListTest {
    public static void main(String[] args) throws Exception {
        FutureTask<Boolean> future = new FutureTask<>(ThreadAsWispWhiteListTest::isRealThread);
        new Thread(future, "wisp-1").start();
        assertFalse(future.get());
        future = new FutureTask<>(ThreadAsWispWhiteListTest::isRealThread);
        new Thread(future, "thread-1").start();
        assertTrue(future.get());
    }

    private static boolean isRealThread() {
        return SharedSecrets.getJavaLangAccess().currentThread0() == Thread.currentThread();
    }
}
