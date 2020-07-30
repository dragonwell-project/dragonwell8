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
 * @summary Test fix of WispCarrier block on Thread.class lock
 * @requires os.family == "linux"
 * @run main/othervm -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -XX:SyncKnobs="ReportSettings=1:QMode=1"  -Dcom.alibaba.wisp.transparentWispSwitch=true ThreadLockTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

public class ThreadLockTest {
    public static void main(String[] args) throws Exception {

        CountDownLatch done = new CountDownLatch(1);
        AtomicReference<WispEngine> es = new AtomicReference<>(null);

        synchronized (Thread.class) {
            new Thread(() -> {
                es.set(WispEngine.current());
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }, "ThreadIdGeneratorLockTest").start();

            Thread.sleep(100); // make sure "ThreadIdGeneratorLockTest" already sleeping

            /*
                 if ((SyncFlags & 16) == 0 && nxt == NULL && _EntryList == NULL) {
                    // Try to assume the role of responsible thread for the monitor.
                    // CONSIDER:  ST vs CAS vs { if (Responsible==null) Responsible=Self }
                     Atomic::cmpxchg_ptr (Self, &_Responsible, NULL) ;
                 }
                  ----------------
                 if (_Responsible == Self || (SyncFlags & 1)) {
                     use timeout park
                 }
                 add a waiter, let _Responsible != Self
             */
            new Thread(() -> {
                try {
                    Method m = Thread.class.getDeclaredMethod("nextThreadNum");
                    m.setAccessible(true);
                    m.invoke(null);
                } catch (ReflectiveOperationException e) {
                    throw new Error(e);
                }
            }, "waiter").start();
            Thread.sleep(100); // make sure adding waiter is done


            es.get().execute(() -> { // give an name "A"
                Thread.currentThread(); // before fix, we'll hang here
                done.countDown();
            });

            // objectMonitor::EnterI generally puts waiter to head, now _cxq is:
            // "A" (os_park) --> "A" (wisp_park) --> "waiter"
            // set QMode=1 to reverse the queue
            // _Entry_list is:
            // "waiter" --> "A" (wisp_park) --> "A" (os_park)
            Thread.sleep(100); // still hold lock 100 ms, make sure the lsat wisp blocked
        }
        // release:
        // 1. wisp unpark "waiter"
        // 2. wisp unpark "A" (wisp_park) (just after waiter ends)
        // now "ThreadIdGeneratorLockTest" Thread is on os_park, test failed!

        done.await();
    }
}
