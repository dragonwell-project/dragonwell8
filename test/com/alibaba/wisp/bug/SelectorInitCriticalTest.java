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
 * @summary Test the fix to NPE issue caused by unexpected co-routine yielding on synchronized(lock) in SelectorProvider.provider() during initialization of WispCarrier
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true -XX:+UseWispMonitor SelectorInitCriticalTest
 */


import java.lang.reflect.Field;
import java.nio.channels.spi.SelectorProvider;
import java.util.concurrent.CountDownLatch;

public class SelectorInitCriticalTest {
    public static void main(String[] args) throws Exception {
        Field f = SelectorProvider.class.getDeclaredField("lock");
        f.setAccessible(true);
        Object selectorProviderLock = f.get(null);
        CountDownLatch latch = new CountDownLatch(1);

        Thread t = new Thread(latch::countDown);

        synchronized (selectorProviderLock) {
            t.start();
            // Holding selectorProviderLock for a while which will eventually blocks the initialization of t' WispCarrier
            Thread.sleep(100);
        }

        latch.await();

    }
}
