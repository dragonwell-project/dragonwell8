/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
import sun.hotspot.WhiteBox;
import java.util.*;

/*
 * @test TestMethodRecorder
 * @library /testlibrary /testlibrary/whitebox
 * @build TestMethodRecorder
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:-TieredCompilation
 *                   -XX:+CompilationWarmUpRecording
 *                   -XX:-ClassUnloading
 *                   -XX:+UseConcMarkSweepGC
 *                   -XX:-CMSClassUnloadingEnabled
 *                   -XX:-UseSharedSpaces
 *                   -XX:+PrintCompilationWarmUpDetail
 *                   -XX:CompilationWarmUpLogfile=./test.log
 *                   -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *                   TestMethodRecorder
 * @summary Test method recording in CompilationWarmUpRecording model
 */
public class TestMethodRecorder {
    private static WhiteBox whiteBox;
    public static void main(String[] args) throws Exception {
        whiteBox = WhiteBox.getWhiteBox();

        InnerA a = new InnerA();
        a.foo();
        Thread.sleep(3000);
        a.foo();

        String[] list = whiteBox.getCompiledMethodList();
        System.out.println(list.length);
        String methodName = "foo2";

        boolean contain = false;
        for (int i = 0; i < list.length; i++) {
            String s = list[i];
            if (s.equals(methodName)) {
                contain = true;
            }
            System.out.println("method is " + s);
        }
        assertTrue(contain);
    }

    public static class InnerA {
        static {
            System.out.println("InnerA initialize");
        }

        public static String[] aa = new String[0];
        public static List<String> ls = new ArrayList<String>();
        public String foo() {
            for (int i = 0; i < 12000; i++) {
                foo2(aa);
            }
            ls.add("x");
            return ls.get(0);
        }
        public void foo2(String[] a) {
            String s = "aa";
            if (ls.size() > 100 && a.length < 100) {
                ls.clear();
            } else {
                ls.add(s);
            }
        }
    }
}
