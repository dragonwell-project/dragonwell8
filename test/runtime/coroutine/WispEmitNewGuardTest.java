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
 * @summary test emit_guard_for_new in C2 will add control for load
 * @requires os.family == "linux"
 * @run main/othervm -Xcomp -XX:-TieredCompilation -Xbatch -XX:CompileOnly=WispEmitNewGuardTest.testMethod -XX:+PrintCompilation -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true WispEmitNewGuardTest
 */


import com.alibaba.wisp.engine.WispEngine;

import static com.oracle.java.testlibrary.Asserts.*;

public class WispEmitNewGuardTest {
    static {
        System.out.println("================ static initialize ================");
        testMethod();
        System.out.println("================ static initialize done ================");
    }

    public static void main(String[] args) throws Exception{
    }

    public static int testMethod() {
        WispEmitNewGuardTest x = new WispEmitNewGuardTest(42);
        return x.value();
    }

    private int value;
    WispEmitNewGuardTest(int value) {
        this.value = value;
    }

    int value() { return this.value; }
}
