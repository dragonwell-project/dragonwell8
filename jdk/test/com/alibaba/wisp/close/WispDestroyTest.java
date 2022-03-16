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
 * @summary Test WispCarrier's destroy
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true WispDestroyTest
 */

import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.util.concurrent.CountDownLatch;

public class WispDestroyTest {

    static Object e;

    public static void main(String[] args) throws Exception {
        CountDownLatch l = new CountDownLatch(1);

        new Thread(() -> {
            try {
                Field f = WispTask.class.getDeclaredField("carrier");
                f.setAccessible(true);
                e = f.get(SharedSecrets.getJavaLangAccess().getWispTask(Thread.currentThread()));
                l.countDown();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();

        l.await();

        Thread.sleep(1000); // ensure Thread.exit() executed


        Field terminated = e.getClass().getDeclaredField("terminated");
        terminated.setAccessible(true);
        if (!terminated.getBoolean(e)) throw new Error("resource leak!");
    }
}
