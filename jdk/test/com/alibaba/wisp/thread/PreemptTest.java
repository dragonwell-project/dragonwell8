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
 * @summary test wisp time slice preempt
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.carrierEngines=1 PreemptTest

 */

import com.alibaba.wisp.engine.WispEngine;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;

public class PreemptTest {
    public static void main(String[] args) throws Exception {
        doTest(PreemptTest::simpleLoop);
        doTest(PreemptTest::jniLoop);
        doTest(PreemptTest::voidLoop);
    }

    private static void doTest(Runnable r) throws Exception {
        WispEngine.dispatch(r);
        CountDownLatch latch = new CountDownLatch(1);
        WispEngine.dispatch(latch::countDown);
        assertTrue(latch.await(5, TimeUnit.SECONDS));

    }

    private static void complexLoop() {
        MessageDigest md;
        try {
            md = MessageDigest.getInstance("MD5");
        } catch (NoSuchAlgorithmException e) {
            throw new Error(e);
        }

        while (true) {
            md.update("welcome to the wisp world!".getBytes());
            n = md.digest()[0];
        }
    }

    private static void simpleLoop() {
        int x;
        do {
            x = n;
        } while (x == n);
    }

    // TODO: handle safepoint consumed by state switch



    private static void voidLoop() {
        while (true) {}
    }

    private static void jniLoop() {
        try {
            while (true) { Thread.sleep(0);}
        } catch (Exception e) {
            assertTrue(false);
        }
    }

    volatile static int n;
}
