/*
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
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

/* @test
   @bug 6371437 6371422 6371416 6371619 5058184 6371431 6639450 6569191 6577466 8235834
 */

import java.io.*;
import java.nio.*;
import java.nio.charset.*;
import java.util.Arrays;

public class TestIBMBugs {

    private static void bug6371437() throws Exception {
        CharsetEncoder converter = Charset.forName("Cp933").newEncoder();
        converter = converter.onMalformedInput(CodingErrorAction.REPORT);
        converter = converter.onUnmappableCharacter(CodingErrorAction.REPORT);
        CharBuffer in = CharBuffer.wrap(new char[] { (char)4352 });
        try {
              ByteBuffer out = converter.encode(in);
        } catch (CharacterCodingException e) { }
    }

    private static void bug6371422() throws Exception {
        String[] charsets = { "Cp949", "Cp949C" };
        for (int n = 0; n < charsets.length; n++) {
            String charset = charsets[n];
            CharsetEncoder converter = Charset.forName(charset).newEncoder();
            converter = converter.onMalformedInput(CodingErrorAction.REPORT);
            converter = converter.onUnmappableCharacter(CodingErrorAction.REPORT);
            int errors = 0;
            for (int i = 1; i < 0x1ffff; i++) {
                if (i >= 0x1100 && i <= 0x11f9)
                    continue;  //Dont try leading consonant, vowel and trailing
                               //consonant as a single char
                char[] in = (i < 0x10000
                         ? new char[] { (char)i }
                             : new char[] { (char)(0xd800 + ((i - 0x10000) >> 10)),
                              (char)(0xdc00 + ((i - 0x10000) & 0x3ff)) });

                try {
                    ByteBuffer out = converter.encode(CharBuffer.wrap(in));
                    if (out.remaining() == 0 ||
                        (out.remaining() == 1 && out.get(0) == 0x00)) {
                    errors++;
                    }
                } catch (CharacterCodingException e) { }
            }
            if (errors > 0)
                throw new Exception("Charset "+charset+": "+errors+" errors");
        }
    }

    private static void bug6371416() throws Exception {
        String[] charsets = { "Cp933", "Cp949", "Cp949C", "Cp970"};
        for (int n = 0; n < charsets.length; n++) {
            String charset = charsets[n];
            CharsetEncoder converter = Charset.forName(charset).newEncoder();
            converter = converter.onMalformedInput(CodingErrorAction.REPORT);
            converter = converter.onUnmappableCharacter(CodingErrorAction.REPORT);
            int errors = 0;
            for (int i = 0xd800; i < 0xe000; i++) {
                char[] in = new char[] { (char)i };
                try {
                    ByteBuffer out = converter.encode(CharBuffer.wrap(in));
                    if (out.remaining() == 0)
                        errors++;
                } catch (CharacterCodingException e) { }
            }
            if (errors > 0)
                throw new Exception("Charset "+charset+": "+errors+" errors");
        }
    }

    private static void bug6371619() throws Exception {
        String encoding = "Cp964";
        Charset charset = Charset.forName(encoding);
        CharsetDecoder converter = charset.newDecoder();
        converter = converter.onMalformedInput(CodingErrorAction.REPORT);
        converter = converter.onUnmappableCharacter(CodingErrorAction.REPORT);
        int errors = 0;
        for (int b = 0x80; b < 0x100; b++)
            if (!(b == 0x8e ||  // 0x8e is a SS2
                  (b >= 0x80 && b <= 0x8d) || (b >= 0x90 && b <= 0x9f))) {
                ByteBuffer in = ByteBuffer.wrap(new byte[] { (byte)b });
                try {
                    CharBuffer out = converter.decode(in);
                    if (out.length() == 0) {
                        errors++;
                    }
                } catch (CharacterCodingException e) { }
            }
        if (errors > 0)
            throw new Exception("Charset "+charset+": "+errors+" errors");
    }


    private static void bug6371431() throws Exception {
        String encoding = "Cp33722";
        Charset charset = Charset.forName(encoding);
        CharsetDecoder converter = charset.newDecoder();
        converter = converter.onMalformedInput(CodingErrorAction.REPORT);
        converter = converter.onUnmappableCharacter(CodingErrorAction.REPORT);
        int errors = 0;
        for (int b = 0xa0; b < 0x100; b++) {
            ByteBuffer in = ByteBuffer.wrap(new byte[] { (byte)b });
            try {
                CharBuffer out = converter.decode(in);
                if (out.length() == 0) {
                    errors++;
                }
            } catch (CharacterCodingException e) { }
        }
        if (errors > 0)
            throw new Exception("Charset "+charset+": "+errors+" errors");
    }

    private static void bug6639450 () throws Exception {
        byte[] bytes1 = "\\".getBytes("IBM949");
        "\\".getBytes("IBM949C");
        byte[] bytes2 = "\\".getBytes("IBM949");
        if (bytes1.length != 1 || bytes2.length != 1 ||
            bytes1[0] != (byte)0x82 ||
            bytes2[0] != (byte)0x82)
        throw new Exception("IBM949/IBM949C failed");
    }

