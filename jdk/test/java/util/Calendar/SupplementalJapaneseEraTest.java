/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8054214 8173423 8177776
 * @summary Create an equivalent test case for JDK9's SupplementalJapaneseEraTest
 * @library /lib/testlibrary
 * @run testng SupplementalJapaneseEraTest
 */

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.BasicFileAttributes;
import java.text.SimpleDateFormat;
import java.time.chrono.JapaneseChronology;
import java.time.chrono.JapaneseDate;
import java.time.chrono.JapaneseEra;
import java.time.format.DateTimeFormatterBuilder;
import java.time.format.TextStyle;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import static java.util.GregorianCalendar.*;
import java.util.Locale;
import java.util.Properties;
import java.util.TimeZone;

import jdk.testlibrary.ProcessTools;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.Test;
import static org.testng.Assert.*;

public class SupplementalJapaneseEraTest {
    private static final Locale WAREKI_LOCALE = Locale.forLanguageTag("ja-JP-u-ca-japanese");
    private static final String SUP_ERA_NAME = "SupEra";
    private static final String SUP_ERA_ABBR = "S.E.";
    private static final int NEW_ERA_YEAR = 200;
    private static final int NEW_ERA_MONTH = FEBRUARY;
    private static final int NEW_ERA_DAY = 11;
    private static int errors = 0;

    @BeforeTest
    public void prepareTestJDK() throws IOException {
        Path src = Paths.get(System.getProperty("test.jdk")).toAbsolutePath();
        Path dst = Paths.get("testjava").toAbsolutePath();
        Files.walkFileTree(src, new CopyFileVisitor(src, dst));
    }

    @Test
    public void invokeTestJDK() throws Throwable {
        assertTrue(ProcessTools.executeCommand(
            Paths.get("testjava", "bin", "java").toAbsolutePath().toString(),
            "-cp",
            System.getProperty("test.classes"),
            "SupplementalJapaneseEraTest")
            .getExitValue() == 0);
    }

    public static void main(String[] args) {
        testProperty();

        if (errors != 0) {
            throw new RuntimeException("test failed");
        }
    }

    // copied from JDK9's test
    private static void testProperty() {
        Calendar jcal = new Calendar.Builder()
            .setCalendarType("japanese")
            .setFields(ERA, 6, YEAR, 1, DAY_OF_YEAR, 1)
            .build();
        Date firstDayOfEra = jcal.getTime();

        jcal.set(ERA, jcal.get(ERA) - 1); // previous era
        jcal.set(YEAR, 1);
        jcal.set(DAY_OF_YEAR, 1);
        Calendar cal = new GregorianCalendar();
        cal.setTimeInMillis(jcal.getTimeInMillis());
        cal.add(YEAR, 199);
        int year = cal.get(YEAR);

        SimpleDateFormat sdf;
        String expected, got;

        // test long era name
        sdf = new SimpleDateFormat("GGGG y-MM-dd", WAREKI_LOCALE);
        got = sdf.format(firstDayOfEra);
        expected = SUP_ERA_NAME + " 1-02-11";
        if (!expected.equals(got)) {
            System.err.printf("GGGG y-MM-dd: got=\"%s\", expected=\"%s\"%n", got, expected);
            errors++;
        }

        // test era abbreviation
        sdf = new SimpleDateFormat("G y-MM-dd", WAREKI_LOCALE);
        got = sdf.format(firstDayOfEra);
        expected = SUP_ERA_ABBR+" 1-02-11";
        if (!expected.equals(got)) {
            System.err.printf("G y-MM-dd: got=\"%s\", expected=\"%s\"%n", got, expected);
            errors++;
        }

        // confirm the gregorian year
        sdf = new SimpleDateFormat("y", Locale.US);
        int y = Integer.parseInt(sdf.format(firstDayOfEra));
        if (y != year) {
            System.err.printf("Gregorian year: got=%d, expected=%d%n", y, year);
            errors++;
        }

        // test java.time.chrono.JapaneseEra
        JapaneseDate jdate = JapaneseDate.of(year, 2, 11);
        got = jdate.toString();
        expected = "Japanese " + SUP_ERA_NAME + " 1-02-11";
        if (!expected.equals(got)) {
            System.err.printf("JapaneseDate: got=\"%s\", expected=\"%s\"%n", got, expected);
            errors++;
        }
        JapaneseEra jera = jdate.getEra();
        got = jera.getDisplayName(TextStyle.FULL, Locale.US);
        if (!SUP_ERA_NAME.equals(got)) {
            System.err.printf("JapaneseEra (FULL): got=\"%s\", expected=\"%s\"%n", got, SUP_ERA_NAME);
            errors++;
        }
        got = jera.getDisplayName(TextStyle.SHORT, Locale.US);
        if (!SUP_ERA_NAME.equals(got)) {
            System.err.printf("JapaneseEra (SHORT): got=\"%s\", expected=\"%s\"%n", got, SUP_ERA_NAME);
            errors++;
        }
        got = jera.getDisplayName(TextStyle.NARROW, Locale.US);
        if (!SUP_ERA_ABBR.equals(got)) {
            System.err.printf("JapaneseEra (NARROW): got=\"%s\", expected=\"%s\"%n", got, SUP_ERA_ABBR);
            errors++;
        }
        got = jera.getDisplayName(TextStyle.NARROW_STANDALONE, Locale.US);
        if (!SUP_ERA_ABBR.equals(got)) {
            System.err.printf("JapaneseEra (NARROW_STANDALONE): got=\"%s\", expected=\"%s\"%n", got, SUP_ERA_ABBR);
            errors++;
        }

        // test long/abbreviated names with java.time.format
        got = new DateTimeFormatterBuilder()
            .appendPattern("GGGG")
            .appendLiteral(" ")
            .appendPattern("G")
            .appendLiteral(" ")
            .appendPattern("GGGGG")
            .toFormatter(Locale.US)
            .withChronology(JapaneseChronology.INSTANCE)
            .format(jdate);
        expected = SUP_ERA_NAME + " " + SUP_ERA_NAME + " " + SUP_ERA_ABBR;
        if (!expected.equals(got)) {
            System.err.printf("java.time formatter long/abbr names: got=\"%s\", expected=\"%s\"%n", got, expected);
            errors++;
        }
    }

