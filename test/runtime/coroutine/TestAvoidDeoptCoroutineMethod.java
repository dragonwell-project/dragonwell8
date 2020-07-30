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
 * @test TestAvoidDeoptCoroutineMethod
 * @library /testlibrary /testlibrary/whitebox
 * @build TestAvoidDeoptCoroutineMethod
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -XX:+EnableCoroutine -Xmx10m -Xms10m -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI TestAvoidDeoptCoroutineMethod
 * @summary test avoid coroutine intrinsic method to be deoptimized
 * @requires os.family == "linux"
 */

import sun.hotspot.WhiteBox;
import java.dyn.Coroutine;
import java.io.*;

public class TestAvoidDeoptCoroutineMethod {
    public static void main(String[] args) throws Exception {
        WhiteBox whiteBox = WhiteBox.getWhiteBox();
        Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
        runSomeCoroutines(threadCoro);
        // deoptimize all
        whiteBox.deoptimizeAll();
        // if intrinsic methods of coroutine have been deoptimized, it will crash here
        runSomeCoroutines(threadCoro);
    }

    private static void runSomeCoroutines(Coroutine threadCoro) throws Exception {
        for (int i = 0; i < 10000; i++) {
            Coroutine.yieldTo(new Coroutine(() -> {})); // switch to new created coroutine and let it die
        }
        System.out.println("end of run");
    }
}
