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
 * @summary test bug of update stealEnable fail
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 DisableStealBugTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertTrue;

public class DisableStealBugTest {
    public static void main(String[] args) throws Exception {
        AtomicReference<WispTask> task = new AtomicReference<>();

        WispEngine.dispatch(() -> {
            task.set(SharedSecrets.getWispEngineAccess().getCurrentTask());
            setOrGetStealEnable(task.get(), true, false);
            try {
                Thread.sleep(10);
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        });

        Thread.sleep(2000);
        boolean stealEnable = setOrGetStealEnable(task.get(), false, false);
        assertTrue(stealEnable);
    }

    static boolean setOrGetStealEnable(WispTask task, boolean isSet, boolean b) {
        try {
            Field resumeEntryField = task.getClass().getDeclaredField("resumeEntry");
            resumeEntryField.setAccessible(true);
            final Object resumeEntry = resumeEntryField.get(task);

            Field stealEnableField = resumeEntry.getClass().getDeclaredField("stealEnable");
            stealEnableField.setAccessible(true);
            if (isSet) {
                stealEnableField.setBoolean(resumeEntry, b);
                return b;
            } else {
                return stealEnableField.getBoolean(resumeEntry);
            }
        } catch (ReflectiveOperationException e) {
            throw new Error(e);
        }
    }
}
