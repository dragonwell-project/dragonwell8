/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8215694
 * @summary keytool cannot generate RSASSA-PSS certificates
 * @library /lib/testlibrary
 * @run main/timeout=600 PSS
 */

import jdk.testlibrary.Asserts;
import jdk.testlibrary.security.DerUtils;
import sun.security.util.ObjectIdentifier;
import sun.security.x509.AlgorithmId;

import java.io.File;
import java.io.FileInputStream;
import java.security.KeyStore;
import java.security.cert.X509Certificate;

public class PSS {

    public static void main(String[] args) throws Exception {

        // make sure no old file exists
        new File("ks").delete();

        genkeypair("p", "-keyalg RSASSA-PSS -sigalg RSASSA-PSS");
        genkeypair("a", "-keyalg RSA -sigalg RSASSA-PSS -keysize 2048");
        genkeypair("b", "-keyalg RSA -sigalg RSASSA-PSS -keysize 4096");
        genkeypair("c", "-keyalg RSA -sigalg RSASSA-PSS -keysize 8192");

        //@since jdk9
        //KeyStore ks = KeyStore.getInstance(
        //        new File("ks"), "changeit".toCharArray());

        KeyStore ks = KeyStore.getInstance(KeyStore.getDefaultType());
        // get user password and file input stream
        char[] password = "changeit".toCharArray();
        try (FileInputStream fis = new FileInputStream("ks")) {
            ks.load(fis, password);
        }

        check((X509Certificate)ks.getCertificate("p"), "RSASSA-PSS",
                AlgorithmId.SHA256_oid);

        check((X509Certificate)ks.getCertificate("a"), "RSA",
                AlgorithmId.SHA256_oid);

        check((X509Certificate)ks.getCertificate("b"), "RSA",
                AlgorithmId.SHA384_oid);

        check((X509Certificate)ks.getCertificate("c"), "RSA",
                AlgorithmId.SHA512_oid);

        // More commands
        kt("-certreq -alias p -sigalg RSASSA-PSS -file p.req");

        kt("-gencert -alias a -sigalg RSASSA-PSS -infile p.req -outfile p.cert");
        kt("-importcert -alias p -file p.cert");

        kt("-selfcert -alias p -sigalg RSASSA-PSS");
    }

    static void genkeypair(String alias, String options)
            throws Exception {
        kt("-genkeypair -alias " + alias
                + " -dname CN=" + alias + " " + options);
    }

    static void kt(String cmd) throws Exception {
        String listStr = "-storepass changeit -keypass changeit "
                    + "-keystore ks " + cmd;
        sun.security.tools.keytool.Main.main(listStr.split(" "));
    }

    static void check(X509Certificate cert, String expectedKeyAlg,
            ObjectIdentifier expectedMdAlg) throws Exception {
        Asserts.assertEQ(cert.getPublicKey().getAlgorithm(), expectedKeyAlg);
        Asserts.assertEQ(cert.getSigAlgName(), "RSASSA-PSS");
        DerUtils.checkAlg(cert.getSigAlgParams(), "000", expectedMdAlg);
    }
}
