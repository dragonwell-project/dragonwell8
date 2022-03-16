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
 * @summary Verify park not happened for a very small interval
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 YieldFewNanosTest
 */

import com.alibaba.wisp.engine.WispEngine;

import sun.misc.SharedSecrets;

import java.lang.reflect.Method;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.LockSupport;

import static jdk.testlibrary.Asserts.assertTrue;

public class YieldFewNanosTest {
    public static void main(String[] args) throws Exception {
        assertTrue(Executors.newSingleThreadExecutor().submit(() -> {
            long pc = (long) new YieldEmptyQueueTest.ObjAccess(SharedSecrets.getJavaLangAccess().getWispTask(Thread.currentThread()))
                    .ref("carrier").ref("counter").ref("parkCount").obj;
            LockSupport.parkNanos(1);
            return (long) new YieldEmptyQueueTest.ObjAccess(SharedSecrets.getJavaLangAccess().getWispTask(Thread.currentThread()))
                    .ref("carrier").ref("counter").ref("parkCount").obj == pc;
        }).get());
    }
}
