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
 * @summary verify carrier is doing epoll instead of poller when useCarrierAsPoller is enabled
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.useCarrierAsPoller=true CarrierAsPollerTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.useCarrierAsPoller=false CarrierAsPollerTest
 */


import java.lang.reflect.Field;

import static jdk.testlibrary.Asserts.*;

public class CarrierAsPollerTest {
    public static void main(String[] args) throws Exception {
        new Thread(() -> {}).start();  // if we're owner..let another carrier become owner
        long start = System.currentTimeMillis();
        while (System.currentTimeMillis() - start < 200) {
            // wait acquire
        }

        Class<?> clazz = Class.forName("com.alibaba.wisp.engine.WispEventPump$Pool");
        Field pumps = clazz.getDeclaredField("pumps");
        pumps.setAccessible(true);
        Object[] a = (Object[]) pumps.get(clazz.getEnumConstants()[0]);
        Field ownerField = Class.forName("com.alibaba.wisp.engine.WispEventPump").getDeclaredField("owner");
        ownerField.setAccessible(true);
        Object owner = ownerField.get(a[0]);
        if (!Boolean.getBoolean("com.alibaba.wisp.useCarrierAsPoller")) {
            assertNull(owner);
            return;
        }

        assertNotNull(owner);
        Field threadField = owner.getClass().getDeclaredField("thread");
        threadField.setAccessible(true);
        Thread thread = (Thread) threadField.get(owner);
        System.out.println(thread.getName());
        assertTrue(thread.getName().startsWith("Wisp-Root-Worker"));
    }
}
