/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8278851
 * @summary Check that jar entry with at least one non-disabled digest
 *          algorithm in manifest is treated as signed
 * @library /lib/testlibrary /lib/security
 * @build jdk.testlibrary.JarUtils
 *        jdk.testlibrary.Utils
 * @run main/othervm JarWithOneNonDisabledDigestAlg
 */

import java.io.InputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.security.CodeSigner;
import java.security.KeyStore;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.Locale;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.zip.ZipFile;

import jdk.testlibrary.JarUtils;

public class JarWithOneNonDisabledDigestAlg {

    private static final String PASS = "changeit";
    private static final String TESTFILE1 = "testfile1";
    private static final String TESTFILE2 = "testfile2";

    public static void main(String[] args) throws Exception {
        SecurityUtils.removeFromDisabledAlgs("jdk.jar.disabledAlgorithms",
            Arrays.asList("SHA1"));
        Files.write(Paths.get(TESTFILE1), TESTFILE1.getBytes());
        JarUtils.createJarFile(Paths.get("unsigned.jar"), Paths.get("."),
            Paths.get(TESTFILE1));

        genkeypair("-alias SHA1 -sigalg SHA1withRSA");
        genkeypair("-alias SHA256 -sigalg SHA256withRSA");

        // Sign JAR twice with same signer but different digest algorithms
        // so that each entry in manifest file contains two digest values.
        signJarFile("SHA1", "MD5", "unsigned.jar", "signed.jar");
        signJarFile("SHA1", "SHA1", "signed.jar", "signed2.jar");
        checkThatJarIsSigned("signed2.jar", false);

        // add another file to the JAR
        Files.write(Paths.get(TESTFILE2), "testFile2".getBytes());
        JarUtils.updateJar("signed2.jar", "signed3.jar", TESTFILE2);
        Files.move(Paths.get("signed3.jar"), Paths.get("signed2.jar"), StandardCopyOption.REPLACE_EXISTING);

        // Sign again with different signer (SHA256) and SHA-1 digestalg.
        // TESTFILE1 should have two signers and TESTFILE2 should have one
        // signer.
        signJarFile("SHA256", "SHA1", "signed2.jar", "multi-signed.jar");

        checkThatJarIsSigned("multi-signed.jar", true);
    }

    private static KeyStore.PrivateKeyEntry getEntry(KeyStore ks, String alias)
        throws Exception {

        return (KeyStore.PrivateKeyEntry)
            ks.getEntry(alias,
                new KeyStore.PasswordProtection(PASS.toCharArray()));
    }

    private static void genkeypair(String cmd) throws Exception {
        cmd = "-genkeypair -keystore keystore -storepass " + PASS +
              " -keypass " + PASS + " -keyalg rsa -dname CN=Duke " + cmd;
        sun.security.tools.keytool.Main.main(cmd.split(" "));
    }

    private static void signJarFile(String alias,
        String digestAlg, String inputFile, String outputFile)
        throws Exception {

        // create a backup file as the signed file will be in-place
        Path from = Paths.get(inputFile);
        Path backup = Files.createTempFile(inputFile, "jar");
        Path output = Paths.get(outputFile);

        Files.copy(from, backup, StandardCopyOption.REPLACE_EXISTING);

        // sign the jar
        sun.security.tools.jarsigner.Main.main(
                ("-keystore keystore -storepass " + PASS +
                 " -digestalg " + digestAlg + " " + inputFile + " " +
                 alias).split(" "));

        Files.copy(from, output, StandardCopyOption.REPLACE_EXISTING);
        Files.copy(backup, from, StandardCopyOption.REPLACE_EXISTING);
    }

    private static void checkThatJarIsSigned(String jarFile, boolean multi)
        throws Exception {

        try (JarFile jf = new JarFile(jarFile, true)) {
            Enumeration<JarEntry> entries = jf.entries();
            while (entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                if (entry.isDirectory() || isSigningRelated(entry.getName())) {
                    continue;
                }
                InputStream is = jf.getInputStream(entry);
                while (is.read() != -1);
                CodeSigner[] signers = entry.getCodeSigners();
                if (signers == null) {
                    throw new Exception("JarEntry " + entry.getName() +
                        " is not signed");
                } else if (multi) {
                    if (entry.getName().equals(TESTFILE1) &&
                        signers.length != 2) {
                        throw new Exception("Unexpected number of signers " +
                            "for " + entry.getName() + ": " + signers.length);
                    } else if (entry.getName().equals(TESTFILE2) &&
                        signers.length != 1) {
                        throw new Exception("Unexpected number of signers " +
                            "for " + entry.getName() + ": " + signers.length);
                    }
                }
            }
        }
    }

    private static boolean isSigningRelated(String name) {
        name = name.toUpperCase(Locale.ENGLISH);
        if (!name.startsWith("META-INF/")) {
            return false;
        }
        name = name.substring(9);
        if (name.indexOf('/') != -1) {
            return false;
        }
        return name.endsWith(".SF")
            || name.endsWith(".DSA")
            || name.endsWith(".RSA")
            || name.endsWith(".EC")
            || name.equals("MANIFEST.MF");
    }
}
