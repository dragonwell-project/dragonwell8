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
 * @summary verify epollArray is set for Selector.select()
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.monolithicPoll=true MonolithicPollTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.monolithicPoll=false MonolithicPollTest
 */

import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.nio.channels.Selector;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertEQ;

public class MonolithicPollTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<WispTask> task = new AtomicReference<>();
        Executors.newSingleThreadExecutor().submit(() -> {
            task.set(SharedSecrets.getWispEngineAccess().getCurrentTask());
            Selector.open().select();
            return null;
        });

        Thread.sleep(200);

        while (task.get() == null) {
        }

        Boolean nz = Executors.newSingleThreadExecutor().submit(() -> {
            Field arrayField = WispTask.class.getDeclaredField("epollArray");
            arrayField.setAccessible(true);
            long array = arrayField.getLong(task.get());
            return array != 0;
        }).get();

        assertEQ(nz, Boolean.getBoolean("com.alibaba.wisp.monolithicPoll"));
    }
}
