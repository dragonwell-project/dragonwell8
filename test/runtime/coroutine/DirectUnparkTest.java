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
 * @library /testlibrary
 * @summary Test the optimization of direct unpark with Object.wait/notify
 * @run main/othervm  -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.enableThreadAsWisp=true -Dcom.alibaba.wisp.version=2 -Dcom.alibaba.wisp.allThreadAsWisp=true DirectUnparkTest
*/

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;

public class DirectUnparkTest {
    public static void main(String[] args) throws Exception {
        DirectUnparkTest s = new DirectUnparkTest();
        WispEngine.dispatch(s::bar);
        long now = System.currentTimeMillis();
        synchronized (s) {
            s.latch.countDown();
            while (s.finishCnt < N) {
                s.wait();
                s.finishCnt++;
                s.notifyAll();
            }
        }
        long elapsed = System.currentTimeMillis() - now;
        if (elapsed > 3000)
            throw new Error("elapsed = " + elapsed);
    }

    static volatile int finishCnt = 0;
    private CountDownLatch latch = new CountDownLatch(1);
    static final int N = 40000;

    private void bar() {
        try {
            latch.await();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        while (finishCnt < N) {
            synchronized (this) {
                notifyAll();
                finishCnt++;
                try {
                    wait();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }
}
