/*
 * Copyright (c) 2012, 2026, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8005471 8381379 8390388 8390380 8392519 8392377 8393344 8393337
 * @run main/othervm -Djava.locale.providers=CLDR CLDRDisplayNamesTest
 * @summary Make sure that localized time zone names of CLDR are used
 * if specified.
 */

import java.text.SimpleDateFormat;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.util.*;
import static java.util.TimeZone.*;

public class CLDRDisplayNamesTest {
    /*
     * The first element is a language tag. The rest are localized
     * display names of America/Los_Angeles copied from the CLDR
     * resources data. If data change in CLDR, test data below will
     * need to be changed accordingly.
     *
     * Generic names are NOT tested (until they are supported by API).
     */
    static final String[][] CLDR_DATA = {
        {
            "ja-JP",
            "\u30a2\u30e1\u30ea\u30ab\u592a\u5e73\u6d0b\u6a19\u6e96\u6642",
            "PST",
            "\u30a2\u30e1\u30ea\u30ab\u592a\u5e73\u6d0b\u590f\u6642\u9593",
            "PDT",
            //"\u30a2\u30e1\u30ea\u30ab\u592a\u5e73\u6d0b\u6642\u9593",
            //"PT"
        },
        {
            "zh-CN",
            "\u592a\u5e73\u6d0b\u6807\u51c6\u65f6\u95f4",
            "PST",
            "\u592a\u5e73\u6d0b\u590f\u4ee4\u65f6\u95f4",
            "PDT",
            //"\u7f8e\u56fd\u592a\u5e73\u6d0b\u65f6\u95f4",
            //"PT"
        },
        {
            "de-DE",
            "Nordamerikanische Westk\u00fcsten-Winterzeit",
            "PST",
            "Nordamerikanische Westk\u00fcsten-Sommerzeit",
            "PDT",
            //"Nordamerikanische Westk\u00fcstenzeit",
            //"PT"
        },
    };

    static final Object[][] EXPLICIT_DST_OFFSET_DATA = {
        {ZonedDateTime.of(2026, 4, 5, 0, 0, 0, 0, ZoneId.of("Europe/Dublin")), "Irish Summer Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("Europe/Dublin")), "Greenwich Mean Time"},
        {ZonedDateTime.of(2026, 4, 5, 0, 0, 0, 0, ZoneId.of("Eire")), "Irish Summer Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("Eire")), "Greenwich Mean Time"},
        {ZonedDateTime.of(2026, 4, 5, 0, 0, 0, 0, ZoneId.of("America/Vancouver")), "Pacific Daylight Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("America/Vancouver")), "Pacific Daylight Time"},
        {ZonedDateTime.of(2026, 1, 5, 0, 0, 0, 0, ZoneId.of("America/Edmonton")), "Mountain Standard Time"},
        {ZonedDateTime.of(2026, 7, 5, 0, 0, 0, 0, ZoneId.of("America/Edmonton")), "Mountain Daylight Time"},
        {ZonedDateTime.of(2026, 1, 5, 0, 0, 0, 0, ZoneId.of("America/Yellowknife")), "Mountain Standard Time"},
        {ZonedDateTime.of(2026, 7, 5, 0, 0, 0, 0, ZoneId.of("America/Yellowknife")), "Mountain Daylight Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("America/Inuvik")), "Mountain Daylight Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("America/Winnipeg")), "Central Daylight Time"},
        {ZonedDateTime.of(2026, 12, 5, 0, 0, 0, 0, ZoneId.of("America/Rainy_River")), "Central Daylight Time"}

    };

    public static void main(String[] args) {
        TimeZone tz = TimeZone.getTimeZone("America/Los_Angeles");
        int errors = 0;
        for (String[] data : CLDR_DATA) {
            Locale locale = Locale.forLanguageTag(data[0]);
            for (int i = 1; i < data.length; i++) {
                int style = ((i % 2) == 1) ? LONG : SHORT;
                boolean daylight = (i == 3 || i == 4);
                String name = tz.getDisplayName(daylight, style, locale);
                if (!data[i].equals(name)) {
                    System.err.printf("error: got '%s' expected '%s' (style=%d, daylight=%s, locale=%s)%n",
                                      name, data[i], style, daylight, locale);
                    errors++;
                }
            }
        }

        DateTimeFormatter dtf = DateTimeFormatter.ofPattern("zzzz", Locale.US);
        SimpleDateFormat sdf = new SimpleDateFormat("zzzz", Locale.US);
        for (Object[] data : EXPLICIT_DST_OFFSET_DATA) {
            ZonedDateTime zdt = (ZonedDateTime) data[0];
            String expected = (String) data[1];

            String actual = dtf.format(zdt);
            if (!expected.equals(actual)) {
                System.err.printf("error: java.time got '%s' expected '%s' (zone=%s, date=%s)%n",
                                  actual, expected, zdt.getZone(), zdt.toLocalDate());
                errors++;
            }

            sdf.setTimeZone(TimeZone.getTimeZone(zdt.getZone().getId()));
            actual = sdf.format(Date.from(zdt.toInstant()));
            if (!expected.equals(actual)) {
                System.err.printf("error: java.text got '%s' expected '%s' (zone=%s, date=%s)%n",
                                  actual, expected, zdt.getZone(), zdt.toLocalDate());
                errors++;
            }
        }

        if (errors > 0) {
            throw new RuntimeException("test failed");
        }
    }
}
