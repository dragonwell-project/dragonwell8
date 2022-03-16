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
 * @summary Test low level coroutine implement
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine CoroutineTest
 */

import java.dyn.Coroutine;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * test low level coroutine implement
 * 2 coroutine switch to each other and see the sequence
 */
public class CoroutineTest {
    static int i = 0;
    static Coroutine co1, co2;

    public static void main(String[] args) {
        try {

            co1 = new Coroutine(() -> {
                try {
                    if (i++ != 0)
                        throw new RuntimeException("co1 wrong sequence, expect 0");
                    Coroutine.yieldTo(co2);
                    if (i++ != 2)
                        throw new RuntimeException("co1 wrong sequence, expect 2");
                    Coroutine.yieldTo(co2);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
            co2 = new Coroutine(() -> {

                try {
                    if (i++ != 1)
                        throw new RuntimeException("co2 wrong sequence, expect 1");
                    Coroutine.yieldTo(co1);
                    if (i++ != 3)
                        throw new RuntimeException("co2 wrong sequence, expect 3");
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });

            Coroutine.yieldTo(co1);


        } catch (Exception e) {
            throw new RuntimeException();
        }
    }
}
