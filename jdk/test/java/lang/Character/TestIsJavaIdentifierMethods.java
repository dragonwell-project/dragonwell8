/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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
 * @summary Test behavior of isJavaIdentifierXX, testIsJavaLetter, and
 *  testIsJavaLetterOrDigit methods for all code points.
 * @bug 8218915
 */

public class TestIsJavaIdentifierMethods {
    // Unassigned code points not present in Unicode 6.2 (which Java SE 8
    // is based upon), including: various currency symbol sign code points
    // (Nordic Mark ... Bitcoin), Japanese Era Square character code point,
    // and 35 CJK Unified Ideograph code points from GB18030-2022
    private static final int CS_SIGNS_CODEPOINT_START = 0x20BB;
    private static final int CS_SIGNS_CODEPOINT_END = 0x20BF;
    private static final int JAPANESE_ERA_CODEPOINT = 0x32FF;
    private static final int GB18030_2022_CODEPOINT_START = 0x9FCD;
    private static final int GB18030_2022_CODEPOINT_END = 0x9FEF;

    public static void main(String[] args) {
        testIsJavaIdentifierPart_int();
        testIsJavaIdentifierPart_char();
        testIsJavaIdentifierStart_int();
        testIsJavaIdentifierStart_char();
        testIsJavaLetter();
        testIsJavaLetterOrDigit();
    }

    /**
     * Assertion testing for public static boolean isJavaIdentifierPart(int
     * codePoint), A character may be part of a Java identifier if any of the
     * following are true:
     * <ul>
     * <li>it is a letter</li>
     * <li>it is a currency symbol (such as <code>'$'</code>)</li>
     * <li>it is a connecting punctuation character (such as <code>'_'</code>)
     * </li>
     * <li>it is a digit</li>
     * <li>it is a numeric letter (such as a Roman numeral character)</li>
     * <li>it is a combining mark</li>
     * <li>it is a non-spacing mark</li>
     * <li><code>isIdentifierIgnorable</code> returns <code>true</code> for the
     * character</li>
     * </ul>
     * All code points from (0x0000..0x10FFFF) are tested.
     */
    public static void testIsJavaIdentifierPart_int() {
        for (int cp = 0; cp <= Character.MAX_CODE_POINT; cp++) {
            boolean expected = false;
            // Since Character.isJavaIdentifierPart(int) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (cp != JAPANESE_ERA_CODEPOINT &&
                    !(cp >= CS_SIGNS_CODEPOINT_START && cp <= CS_SIGNS_CODEPOINT_END) &&
                    !(cp >= GB18030_2022_CODEPOINT_START && cp <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(cp);
                expected = Character.isLetter(cp)
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION
                        || Character.isDigit(cp)
                        || type == Character.LETTER_NUMBER
                        || type == Character.COMBINING_SPACING_MARK
                        || type == Character.NON_SPACING_MARK
                        || Character.isIdentifierIgnorable(cp);
            }

            if (Character.isJavaIdentifierPart(cp) != expected) {
                throw new RuntimeException(
                   "Character.isJavaIdentifierPart(int) failed for codepoint "
                                + Integer.toHexString(cp));
            }
        }
    }

    /**
     * Assertion testing for public static boolean isJavaIdentifierPart(char
     * ch), A character may be part of a Java identifier if any of the
     * following are true:
     * <ul>
     * <li>it is a letter;
     * <li>it is a currency symbol (such as "$");
     * <li>it is a connecting punctuation character (such as "_");
     * <li>it is a digit;
     * <li>it is a numeric letter (such as a Roman numeral character);
     * <li>it is a combining mark;
     * <li>it is a non-spacing mark;
     * <li>isIdentifierIgnorable returns true for the character.
     * </ul>
     * All Unicode code points in the BMP (0x0000..0xFFFF) are tested.
     */
    public static void testIsJavaIdentifierPart_char() {
        for (int i = 0; i <= Character.MAX_VALUE; ++i) {
            char ch = (char) i;
            boolean expected = false;
            // Since Character.isJavaIdentifierPart(char) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (i != JAPANESE_ERA_CODEPOINT &&
                    !(i >= CS_SIGNS_CODEPOINT_START && i <= CS_SIGNS_CODEPOINT_END) &&
                    !(i >= GB18030_2022_CODEPOINT_START && i <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(ch);
                expected = Character.isLetter(ch)
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION
                        || Character.isDigit(ch)
                        || type == Character.LETTER_NUMBER
                        || type == Character.COMBINING_SPACING_MARK
                        || type == Character.NON_SPACING_MARK
                        || Character.isIdentifierIgnorable(ch);
            }

            if (Character.isJavaIdentifierPart((char) i) != expected) {
                throw new RuntimeException(
                "Character.isJavaIdentifierPart(char) failed for codepoint "
                                + Integer.toHexString(i));
            }
        }
    }

