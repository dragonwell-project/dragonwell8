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

package jdk.testlibrary;

import java.lang.reflect.Method;
import java.util.Arrays;
import static jdk.testlibrary.Asserts.fail;

public class TestUtils {

    /**
     * Run all methods of {@code clazz} whose names start with {@code prefix}
     * @param prefix
     * @param clazz
     * @param object
     */
    public static void runWithPrefix(String prefix, Class clazz, Object object) {
        if (prefix == null || clazz == null || object == null) {
            throw new IllegalArgumentException("Bad arguments");
        }

        long totalTests = Arrays.stream(clazz.getDeclaredMethods())
                .filter(m -> m.getName().startsWith(prefix))
                .count();
        long passed = 0;
        long failed = 0;

        for (Method method : clazz.getDeclaredMethods()) {
            if (method.getName().startsWith(prefix)) {
                method.setAccessible(true);
                String name = clazz.getName() + "." + method.getName();
                println("=== Begin test " + name + " ===");
                try {
                    method.invoke(object);
                    ++passed;
                    println("=== PASSED (" + passed + " passed, " + failed +" failed, "
                            + totalTests + " total) ===");
                } catch (Throwable e) {
                    e.printStackTrace();
                    ++failed;
                    println("=== FAILED ( " + passed + " passed, " + failed +" failed"
                            + totalTests + " total) ===");
                }
            }
        }

        if (failed != 0) {
            fail("Total " + failed + "/" + totalTests + " testcases failed, class " + clazz.getName());
        } else {
            println("All " + totalTests + " testcases passed, class " + clazz.getName());
        }
    }

    private static void println(String msg) {
        System.err.println(msg);
        System.out.println(msg);
    }
}
