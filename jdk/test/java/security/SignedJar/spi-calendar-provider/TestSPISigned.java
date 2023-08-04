/*
 * Copyright (c) 2022, Red Hat, Inc.
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

import jdk.testlibrary.JarUtils;
import jdk.testlibrary.SecurityTools;
import jdk.testlibrary.Asserts;
import jdk.testlibrary.ProcessTools;
import jdk.testlibrary.OutputAnalyzer;
import java.util.Calendar;
import java.util.Locale;
import java.util.List;
import java.util.ArrayList;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.nio.file.Files;
import java.io.File;
import static java.util.Calendar.WEDNESDAY;

/*
 * @test
 * @bug 8297684 8269039
 * @summary Checking custom CalendarDataProvider with SPI contained in signed jar does
 *          not produce NPE.
 * @library /lib/testlibrary
 * @library provider
 * @build baz.CalendarDataProviderImpl
 * @run main/timeout=600 TestSPISigned
 */
public class TestSPISigned {

    private static final String TEST_CLASSES = System.getProperty("test.classes", ".");
    private static final String TEST_SRC = System.getProperty("test.src", ".");

    private static final Path META_INF_DIR = Paths.get(TEST_SRC, "provider", "meta");
    private static final Path META_INF_DIR_SOURCE = META_INF_DIR.resolve("META-INF");
    private static final Path SERVICES_DIR_SOURCE = META_INF_DIR_SOURCE.resolve("services");
    private static final Path PROVIDER_SOURCE = SERVICES_DIR_SOURCE.resolve("java.util.spi.CalendarDataProvider");
    private static final Path PROVIDER_PARENT = Paths.get(TEST_CLASSES);
    private static final Path PROVIDER_DIR = PROVIDER_PARENT.resolve("provider");
    private static final Path META_INF_DIR_TARGET = PROVIDER_DIR.resolve("META-INF");
    private static final Path SERVICES_DIR_TARGET = META_INF_DIR_TARGET.resolve("services");
    private static final Path PROVIDER_TARGET = SERVICES_DIR_TARGET.resolve("java.util.spi.CalendarDataProvider");
    private static final Path MODS_DIR = Paths.get("mods");
    private static final Path UNSIGNED_JAR = MODS_DIR.resolve("unsigned-with-locale.jar");
    private static final Path SIGNED_JAR = MODS_DIR.resolve("signed-with-locale.jar");

    public static void main(String[] args) throws Throwable {
        if (args.length == 1) {
            String arg = args[0];
            if ("run-test".equals(arg)) {
                System.out.println("Debug: Running test");
                String provProp = System.getProperty("java.ext.dirs");
                String[] extDirs = provProp.split(File.pathSeparator);
                boolean extDirFound = false;
                for (int i = 0; i < extDirs.length; i++) {
                    if (extDirs[i].equals(MODS_DIR.toAbsolutePath().toString())) {
                        extDirFound = true;
                        break;
                    }
                }
                if (!extDirFound) {
                   throw new RuntimeException("Test failed! Expected -Djava.ext.dirs to include CalendarDataProvider jar");
                }
                doRunTest();
            } else {
               throw new RuntimeException("Test failed! Expected 'run-test' arg for test run");
            }
        } else {
            // Set up signed jar with custom calendar data provider
            //
            // 1. Create jar with custom CalendarDataProvider
            Files.createDirectory(META_INF_DIR_TARGET);
            Files.createDirectory(SERVICES_DIR_TARGET);
            Files.copy(PROVIDER_SOURCE, PROVIDER_TARGET);
            JarUtils.createJarFile(UNSIGNED_JAR, PROVIDER_DIR);

            // create signer's keypair
            SecurityTools.keytool("-genkeypair -keyalg RSA -keystore ks " +
                                  "-storepass changeit -dname CN=test -alias test")
                     .shouldHaveExitValue(0);
            // sign jar
            SecurityTools.jarsigner("-keystore ks -storepass changeit " +
                                "-signedjar " + SIGNED_JAR + " " + UNSIGNED_JAR + " test")
                     .shouldHaveExitValue(0);
            // run test, which must not throw a NPE
            List<String> testRun = new ArrayList<>();
            String extDir = System.getProperty("java.home") + File.separator + "lib" + File.separator + "ext";
            testRun.add("-Djava.ext.dirs=" + extDir + File.pathSeparator + MODS_DIR.toAbsolutePath().toString());
            testRun.add("-cp");
            String classPath = System.getProperty("java.class.path");
            testRun.add(classPath);
            testRun.add(TestSPISigned.class.getSimpleName());
            testRun.add("run-test");
            OutputAnalyzer out = ProcessTools.executeTestJvm(testRun.toArray(new String[]{}));
            out.shouldHaveExitValue(0);
            out.shouldContain("DEBUG: Getting xx language");
        }
    }

    private static void doRunTest() {
        Locale locale = new Locale("xx", "YY");
        Calendar kcal = Calendar.getInstance(locale);
        try {
            check(WEDNESDAY, kcal.getFirstDayOfWeek());
            check(7, kcal.getMinimalDaysInFirstWeek());
        } catch (Throwable ex) {
            throw new RuntimeException("Test failed with signed jar and " +
                    " argument java.locale.providers=SPI", ex);
        }
    }

    private static <T> void check(T expected, T actual) {
        Asserts.assertEquals(expected, actual, "Expected calendar from SPI to be in effect");
    }

}
