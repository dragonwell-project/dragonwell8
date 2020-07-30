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
 * @summary test bug fix of SharedSecrets and Unsafe class initializer circular dependency
 * @requires os.family == "linux"
 * @run main UnsafeDependencyBugTest 10
 */


import java.io.File;
import java.util.concurrent.TimeUnit;

import static jdk.testlibrary.Asserts.assertTrue;

/**
 * We need thousand times to reproduce the DEADLOCK. Don't spend too much time here..
 * We already add svt test ajdk_svt/wispTest to ensure the DEADLOCK already solved.
 */
public class UnsafeDependencyBugTest {

    public static void main(String[] args) throws Exception {
        long start = System.currentTimeMillis();
        int i;
        for (i = 0; System.currentTimeMillis() - start < Integer.valueOf(args[0]) * 1000; i++) {
            runLauncherWithWisp();
        }
        System.out.println("tested " +  i + " times");
    }

    private static void runLauncherWithWisp() throws Exception {
        Process p = new ProcessBuilder(System.getProperty("java.home") + "/bin/java", "-XX:+UnlockExperimentalVMOptions", "-XX:+EnableCoroutine")
                .redirectErrorStream(true)
                .redirectOutput(new File("/dev/null"))
                .start();

        assertTrue(p.waitFor(4, TimeUnit.SECONDS));
    }
}
