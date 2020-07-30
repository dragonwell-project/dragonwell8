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
 * @summary Test Daemon Thread Group implementation
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.useCarrierAsPoller=false DaemonThreadGroupTest
*/


import java.lang.reflect.Field;

import static jdk.testlibrary.Asserts.assertTrue;


/**
 * test the Daemon Thread Group implementation
 */
public class DaemonThreadGroupTest {
    public static void main(String... arg) throws Exception {
        Field f = Class.forName("com.alibaba.wisp.engine.WispEngine").getDeclaredField("unparkDispatcher");
        f.setAccessible(true);
        Thread t = (Thread) f.get(null);

        f = Class.forName("com.alibaba.wisp.engine.WispEngine").getDeclaredField("DAEMON_THREAD_GROUP");
        f.setAccessible(true);
        ThreadGroup threadGroup = (ThreadGroup) f.get(null);

        System.out.println(threadGroup.getName());

        assertTrue(t.getThreadGroup() == threadGroup, "the thread isn't in daemonThreadGroup");
    }
}
