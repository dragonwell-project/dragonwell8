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
 * This file is available under and governed by the GNU General Public
 * License version 2 only, as published by the Free Software Foundation.
 * However, the following notice accompanied the original version of this
 * file:
 *
 * Copyright (c) 2009-2012, Stephen Colebourne & Michael Nascimento Santos
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of JSR-310 nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * @test
 * @bug 8042131 8210633
 */

package test.java.time.format;

import java.time.chrono.ChronoLocalDate;
import java.time.chrono.IsoChronology;
import java.time.chrono.JapaneseChronology;
import java.time.chrono.JapaneseEra;
import java.time.chrono.MinguoChronology;
import java.time.chrono.ThaiBuddhistChronology;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeFormatterBuilder;
import java.time.format.ResolverStyle;
import java.time.temporal.ChronoField;
import java.time.temporal.TemporalAccessor;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

import static org.testng.Assert.assertEquals;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;

/**
 * Test TestDateTimeFormatterBuilderWithLocale.
 */
@Test
public class TestDateTimeFormatterBuilderWithLocale {

    private DateTimeFormatterBuilder builder;

    @BeforeMethod
    public void setUp() {
        builder = new DateTimeFormatterBuilder();
    }

    @DataProvider(name="mapTextLookup")
    Object[][] data_mapTextLookup() {
        return new Object[][] {
            {IsoChronology.INSTANCE.date(1, 1, 1), Locale.ENGLISH},
            {JapaneseChronology.INSTANCE.date(JapaneseEra.HEISEI, 1, 1, 8), Locale.ENGLISH},
            {MinguoChronology.INSTANCE.date(1, 1, 1), Locale.ENGLISH},
            {ThaiBuddhistChronology.INSTANCE.date(1, 1, 1), Locale.ENGLISH},
        };
    }

    @Test(dataProvider="mapTextLookup")
    public void test_appendText_mapTextLookup(ChronoLocalDate date, Locale locale) {
        final String firstYear = "firstYear";
        final String firstMonth = "firstMonth";
        final String firstYearMonth = firstYear + firstMonth;
        final long first = 1L;

        Map<Long, String> yearMap = new HashMap<Long, String>();
        yearMap.put(first, firstYear);

        Map<Long, String> monthMap = new HashMap<Long, String>();
        monthMap.put(first, firstMonth);

        DateTimeFormatter formatter = builder
            .appendText(ChronoField.YEAR_OF_ERA, yearMap)
            .appendText(ChronoField.MONTH_OF_YEAR, monthMap)
            .toFormatter(locale)
            .withResolverStyle(ResolverStyle.STRICT);

        assertEquals(date.format(formatter), firstYearMonth);

        TemporalAccessor ta = formatter.parse(firstYearMonth);
        assertEquals(ta.getLong(ChronoField.YEAR_OF_ERA), first);
        assertEquals(ta.getLong(ChronoField.MONTH_OF_YEAR), first);
    }
}
