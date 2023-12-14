/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @bug 8202952
 * @summary C2: Unexpected dead nodes after matching
 *
 * @run main/othervm -XX:-TieredCompilation -Xcomp -XX:CompileOnly=::test
 *      compiler.c2.TestMatcherLargeOffset
 */
package compiler.c2;

public class TestMatcherLargeOffset {

    public static int s = 0;
    public static final int N = 400;
    public static int a[] = new int[N];

    public static void test() {
        int x = -57785;

        for (int i = 0; i < N; i++) {
            a[(x >>> 1) % N] &= (--x);

            for (int j = 4; j > 1; j -= 3) {
                switch ((j % 9) + 119) {
                    case 122:
                        s += i;
                        break;
                    case 123:
                        s += i;
                        break;
                    default:
                }
            }
        }
    }

    public static void main(String[] args) {
        for (int i = 0; i < 16000; i++) {
            test();
        }

        System.out.println("s = " + s);
   }
}
