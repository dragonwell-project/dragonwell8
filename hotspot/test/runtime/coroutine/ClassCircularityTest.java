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
 * @summary test fix of parallel class-loading problem when we're using coroutine
 * @requires os.family == "linux"
 * @run main/othervm -Xmx20m -XX:+AllowParallelDefineClass -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true ClassCircularityTest
 */


import com.alibaba.wisp.engine.WispEngine;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.concurrent.CountDownLatch;

import static com.oracle.java.testlibrary.Asserts.*;

public class ClassCircularityTest {

    class TestSuper {
    }

    class TestChild extends TestSuper {
    }

    private static final String CLAZZ = "ClassCircularityTest$TestChild";
    private static final String SUPER_CLAZZ = "ClassCircularityTest$TestSuper";

    public static void main(String[] args) throws Exception {
        URLClassLoader appLoader = (URLClassLoader) ClassCircularityTest.class.getClassLoader();
        for (int i = 0; i < 2000; i++) {
            boolean[] classCircularityError = new boolean[]{false};

            CountDownLatch latch = new CountDownLatch(1);
            ClassLoader loader = new ParallelClassLoader(appLoader.getURLs(), classCircularityError, latch);
            Class.forName(CLAZZ, true, loader);
            latch.await();
            assertFalse(classCircularityError[0]);
            if (i % 1000 == 0) System.out.println(i);
        }
    }

    static class ParallelClassLoader extends URLClassLoader {
        static {
            registerAsParallelCapable();
        }

        public ParallelClassLoader(URL[] urls, boolean[] classCircularityError, CountDownLatch latch) {
            super(urls, null);
            this.classCircularityError = classCircularityError;
            this.latch = latch;
        }

        private boolean oneShot = false;
        private boolean[] classCircularityError;
        private CountDownLatch latch;

        @Override
        public Class<?> loadClass(String name) throws ClassNotFoundException {
            if (name.equals(SUPER_CLAZZ) && !oneShot) {
                oneShot = true;
                WispEngine.dispatch(() -> {
                    try {
                        Class.forName(CLAZZ, true, ParallelClassLoader.this);
                    } catch (ClassNotFoundException e) {
                        throw new Error(e);
                    } catch (ClassCircularityError e) {
                        classCircularityError[0] = true;
                    }
                    latch.countDown();
                });
            }
            return super.loadClass(name);
        }
    }
}
