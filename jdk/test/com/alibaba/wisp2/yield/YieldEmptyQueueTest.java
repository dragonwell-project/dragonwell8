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
 * @summary Verify yield not really happened when queue is empty
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 YieldEmptyQueueTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.Executors;

import static jdk.testlibrary.Asserts.assertTrue;

public class YieldEmptyQueueTest {
    public static void main(String[] args) throws Exception {

        assertTrue(Executors.newSingleThreadExecutor().submit(() -> {
            long sc = (long) new ObjAccess(SharedSecrets.getJavaLangAccess().getWispTask(Thread.currentThread()))
                    .ref("carrier").ref("counter").ref("switchCount").obj;
            Thread.yield();
            return (long) new ObjAccess(SharedSecrets.getJavaLangAccess().getWispTask(Thread.currentThread()))
                    .ref("carrier").ref("counter").ref("switchCount").obj == sc;
        }).get());
    }

    static class ObjAccess {
        Object obj;

        ObjAccess(Object obj) {
            this.obj = obj;
        }

        ObjAccess ref(String field) {
            try {
                Field f;
                try {
                    f = obj.getClass().getDeclaredField(field);
                } catch (NoSuchFieldException e) {
                    f = obj.getClass().getSuperclass().getDeclaredField(field);
                }
                f.setAccessible(true);
                return new ObjAccess(f.get(obj));
            } catch (ReflectiveOperationException e) {
                throw new Error(e);
            }
        }
    }
}
