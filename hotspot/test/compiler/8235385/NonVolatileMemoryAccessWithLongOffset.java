/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates. All rights reserved.
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


import sun.misc.Unsafe;
import java.util.Random;
import java.math.BigDecimal;
import java.math.BigInteger;
import static java.lang.Math.abs;
import static java.lang.Math.pow;
import static java.lang.Math.round;
import static java.math.BigInteger.TEN;
import java.lang.reflect.Field;
import java.math.BigInteger;
import java.nio.ByteOrder;
import static java.lang.String.format;
import static java.lang.System.arraycopy;

/* @test
 * @bug 8235385
 * @summary Verifies non-volatile memory access with long offset
 * @requires os.arch == "aarch64"
 */
public class NonVolatileMemoryAccessWithLongOffset {
    private static final Unsafe unsafe;
    static Random random = new Random();

    static {
        Field f = null;
        try {
            f = Unsafe.class.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            unsafe = (Unsafe) f.get(null);
        } catch (ReflectiveOperationException e) {
            throw new Error(e);
        }
    }
    private static final int MAX_LENGTH = 10000;

    private static byte[] input0 = new byte[MAX_LENGTH];
    private static byte[] input1 = new byte[MAX_LENGTH];
    private static final int MAX_VALUE_LENGTH = random.nextInt(200) + 20;
    private static final byte[] maxValue = new byte[MAX_VALUE_LENGTH];
    private static final byte[] minValue = new byte[MAX_VALUE_LENGTH];
    private static final int DECIMAL_MAX_VALUE_LENGTH = MAX_VALUE_LENGTH - 15;
    private static byte[] byteArray;

    private static final    int valueType = random.nextInt(100);
    private static final    int numNulls = random.nextInt(100);
    private static final    int numRows = random.nextInt(100);
    private static final    int countDistinct = random.nextInt(100);
    private static final    long rawDataSize = (long)random.nextInt(100);
    private static final    long sum = (long)random.nextInt(1000);
    private static final    int version = random.nextInt(100);


    private static final    int dictOffset = random.nextInt(100);
    private static final    int dictLength = random.nextInt(100);
    private static final    int histOffset = random.nextInt(100);
    private static final    int histLength = random.nextInt(100);
    private static final    int dpnOffset = random.nextInt(100);
    private static final    int dpnCount = random.nextInt(100);

    private static final    long maxRowCount = (long)random.nextInt(1000);
    private static final    long minRowCount = (long)random.nextInt(1000);
    private static final    long totalRowCount = (long)random.nextInt(100);
    private static final    long maxMemSize = (long)random.nextInt(1000);
    private static final    long minMemSize = (long)random.nextInt(100);
    private static final    long totalMemSize = (long)random.nextInt(1000);

    private static final    long toastOffset = (long)random.nextInt(1000);
    private static final    boolean hasToast = random.nextInt(100) > 50;


    private static final int MAX_STRING_LENGTH = random.nextInt(300) + 20;
    private static final byte[] maxString = new byte[MAX_STRING_LENGTH];
    private static final byte[] minString = new byte[MAX_STRING_LENGTH];
    private static int maxStringLength = random.nextInt(100);
    private static int minStringLength = random.nextInt(100);
    private static final boolean maxStringIsNull = random.nextInt(100) > 20;
    private static final boolean minStringIsNull = random.nextInt(100) > 60;

    private static final short precision = (short)random.nextInt(100);
    private static final short scale = (short)random.nextInt(100);
    private static final boolean    useShortCompressFloat = random.nextInt(100) > 60;
    private static final int SHORT_SIZE = 2;
    private static final int CHAR_SIZE = 2;
    private static final int INT_SIZE = 4;
    private static final int LONG_SIZE = 8;
    private static final int FLOAT_SIZE = 4;
    private static final int DOUBLE_SIZE = 8;
    private static final int    BOOLEAN_SIZE = 1;

    private static void premitiveAssert(boolean flag) {
        if (flag == false) {
            throw new RuntimeException("overflow!");
        }
    }

    private static long BYTE_ARRAY_OFFSET = unsafe.ARRAY_BYTE_BASE_OFFSET;


