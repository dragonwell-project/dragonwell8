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

/*
 * @test
 * @bug 8202088 8207152 8217609 8219890
 * @summary Test the localized Japanese new era name (May 1st. 2019-)
 *      is retrieved no matter CLDR provider contains the name or not.
 * @run main/othervm -Djava.locale.providers=CLDR JapaneseEraNameTest
 *
 */

import static java.util.Calendar.*;
import static java.util.Locale.*;
import java.util.Calendar;
import java.util.Locale;


public class JapaneseEraNameTest {
    static final Calendar c = new Calendar.Builder()
            .setCalendarType("japanese")
            .setFields(ERA, 5, YEAR, 1, MONTH, MAY, DAY_OF_MONTH, 1)
            .build();


    static final Object[][] names = {
            // Since the test fails for below particular data
            // on prior 8u versions for all eras, commenting it
            // temporarily. Will be fixed as part of JDK-8220020.
            // { LONG,     JAPAN,   "\u4ee4\u548c" },
            { LONG,     US,      "Reiwa" },
            { LONG,     CHINA,   "Reiwa" },
            { SHORT,    JAPAN,   "\u4ee4\u548c" },
            { SHORT,    US,      "Reiwa" },
            { SHORT,    CHINA,   "R" },
        };

    public static void main(String[] args) {
        for (Object[] data : names) {
            if(!c.getDisplayName(ERA, (int)data[0], (Locale)data[1])
            .equals(data[2])) {
                throw new RuntimeException("JapaneseEraNameTest failed for " +
                  String.format("%1$s %2$s %3$s", data[0], data[1], data[2]));
            }
        }
    }
}