    private static class CopyFileVisitor extends SimpleFileVisitor<Path> {
        private final Path copyFrom;
        private final Path copyTo;
        private final Path calProps = Paths.get("calendars.properties");
        private final String JA_CAL_KEY = "calendar.japanese.eras";

        public CopyFileVisitor(Path copyFrom, Path copyTo) {
            this.copyFrom = copyFrom;
            this.copyTo = copyTo;
        }

        @Override
        public FileVisitResult preVisitDirectory(Path file,
                BasicFileAttributes attrs) throws IOException {
            Path relativePath = copyFrom.relativize(file);
            Path destination = copyTo.resolve(relativePath);
            if (!destination.toFile().exists()) {
                Files.createDirectories(destination);
            }
            return FileVisitResult.CONTINUE;
        }

        @Override
        public FileVisitResult visitFile(Path file,
                BasicFileAttributes attrs) throws IOException {
            if (!file.toFile().isFile()) {
                return FileVisitResult.CONTINUE;
            }
            Path relativePath = copyFrom.relativize(file);
            Path destination = copyTo.resolve(relativePath);
            if (relativePath.getFileName().equals(calProps)) {
                modifyCalendarProperties(file, destination);
            } else {
                Files.copy(file, destination, StandardCopyOption.COPY_ATTRIBUTES);
            }
            return FileVisitResult.CONTINUE;
        }

        private void modifyCalendarProperties(Path src, Path dst) throws IOException {
            Properties p = new Properties();
            try (BufferedReader br = Files.newBufferedReader(src)) {
                p.load(br);
            }

            String eras = p.getProperty(JA_CAL_KEY);
            if (eras != null) {
                p.setProperty(JA_CAL_KEY,
                    eras +
                    "; name=" + SupplementalJapaneseEraTest.SUP_ERA_NAME +
                    ",abbr=" + SupplementalJapaneseEraTest.SUP_ERA_ABBR +
                    ",since=" + since());
            }
            try (BufferedWriter bw = Files.newBufferedWriter(dst)) {
                p.store(bw, "test calendars.properties for supplemental Japanese era");
            }
        }

        private long since() {
            return new Calendar.Builder()
                .setCalendarType("japanese")
                .setTimeZone(TimeZone.getTimeZone("GMT"))
                .setFields(ERA, 5)
                .setDate(SupplementalJapaneseEraTest.NEW_ERA_YEAR,
                    SupplementalJapaneseEraTest.NEW_ERA_MONTH,
                    SupplementalJapaneseEraTest.NEW_ERA_DAY)
                .build()
                .getTimeInMillis();
        }
    }
}