    /**
     * Assertion testing for public static boolean isJavaIdentifierStart(int
     * codePoint), A character may start a Java identifier if and only if it is
     * one of the following:
     * <ul>
     * <li>it is a letter;</li>
     * <li>getType(ch) returns LETTER_NUMBER;</li>
     * <li>it is a currency symbol (such as "$");</li>
     * <li>it is a connecting punctuation character (such as "_");</li>
     * </ul>
     * All Code points from (0x0000..0x10FFFF) are tested.
     */
    public static void testIsJavaIdentifierStart_int() {
        for (int cp = 0; cp <= Character.MAX_CODE_POINT; cp++) {
            boolean expected = false;
            // Since Character.isJavaIdentifierStart(int) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (cp != JAPANESE_ERA_CODEPOINT &&
                    !(cp >= CS_SIGNS_CODEPOINT_START && cp <= CS_SIGNS_CODEPOINT_END) &&
                    !(cp >= GB18030_2022_CODEPOINT_START && cp <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(cp);
                expected = Character.isLetter(cp)
                        || type == Character.LETTER_NUMBER
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION;
            }

            if (Character.isJavaIdentifierStart(cp) != expected) {
                throw new RuntimeException(
                "Character.isJavaIdentifierStart(int) failed for codepoint "
                                + Integer.toHexString(cp));
            }
        }
    }

    /**
     * Assertion testing for public static boolean isJavaIdentifierStart(char),
     * A character may start a Java identifier if and only if it is
     * one of the following:
     * <ul>
     * <li>it is a letter;</li>
     * <li>getType(ch) returns LETTER_NUMBER;</li>
     * <li>it is a currency symbol (such as "$");</li>
     * <li>it is a connecting punctuation character (such as "_");</li>
     * </ul>
     * All Unicode code points in the BMP (0x0000..0xFFFF) are tested.
     */
    public static void testIsJavaIdentifierStart_char() {
        for (int i = 0; i <= Character.MAX_VALUE; i++) {
            char ch = (char) i;
            boolean expected = false;
            // Since Character.isJavaIdentifierStart(char) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (i != JAPANESE_ERA_CODEPOINT &&
                    !(i >= CS_SIGNS_CODEPOINT_START && i <= CS_SIGNS_CODEPOINT_END) &&
                    !(i >= GB18030_2022_CODEPOINT_START && i <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(ch);
                expected = Character.isLetter(ch)
                        || type == Character.LETTER_NUMBER
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION;
            }

            if (Character.isJavaIdentifierStart(ch) != expected) {
                throw new RuntimeException(
                "Character.isJavaIdentifierStart(char) failed for codepoint "
                                + Integer.toHexString(i));
            }
        }
    }

    /**
     * Assertion testing for public static boolean isJavaLetter(char ch), A
     * character may start a Java identifier if and only if one of the
     * following is true:
     * <ul>
     * <li>isLetter(ch) returns true
     * <li>getType(ch) returns LETTER_NUMBER
     * <li>ch is a currency symbol (such as "$")
     * <li>ch is a connecting punctuation character (such as "_").
     * </ul>
     * All Unicode code points in the BMP (0x0000..0xFFFF) are tested.
     */
    public static void testIsJavaLetter() {
        for (int i = 0; i <= Character.MAX_VALUE; ++i) {
            char ch = (char) i;
            boolean expected = false;
            // Since Character.isJavaLetter(char) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (i != JAPANESE_ERA_CODEPOINT &&
                    !(i >= CS_SIGNS_CODEPOINT_START && i <= CS_SIGNS_CODEPOINT_END) &&
                    !(i >= GB18030_2022_CODEPOINT_START && i <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(ch);
                expected = Character.isLetter(ch)
                        || type == Character.LETTER_NUMBER
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION;
            }

            if (Character.isJavaLetter(ch) != expected) {
                throw new RuntimeException(
                        "Character.isJavaLetter(ch) failed for codepoint "
                                + Integer.toHexString(i));
            }
        }
    }

    /**
     * Assertion testing for public static boolean isJavaLetterOrDigit(char
     * ch), A character may be part of a Java identifier if and only if any
     *  of the following are true:
     * <ul>
     * <li>it is a letter
     * <li>it is a currency symbol (such as '$')
     * <li>it is a connecting punctuation character (such as '_')
     * <li>it is a digit
     * <li>it is a numeric letter (such as a Roman numeral character)
     * <li>it is a combining mark
     * <li>it is a non-spacing mark
     * <li>isIdentifierIgnorable returns true for the character.
     * </ul>
     * All Unicode code points in the BMP (0x0000..0xFFFF) are tested.
     */
    public static void testIsJavaLetterOrDigit() {
        for (int i = 0; i <= Character.MAX_VALUE; ++i) {
            char ch = (char) i;
            boolean expected = false;
            // Since Character.isJavaLetterOrDigit(char) strictly conforms to
            // character information from version 6.2 of the Unicode Standard,
            // check if code point is one of the extra unassigned
            // code points (defined at the beginning of the file). If the code
            // point is found to be one of the unassigned code points,
            // value of variable "expected" is considered false.
            if (i != JAPANESE_ERA_CODEPOINT &&
                    !(i >= CS_SIGNS_CODEPOINT_START && i <= CS_SIGNS_CODEPOINT_END) &&
                    !(i >= GB18030_2022_CODEPOINT_START && i <= GB18030_2022_CODEPOINT_END)) {
                byte type = (byte) Character.getType(ch);
                expected = Character.isLetter(ch)
                        || type == Character.CURRENCY_SYMBOL
                        || type == Character.CONNECTOR_PUNCTUATION
                        || Character.isDigit(ch)
                        || type == Character.LETTER_NUMBER
                        || type == Character.COMBINING_SPACING_MARK
                        || type == Character.NON_SPACING_MARK
                        || Character.isIdentifierIgnorable(ch);
            }

            if (Character.isJavaLetterOrDigit(ch) != expected) {
                throw new RuntimeException(
                  "Character.isJavaLetterOrDigit(ch) failed for codepoint "
                                + Integer.toHexString(i));
            }
        }
    }
}
