/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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

import org.testng.Assert;
import org.testng.annotations.Test;

/*
 * @test
 * @bug 8271459
 * @summary C2 applies string opts to StringBuilder object created with a negative size and misses the NegativeArraySizeException.
 * @run testng TestNegativeStringBuilderCapacity
 */
public class TestNegativeStringBuilderCapacity {

    static final int pass_count = 10000;

    static final String doIdenticalPositiveConst() throws Throwable {
        // C2 knows that argument is 5 and applies string opts without runtime check.
        StringBuilder sb = new StringBuilder(5); // StringBuilder object optimized away by string opts.
        return sb.toString(); // Call optimized away by string opts.
    }

    @Test
    public static final void testIdenticalPositiveConst() {
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doIdenticalPositiveConst();
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, 0, String.format("%d exception were thrown, 0 expected", throw_count));
    }

    static final String doIdenticalNegativeConst() throws Throwable {
        // C2 knows that we always have a negative int -> bail out of string opts
        StringBuilder sb = new StringBuilder(-5);
        return sb.toString(); // Call stays due to bailout.
    }

    @Test
    public static final void testIdenticalNegativeConst() {
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doIdenticalNegativeConst();
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, pass_count, String.format("%d exception were thrown, %d expected", throw_count, pass_count));
    }

    static int aField;

    static final String doField() throws Throwable {
        // C2 does not know if iFld is positive or negative. It applies string opts but inserts a runtime check to
        // bail out to interpreter
        StringBuilder sb = new StringBuilder(aField);
        return sb.toString();
    }

    @Test
    public static final void testPositiveField() {
        aField = 4;
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doField();
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, 0, String.format("%d exception were thrown, 0 expected", throw_count));
    }

    @Test
    public static final void testNegativeField() {
        aField = -4;
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doField();
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, pass_count, String.format("%d exception were thrown, %d expected", throw_count, pass_count));
    }

    static final String doPossiblyNegativeConst(boolean flag) throws Throwable {
        // C2 knows that cap is between -5 and 5. It applies string opts but inserts a runtime check to
        // bail out to interpreter. This path is sometimes taken and sometimes not.
        final int capacity = flag ? 5 : -5;
        StringBuilder sb = new StringBuilder(capacity);
        return sb.toString();
    }

    @Test
    public static final void testPossiblyNegativeConst() {
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doPossiblyNegativeConst((pass % 2) == 0);
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, pass_count/2, String.format("%d exception were thrown, %d expected", throw_count, pass_count/2));
    }

    static final String doPositiveConst(boolean flag) throws Throwable {
        // C2 knows that cap is between 1 and 100 and applies string opts without runtime check.
        final int capacity = flag ? 1 : 100;
        StringBuilder sb = new StringBuilder(capacity);
        return sb.toString();
    }

    @Test
    public static final void testPositiveConst() {
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doPositiveConst((pass % 2) == 0);
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, 0, String.format("%d exception were thrown, 0 expected", throw_count));
    }

    static final String doArg(int capacity) throws Throwable {
        // C2 does not know if cap is positive or negative. It applies string opts but inserts a runtime check to
        // bail out to interpreter. This path is always taken because cap is always negative.
        StringBuilder sb = new StringBuilder(capacity);
        return sb.toString();
    }

    @Test
    public static final void testArg() {
        int throw_count = 0;
        for ( int pass = 0; pass < pass_count; pass++ ) {
            try {
                String s = doArg((pass % 2) == 0 ? 3 : -3 );
            } catch (NegativeArraySizeException e) {
                throw_count++;
            } catch (Throwable e) {
                Assert.fail("Unexpected exception thrown");
            }
        }
        Assert.assertEquals( throw_count, pass_count/2, String.format("%d exception were thrown, %d expected", throw_count, pass_count/2));
    }
}
