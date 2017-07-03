/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8176329
 * @summary Test for jdeps warning when it encounters a multi-release jar
 * @run testng MRJarWarning
 */

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import org.testng.Assert;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;

public class MRJarWarning {
    private static final String WARNING = " is a multi-release jar file";
    private static final String MRJAR_ATTR = "Multi-Release";

    Path mrjar1;
    Path mrjar2;
    Path nonMRjar;
    Path mrjarAllCaps;

    private Attributes defaultAttributes;

    @BeforeSuite
    public void setup() throws IOException {
        defaultAttributes = new Attributes();
        defaultAttributes.putValue("Manifest-Version", "1.0");
        defaultAttributes.putValue("Created-By", "1.8.0-internal");

        mrjar1   = Paths.get("mrjar1.jar");
        mrjar2   = Paths.get("mrjar2.jar");
        nonMRjar = Paths.get("nonMRjar.jar");
        mrjarAllCaps = Paths.get("mrjarAllCaps.jar");

        Attributes mrJarAttrs = new Attributes(defaultAttributes);
        mrJarAttrs.putValue(MRJAR_ATTR, "true");

        build(mrjar1, mrJarAttrs);
        build(mrjar2, mrJarAttrs);
        build(nonMRjar, defaultAttributes);

        // JEP 238 - "Multi-Release JAR Files" states that the attribute name
        // and value are case insensitive.  Try with all caps to ensure that
        // jdeps still recognizes a multi-release jar.
        Attributes allCapsAttrs = new Attributes(defaultAttributes);
        allCapsAttrs.putValue(MRJAR_ATTR.toUpperCase(), "TRUE");
        build(mrjarAllCaps, allCapsAttrs);
    }

    @DataProvider(name="provider")
    private Object[][] args() {
        // jdeps warning messages may be localized.
        // This test only checks for the English version.  Return an empty
        // array (skip testing) if the default language is not English.
        String language = Locale.getDefault().getLanguage();
        System.out.println("Language: " + language);

        if ("en".equals(language)) {
            return new Object[][] {
                // one mrjar arg
                {   Arrays.asList(mrjar1.toString()),
                    Arrays.asList(mrjar1)},
                // two mrjar args
                {   Arrays.asList(mrjar1.toString(), mrjar2.toString()),
                    Arrays.asList(mrjar1, mrjar2)},
                // one mrjar arg, with mrjar on classpath
                {   Arrays.asList("-cp", mrjar2.toString(), mrjar1.toString()),
                    Arrays.asList(mrjar1, mrjar2)},
                // non-mrjar arg, with mrjar on classpath
                {   Arrays.asList("-cp", mrjar1.toString(), nonMRjar.toString()),
                    Arrays.asList(mrjar1)},
                // mrjar arg with jar attribute name/value in ALL CAPS
                {   Arrays.asList(mrjarAllCaps.toString()),
                    Arrays.asList(mrjarAllCaps)},
                // non-mrjar arg
                {   Arrays.asList(nonMRjar.toString()),
                    Collections.emptyList()}
            };
        } else {
            System.out.println("Non-English language \""+ language +
                    "\"; test passes superficially");
            return new Object[][]{};
        }
    }

    /* Run jdeps with the arguments given in 'args', and confirm that a warning
     * is issued for each Multi-Release jar in 'expectedMRpaths'.
     */
    @Test(dataProvider="provider")
    public void checkWarning(List<String> args, List<Path> expectedMRpaths) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);

        int rc = com.sun.tools.jdeps.Main.run(args.toArray(new String[args.size()]), pw);
        pw.close();

        expectedMRJars(sw.toString(), expectedMRpaths);
        Assert.assertEquals(rc, 0, "non-zero exit code from jdeps");
    }

    /* Confirm that warnings for the specified paths are in the output (or that
     * warnings are absent if 'paths' is empty).
     * Doesn't check for extra, unexpected warnings.
     */
    private static void expectedMRJars(String output, List<Path> paths) {
        if (paths.isEmpty()) {
            Assert.assertFalse(output.contains(WARNING),
                               "Expected no mrjars, but found:\n" + output);
        } else {
            for (Path path : paths) {
                String expect = "Warning: " + path.toString() + WARNING;
                Assert.assertTrue(output.contains(expect),
                        "Did not find:\n" + expect + "\nin:\n" + output + "\n");
            }
        }
    }

    /* Build a jar at the expected path, containing the given attributes */
    private static void build(Path path, Attributes attributes) throws IOException {
        try (OutputStream os = Files.newOutputStream(path);
             JarOutputStream jos = new JarOutputStream(os)) {

            JarEntry me = new JarEntry("META-INF/MANIFEST.MF");
            jos.putNextEntry(me);
            Manifest manifest = new Manifest();
            manifest.getMainAttributes().putAll(attributes);
            manifest.write(jos);
            jos.closeEntry();
        }
    }
}
