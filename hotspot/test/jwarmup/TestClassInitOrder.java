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

import sun.hotspot.WhiteBox;
import com.oracle.java.testlibrary.*;

/*
 * @test TestClassInitOrder
 * @library /testlibrary /testlibrary/whitebox
 * @build TestClassInitOrder
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -Xbootclasspath/a:. -XX:-TieredCompilation
 *                   -XX:-UseSharedSpaces
 *                   -XX:+CompilationWarmUpRecording
 *                   -XX:-ClassUnloading
 *                   -XX:+UseConcMarkSweepGC
 *                   -XX:-CMSClassUnloadingEnabled
 *                   -XX:+PrintCompilationWarmUpDetail
 *                   -XX:CompilationWarmUpLogfile=./test.log
 *                   -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *                   TestClassInitOrder
 * @summary Verify class initialization order in JitWarmUp recording
 */
public class TestClassInitOrder {
    private static WhiteBox whiteBox;
    public static void main(String[] args) throws Exception {
        whiteBox = WhiteBox.getWhiteBox();
        InnerA a = new InnerA();
        System.out.println("after new A");

        InnerB b = new InnerB();
        System.out.println("after new B");

        String[] list = whiteBox.getClassInitOrderList();
        System.out.println(list.length);

        String InnerAName = InnerA.class.getName();
        String InnerBName = InnerB.class.getName();
        int orderA = 0;
        int orderB = 0;
        for (int i = 0; i < list.length; i++) {
            String s = list[i];
            if (s.equals(InnerAName)) {
                orderA = i;
            } else if (s.equals(InnerBName)) {
                orderB = i;
            }
        }
        System.out.println("Class A order is " + orderA);
        System.out.println("Class B order is " + orderB);
        Asserts.assertTrue(orderA < orderB);
    }

    public static class InnerA {
        static {
            System.out.println("InnerA initialize");
        }
    }

    public static class InnerB {
        static {
            System.out.println("InnerB initialize");
        }
    }
}
