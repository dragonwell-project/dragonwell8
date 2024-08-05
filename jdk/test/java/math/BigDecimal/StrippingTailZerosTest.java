/*
 * Copyright (c) 2024, Alibaba Group Holding Limited. All rights reserved.
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
 * @summary test correctness of BigDecimal.divide via optimization
 * @library /test/lib
 * @run main/othervm StrippingTailZerosTest
 */

import java.lang.reflect.Field;
import java.math.BigDecimal;
import java.math.MathContext;
import java.security.SecureRandom;
import jdk.test.lib.Asserts;

public class StrippingTailZerosTest {
    private static final int TEST_ROUND = 1000000;

    private static void testDivision(double dividend, Field optField) throws Exception{
        /*
        We intend to apply a low precision for dividend and a high precision for quotient.
        Together with divisor 2, the quotient should have a long tail of zeros.
        Like 67.89200 / 2 = 33.9460000000000
        And then out optimization about stripping zeros should take effect.
         */
        BigDecimal num1 = new BigDecimal(dividend, MathContext.DECIMAL32);
        BigDecimal num2 = new BigDecimal(2);

        optField.setBoolean(null, true);
        BigDecimal resultOptTrue = num1.divide(num2, MathContext.DECIMAL128);

        optField.setBoolean(null, false);
        BigDecimal resultOptFalse = num1.divide(num2, MathContext.DECIMAL128);

        Asserts.assertEQ(resultOptTrue, resultOptFalse);
    }

    public static void main(String[] args) {
        try {
            Class<BigDecimal> clazz = BigDecimal.class;
            Field optField = clazz.getDeclaredField("opt");
            optField.setAccessible(true);

            SecureRandom random = new SecureRandom();
            for (int i = 0; i < TEST_ROUND; i++) {
                testDivision(random.nextDouble(), optField);
            }

            double[] specialCases = {0.00000000000000D, 123456789.123456789, -123456789.123456789,
                    1e10, 1e-010, -1e10, -1e-010,
                    Double.MAX_VALUE, Double.MIN_VALUE,
                    -Double.MAX_VALUE, -Double.MIN_VALUE};
            for (double specialCase: specialCases) {
                testDivision(specialCase, optField);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