    public static final void toBytes(short obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         SHORT_SIZE));
        unsafe.putShort(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(int obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         INT_SIZE));
        unsafe.putInt(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(long obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         LONG_SIZE));
        unsafe.putLong(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(float obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         FLOAT_SIZE));
        unsafe.putFloat(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(double obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         DOUBLE_SIZE));
        unsafe.putDouble(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(char obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         CHAR_SIZE));
        unsafe.putChar(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    public static final void toBytes(boolean obj, byte[] rawBytes, int start) {
        premitiveAssert(rawBytes.length >= (start +         1));
        unsafe.putBoolean(rawBytes, (long) BYTE_ARRAY_OFFSET + start, obj);
    }

    private static int bytesSize() {
        return MAX_LENGTH;
    }
    static byte[] toBytesDup() {
        byte[] rawBytes = input1;

        int offset = 0;

        toBytes(valueType, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(numNulls, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(numRows, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(countDistinct, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(rawDataSize, rawBytes, offset);
        offset +=         LONG_SIZE;

        toBytes(sum, rawBytes, offset);
        offset +=         LONG_SIZE;
        if (version > 50) {
            System.arraycopy(maxValue, 0, rawBytes, offset, MAX_VALUE_LENGTH);
            offset += MAX_VALUE_LENGTH;

            System.arraycopy(minValue, 0, rawBytes, offset, MAX_VALUE_LENGTH);
            offset += MAX_VALUE_LENGTH;
        } else {
            System.arraycopy(maxValue, 0, rawBytes, offset, DECIMAL_MAX_VALUE_LENGTH);
            offset += DECIMAL_MAX_VALUE_LENGTH;

            System.arraycopy(minValue, 0, rawBytes, offset, DECIMAL_MAX_VALUE_LENGTH);
            offset += DECIMAL_MAX_VALUE_LENGTH;
        }

        toBytes(dictOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dictLength, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(histOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(histLength, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dpnOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dpnCount, rawBytes, offset);
        offset +=         INT_SIZE;
        if (version >= 60) {
            toBytes(maxRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(minRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(totalRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(maxMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(minMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(totalMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;
        }

       if (version >= 65) {
           toBytes(toastOffset, rawBytes, offset);
           offset +=         LONG_SIZE;
           toBytes(hasToast, rawBytes, offset);
           offset +=         BOOLEAN_SIZE;
       }
       if (version >= 70) {
          System.arraycopy(maxString, 0, rawBytes, offset, MAX_STRING_LENGTH);
          offset += MAX_STRING_LENGTH;

          System.arraycopy(minString, 0, rawBytes, offset, MAX_STRING_LENGTH);
          offset += MAX_STRING_LENGTH;

          toBytes(maxStringLength, rawBytes, offset);
          offset +=         INT_SIZE;

          toBytes(minStringLength, rawBytes, offset);
          offset +=         INT_SIZE;

          toBytes(maxStringIsNull, rawBytes, offset);
          offset +=         BOOLEAN_SIZE;

          toBytes(minStringIsNull, rawBytes, offset);
          offset +=         BOOLEAN_SIZE;
       }

       if (version >= 75) {
           toBytes(precision, rawBytes, offset);
           offset +=         SHORT_SIZE;

           toBytes(scale, rawBytes, offset);
           offset +=         SHORT_SIZE;
       }

       if (version >= 80) {
           toBytes(useShortCompressFloat, rawBytes, offset);
           offset +=         BOOLEAN_SIZE;
       }
        return rawBytes;
    }


    static byte[] toBytes() {
        byte[] rawBytes = input0;

        int offset = 0;

        toBytes(valueType, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(numNulls, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(numRows, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(countDistinct, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(rawDataSize, rawBytes, offset);
        offset +=         LONG_SIZE;

        toBytes(sum, rawBytes, offset);
        offset +=         LONG_SIZE;

        //if (version.getVersion() < DetailColumn.VERSION_EIGHT) {
        if (version > 50) {
            System.arraycopy(maxValue, 0, rawBytes, offset, MAX_VALUE_LENGTH);
            offset += MAX_VALUE_LENGTH;

            System.arraycopy(minValue, 0, rawBytes, offset, MAX_VALUE_LENGTH);
            offset += MAX_VALUE_LENGTH;
        } else {
            System.arraycopy(maxValue, 0, rawBytes, offset, DECIMAL_MAX_VALUE_LENGTH);
            offset += DECIMAL_MAX_VALUE_LENGTH;

            System.arraycopy(minValue, 0, rawBytes, offset, DECIMAL_MAX_VALUE_LENGTH);
            offset += DECIMAL_MAX_VALUE_LENGTH;
        }

        toBytes(dictOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dictLength, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(histOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(histLength, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dpnOffset, rawBytes, offset);
        offset +=         INT_SIZE;

        toBytes(dpnCount, rawBytes, offset);
        offset +=         INT_SIZE;

        if (version >= 60) {
            toBytes(maxRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(minRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(totalRowCount, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(maxMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(minMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;

            toBytes(totalMemSize, rawBytes, offset);
            offset +=         LONG_SIZE;
        }

       if (version >= 65) {
           toBytes(toastOffset, rawBytes, offset);
           offset +=         LONG_SIZE;
           toBytes(hasToast, rawBytes, offset);
           offset +=         BOOLEAN_SIZE;
       }

       if (version >= 70) {
          System.arraycopy(maxString, 0, rawBytes, offset, MAX_STRING_LENGTH);
          offset += MAX_STRING_LENGTH;

          System.arraycopy(minString, 0, rawBytes, offset, MAX_STRING_LENGTH);
          offset += MAX_STRING_LENGTH;

          toBytes(maxStringLength, rawBytes, offset);
          offset +=         INT_SIZE;

          toBytes(minStringLength, rawBytes, offset);
          offset +=         INT_SIZE;

          toBytes(maxStringIsNull, rawBytes, offset);
          offset +=         BOOLEAN_SIZE;

          toBytes(minStringIsNull, rawBytes, offset);
          offset +=         BOOLEAN_SIZE;
       }

       if (version >= 75) {
           toBytes(precision, rawBytes, offset);
           offset +=         SHORT_SIZE;

           toBytes(scale, rawBytes, offset);
           offset +=         SHORT_SIZE;
       }

       if (version >= 80) {
           toBytes(useShortCompressFloat, rawBytes, offset);
           offset +=         BOOLEAN_SIZE;
       }
        return rawBytes;
    }

    public static void main(String[] args) throws Throwable {
        long s = 0, s1 = 0;
        for (int i = 0; i < input0.length; i++) {
            input0[i] = 0;
            input1[i] = 0;
        }
        for (int i = 0; i < 100000; i++) {
            s += toBytes()[0];
            s1 += toBytesDup()[0];
            for (int j = 0; j < input0.length; j++) {
                if (input0[j] != input1[j]) {
                    throw new RuntimeException("not match!");
                }
            }
        }
    }
}
