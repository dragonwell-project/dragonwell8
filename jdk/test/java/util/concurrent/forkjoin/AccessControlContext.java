/*
 * Copyright (c) 2020, Azul Systems, Inc. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

/*
 * @test
 * @bug 8249677 8249846
 * @summary AccessControlContext should not be dropped in ForkJoinThread
 * @run main/othervm/policy=AccessControlContext.policy/timeout=20 AccessControlContext inherit
 * @run main/othervm/policy=AccessControlContext.policy/timeout=20 AccessControlContext default
 */

import java.security.AccessController;
import java.security.AccessControlException;

import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.ForkJoinWorkerThread;
import java.util.concurrent.RecursiveTask;

public class AccessControlContext {

    static void testPermission() {
        System.getProperty("java.version");
    }

    static void testInherit() {
        ForkJoinPool pool = new ForkJoinPool(1,
                new ForkJoinPool.ForkJoinWorkerThreadFactory() {
                    @Override
                    public ForkJoinWorkerThread newThread(ForkJoinPool forkJoinPool) {
                        return new ForkJoinWorkerThread(forkJoinPool) {
                            @Override
                            public void run() {
                                testPermission();
                                super.run();
                            }
                        };
                    }
                },
                null, false);

        pool.invoke(new RecursiveTask<Object>() {
            @Override
            protected Object compute() {
                System.out.println("done");
                return null;
            }
        });

        pool.shutdown();
    }

    static void testDefault() {
        ForkJoinPool pool = new ForkJoinPool(1);

        pool.invoke(new RecursiveTask<Object>() {
            @Override
            protected Object compute() {
                testPermission();
                System.out.println("done");
                return null;
            }
        });

        pool.shutdown();
    }

    public static void main(String[] args){
        testPermission();

        switch (args[0]) {
        case "inherit":
            testInherit();
            break;
        case "default":
            // Case fails because of "JDK-8249846: Change of behavior after
            // JDK-8237117: Better ForkJoinPool behavior".
            System.out.println("Known to fail with AccessControlException since 8u262 (see JDK-8249846)");
            try {
                testDefault();
                throw new RuntimeException("Pool thread has inherited permissions.");
            } catch (AccessControlException e) {
                System.out.println("PASSED: " + e);
            }
            break;
        }

    }
}