    private static void bug6569191 () throws Exception {
        byte[] bs = new byte[] { (byte)0x81, (byte)0xad,  // fffd ff6d
                                 (byte)0x81, (byte)0xae,  // fffd ff6e
                                 (byte)0x81, (byte)0xaf,  // fffd ff6f
                                 (byte)0x81, (byte)0xb0,  // fffd ff70
                                 (byte)0x85, (byte)0x81,  // fffd ->
                                 (byte)0x85, (byte)0x87,  // 2266 ->
                                 (byte)0x85, (byte)0xe0,  // 32a4 ->
                                 (byte)0x85, (byte)0xf0 };// 7165 fffd
        String s = new String(bs, "Cp943");
        // see DoubleByte for how the unmappables are handled
        if (!"\ufffd\uff6d\ufffd\uff6e\ufffd\uff6f\ufffd\uff70\ufffd\u2266\u32a4\u7165\ufffd"
            .equals(s))
            throw new Exception("Cp943 failed");
    }


    private static void bug6577466 () throws Exception {
        for (int c = Character.MIN_VALUE; c <= Character.MAX_VALUE; c++){
            if (!Character.isDefined((char)c)) continue;
            String s = String.valueOf((char)c);
            byte[] bb = null;
            bb = s.getBytes("x-IBM970");
        }
    }

    // Following test data is for 8235834
    private static final byte[] byteIBM943c2b = new byte[] {
        (byte)0x81, (byte)0x5C, (byte)0x81, (byte)0x60,
        (byte)0x81, (byte)0x61, (byte)0x81, (byte)0x7C,
        (byte)0x88, (byte)0xA0, (byte)0x89, (byte)0x8B,
        (byte)0x89, (byte)0xA8, (byte)0x8A, (byte)0x9A,
        (byte)0x8B, (byte)0xA0, (byte)0x8B, (byte)0xEB,
        (byte)0x8C, (byte)0x71, (byte)0x8C, (byte)0x74,
        (byte)0x8C, (byte)0xB2, (byte)0x8D, (byte)0x8D,
        (byte)0x8D, (byte)0xF2, (byte)0x8E, (byte)0xC6,
        (byte)0x8F, (byte)0x4A, (byte)0x8F, (byte)0xD3,
        (byte)0x8F, (byte)0xDD, (byte)0x90, (byte)0xE4,
        (byte)0x91, (byte)0x7E, (byte)0x91, (byte)0x89,
        (byte)0x91, (byte)0xCB, (byte)0x92, (byte)0x5C,
        (byte)0x92, (byte)0xCD, (byte)0x93, (byte)0x55,
        (byte)0x93, (byte)0x5E, (byte)0x93, (byte)0x98,
        (byte)0x93, (byte)0xC0, (byte)0x94, (byte)0x58,
        (byte)0x94, (byte)0x8D, (byte)0x94, (byte)0xAC,
        (byte)0x94, (byte)0xAE, (byte)0x96, (byte)0x6A,
        (byte)0x96, (byte)0xCB, (byte)0x97, (byte)0x89,
        (byte)0x98, (byte)0x58, (byte)0x9B, (byte)0xA0,
        (byte)0x9D, (byte)0xB7, (byte)0x9E, (byte)0x94,
        (byte)0xE3, (byte)0x79, (byte)0xE4, (byte)0x45,
        (byte)0xE8, (byte)0xF6, (byte)0xFA, (byte)0x55,
        (byte)0xFA, (byte)0x59,
    };

    private static final String strIBM943c2b1 =
        "\u2015\uFF5E\u2225\uFF0D\u555E\u7130\u9DD7\u5699" +
        "\u4FE0\u8EC0\u7E6B\u8346\u9E7C\u9EB4\u6805\u5C62" +
        "\u7E61\u8523\u91AC\u87EC\u6414\u7626\u9A52\u7C1E" +
        "\u6451\u5861\u985A\u79B1\u7006\u56CA\u525D\u6F51" +
        "\u91B1\u9830\u9EB5\u840A\u881F\u5C5B\u6522\u688E" +
        "\u7E48\u8141\u9839\uFFE4\uF86F";

    private static final String strIBM943c2b2 =
        "\u2014\u301C\u2016\u2212\u5516\u7114\u9D0E\u565B" +
        "\u4FA0\u8EAF\u7E4B\u834A\u9E78\u9EB9\u67F5\u5C61" +
        "\u7E4D\u848B\u91A4\u8749\u63BB\u75E9\u9A28\u7BAA" +
        "\u63B4\u586B\u985B\u7977\u6D9C\u56A2\u5265\u6E8C" +
        "\u9197\u982C\u9EBA\u83B1\u874B\u5C4F\u6505\u688D" +
        "\u7E66\u80FC\u983D\u00A6\u2116";

    private static void bug8235834 () throws Exception {
        // 8235834 affects IBM-943 and IBM-943C encoder.
        // The decoded results of the corresponding characters of IBM-943
        // and IBM-943C is the same.
        for (String csName : new String[] {"x-IBM943", "x-IBM943C"}) {
            Charset cs = Charset.forName(csName);
            if (!Arrays.equals(byteIBM943c2b, strIBM943c2b1.getBytes(cs))) {
                throw new Exception(csName+" failed to encode");
            }
            if (!strIBM943c2b2.equals(new String(byteIBM943c2b, cs))) {
                throw new Exception(csName+" failed to round-trip conversion");
            }
        }
    }
    public static void main (String[] args) throws Exception {
        bug6577466();
        // need to be tested before any other IBM949C test case
        bug6639450();
        bug6371437();
        bug6371422();
        bug6371416();
        bug6371619();
        bug6371431();
        bug6569191();
        bug8235834();
    }
}
