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
 * @summary Test timer implementation
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine OverflowTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertTrue;

/**
 * test the time out implementation
 */
public class OverflowTest {
    public static void main(String... arg) throws Exception {

        WispEngineAccess access = SharedSecrets.getWispEngineAccess();

        AtomicReference<WispTask> task1 = new AtomicReference<>();
        AtomicBoolean doAction = new AtomicBoolean(false);
        AtomicBoolean hasError = new AtomicBoolean(false);

        WispEngine.dispatch(() -> {
            task1.set(access.getCurrentTask());
            access.park(Long.MAX_VALUE);
            // if timeout is negative(< now()), this task is selected in doSchedule
            // and park returns immediately
            hasError.set(!doAction.get()); // should not reach here before doing unpark
        });

        access.sleep(100); // switch task
        // let task exit
        doAction.set(true);
        access.unpark(task1.get());
        assertTrue(!hasError.get(), "hasError.get() should be false.");
    }
}
