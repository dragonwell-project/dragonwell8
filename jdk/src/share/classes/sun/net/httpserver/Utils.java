/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package sun.net.httpserver;

/**
 * Provides utility methods for checking header field names and quoted strings.
 */
public class Utils {

    /* Throw IAE if illegal character found.  isValue is true if String is
     * a value. Otherwise it is header name
     */
    public static void checkHeader(String str, boolean isValue) {
        int len = str.length();
        for (int i=0; i<len; i++) {
            char c = str.charAt(i);
            if (c == '\r') {
                if (!isValue) {
                    throw new IllegalArgumentException("Illegal CR found in header");
                }
                // is allowed if it is followed by \n and a whitespace char
                if (i >= len - 2) {
                    throw new IllegalArgumentException("Illegal CR found in header");
                }
                char c1 = str.charAt(i+1);
                char c2 = str.charAt(i+2);
                if (c1 != '\n') {
                    throw new IllegalArgumentException("Illegal char found after CR in header");
                }
                if (c2 != ' ' && c2 != '\t') {
                    throw new IllegalArgumentException("No whitespace found after CRLF in header");
                }
                i+=2;
            } else if (c == '\n') {
                throw new IllegalArgumentException("Illegal LF found in header");
            } else if (c > 255) {
                throw new IllegalArgumentException("Illegal character found in header");
            }
        }
    }

}
