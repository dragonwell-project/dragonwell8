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
 * @summary this feature is not supported in wisp2, just check compatibility
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true DisableThreadAsWispAtRuntimeTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.lang.reflect.Field;

import static jdk.testlibrary.Asserts.assertEQ;

public class DisableThreadAsWispAtRuntimeTest {


    public static void main(String[] args) throws Exception {
        switchShift(true);
        switchShift(false);
    }

    private static void switchShift(boolean val) {
        try {
            Class<?> wispClazz = Class.forName("com.alibaba.wisp.engine.WispEngine");
            Field field = wispClazz.getDeclaredField("shiftThreadModel");
            field.setAccessible(true);
            field.setBoolean(null /*static field*/, val);
        } catch (ReflectiveOperationException e) {
            throw new Error(e);
        }
        assertEQ(WispEngine.enableThreadAsWisp(), val);
    }
}
