/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8339280
 * @summary Test that jarsigner -verify emits a warning when the filename of
 *     an entry in the LOC is changed
 * @library /lib/testlibrary
 * @run junit VerifyJarEntryName
 */

import org.junit.BeforeClass;
import org.junit.Before;
import org.junit.Test;

import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import jdk.testlibrary.SecurityTools;
import static org.junit.Assert.fail;

public class VerifyJarEntryName {

    private static final Path ORIGINAL_JAR = Paths.get("test.jar");
    private static final Path MODIFIED_JAR = Paths.get("modified_test.jar");

    @BeforeClass
    public static void setup() throws Exception {
        try (FileOutputStream fos = new FileOutputStream(ORIGINAL_JAR.toFile());
             ZipOutputStream zos = new ZipOutputStream(fos)) {
            zos.putNextEntry(new ZipEntry(JarFile.MANIFEST_NAME));
            zos.write("Manifest-Version: 1.0\nCreated-By: Test\n".
                    getBytes(StandardCharsets.UTF_8));
            zos.closeEntry();

            // Add hello.txt file
            ZipEntry textEntry = new ZipEntry("hello.txt");
            zos.putNextEntry(textEntry);
            zos.write("hello".getBytes(StandardCharsets.UTF_8));
            zos.closeEntry();
        }

        SecurityTools.keytool("-genkeypair -keystore ks -storepass changeit "
                + "-alias mykey -keyalg rsa -dname CN=me ");

        SecurityTools.jarsigner("-keystore ks -storepass changeit "
                        + ORIGINAL_JAR + " mykey")
                .shouldHaveExitValue(0);
    }

    @Before
    public void cleanup() throws Exception {
        Files.deleteIfExists(MODIFIED_JAR);
    }

    /*
     * Modify a single byte in "MANIFEST.MF" filename in LOC, and
     * validate that jarsigner -verify emits a warning message.
     */
    @Test
    public void verifyManifestEntryName() throws Exception {
        modifyJarEntryName(ORIGINAL_JAR, MODIFIED_JAR, "META-INF/MANIFEST.MF");
        SecurityTools.jarsigner("-verify -verbose " + MODIFIED_JAR)
                .shouldContain("This JAR file contains internal " +
                        "inconsistencies that may result in different " +
                        "contents when reading via JarFile and JarInputStream:")
                .shouldContain("- Manifest is missing when " +
                        "reading via JarInputStream")
                .shouldHaveExitValue(0);
    }

    /*
     * Modify a single byte in signature filename in LOC, and
     * validate that jarsigner -verify emits a warning message.
     */
    @Test
    public void verifySignatureEntryName() throws Exception {
        modifyJarEntryName(ORIGINAL_JAR, MODIFIED_JAR, "META-INF/MYKEY.SF");
        SecurityTools.jarsigner("-verify -verbose " + MODIFIED_JAR)
                .shouldContain("This JAR file contains internal " +
                        "inconsistencies that may result in different " +
                        "contents when reading via JarFile and JarInputStream:")
                .shouldContain("- Entry XETA-INF/MYKEY.SF is present when reading " +
                        "via JarInputStream but missing when reading via JarFile")
                .shouldHaveExitValue(0);
    }

    /*
     * Validate that jarsigner -verify on a valid JAR works without
     * emitting warnings about internal inconsistencies.
     */
    @Test
    public void verifyOriginalJar() throws Exception {
        SecurityTools.jarsigner("-verify -verbose " + ORIGINAL_JAR)
                .shouldNotContain("This JAR file contains internal " +
                        "inconsistencies that may result in different contents when " +
                        "reading via JarFile and JarInputStream:")
                .shouldHaveExitValue(0);
    }

    private void modifyJarEntryName(Path origJar, Path modifiedJar,
            String entryName) throws Exception {
        byte[] jarBytes = Files.readAllBytes(origJar);
        byte[] entryNameBytes = entryName.getBytes(StandardCharsets.UTF_8);
        int pos = 0;
        try {
            while (!Arrays.equals(Arrays.copyOfRange(jarBytes, pos,
                    pos + entryNameBytes.length), entryNameBytes)) pos++;
        } catch (ArrayIndexOutOfBoundsException ignore) {
            fail(entryName + " is not present in the JAR");
        }
        jarBytes[pos] = 'X';
        Files.write(modifiedJar, jarBytes);
    }
}
