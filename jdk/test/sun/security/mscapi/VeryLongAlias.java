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
 * @bug 8223063
 * @requires os.family == "windows"
 * @library /lib/testlibrary
 * @summary Support CNG RSA keys
 */

import jdk.testlibrary.SecurityTools;
import jdk.testlibrary.ProcessTools;

import java.io.File;
import java.io.FileInputStream;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.cert.X509Certificate;
import java.util.List;
import java.util.Random;

public class VeryLongAlias {

    static String alias = String.format("%0512d", new Random().nextInt(100000));

    public static void main(String[] args) throws Throwable {

        SecurityTools.keytool("-genkeypair -storetype pkcs12 -keystore ks"
                + " -storepass changeit -keyalg RSA -dname CN=A -alias "
                + alias);

        KeyStore ks = KeyStore.getInstance("PKCS12");

        try (FileInputStream fis = new FileInputStream("ks")) {
            ks.load(fis, "changeit".toCharArray());
        }

        String id = ((X509Certificate)ks.getCertificate(alias))
                .getSerialNumber().toString(16);
        try {
            // Importing pkcs12 file. Long alias is only supported by CNG.
            //ProcessTools.executeCommand("certutil", "-v", "-p", "changeit",
            //           "-csp", "Microsoft Software Key Storage Provider",
            //           "-user", "-importpfx", "MY", "ks", "NoRoot,NoExport")
            // Adapt for Win7 as it only support a subset of above arguments
            ProcessTools.executeCommand("certutil", "-v", "-p", "changeit",
                        "-user", "-importpfx", "ks", "NoRoot")
                    .shouldHaveExitValue(0);
            test();
        } finally {
            ProcessTools.executeCommand("certutil", "-user", "-delstore", "MY",
                    id);
        }
    }

    static void test() throws Exception {

        char[] pass = "changeit".toCharArray();

        KeyStore k1 = KeyStore.getInstance("Windows-MY");
        k1.load(null, null);

        KeyStore k2 = KeyStore.getInstance("PKCS12");

        try (FileInputStream fis = new FileInputStream("ks")) {
            k2.load(fis, pass);
        }

        PrivateKey p1 = (PrivateKey)k1.getKey(alias, null);
        PublicKey u1 = k1.getCertificate(alias).getPublicKey();

        PrivateKey p2 = (PrivateKey)k2.getKey(alias, pass);
        PublicKey u2 = k2.getCertificate(alias).getPublicKey();

        System.out.println(p1.toString());
        System.out.println(u1.toString());
        if (!p1.toString().contains("type=CNG")) {
            throw new Exception("Not a CNG key");
        }

        testSignature(p1, u1);
        testSignature(p1, u2);
        testSignature(p2, u1);
        testSignature(p2, u2);
    }

    static void testSignature(PrivateKey p, PublicKey u) throws Exception {
        byte[] data = "hello".getBytes();

        String[] ALGS = {
                "NONEwithRSA", "SHA1withRSA",
                "SHA256withRSA", "SHA512withRSA"
        };

        for (String alg : ALGS) {
            if (alg.contains("NONE")) {
                data = MessageDigest.getInstance("SHA-256").digest(data);
            }
            Signature s1 = Signature.getInstance(alg);
            Signature s2 = Signature.getInstance(alg);
            s1.initSign(p);
            s2.initVerify(u);
            s1.update(data);
            s2.update(data);
            if (!s2.verify(s1.sign())) {
                throw new Exception("Error");
            }
        }
    }
}
