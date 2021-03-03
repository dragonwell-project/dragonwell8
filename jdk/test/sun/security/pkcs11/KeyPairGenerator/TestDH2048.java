/*
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 7196382 8072452
 * @summary Ensure that DH key pairs can be generated for 512 - 8192 bits
 * @author Valerie Peng
 * @library ..
 */

import java.io.*;
import java.util.*;

import java.security.*;

import javax.crypto.*;

public class TestDH2048 extends PKCS11Test {

    private static void checkUnsupportedKeySize(KeyPairGenerator kpg, int ks)
        throws Exception {
        try {
            kpg.initialize(ks);
            throw new Exception("Expected IPE not thrown for " + ks);
        } catch (InvalidParameterException ipe) {
        }
    }

    public void main(Provider p) throws Exception {
        if (p.getService("KeyPairGenerator", "DH") == null) {
            System.out.println("KPG for DH not supported, skipping");
            return;
        }
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("DH", p);
        kpg.initialize(512);
        KeyPair kp1 = kpg.generateKeyPair();

        int[] test_values = {768, 1024, 1536, 2048, 3072, 4096, 6144, 8192};
        for (int i : test_values)
        try {
            kpg.initialize(i);
            kp1 = kpg.generateKeyPair();
        } catch (InvalidParameterException ipe) {
            // NSS (as of version 3.13) has a hard coded maximum limit
            // of 2236 or 3072 bits for DHE keys.
            // SunPKCS11-Solaris has limit of 4096 on older systems
            String prov = p.getName();
            System.out.println(i + "-bit DH key pair generation: " + ipe);
            if ((prov.equals("SunPKCS11-NSS") && i > 2048) ||
                (prov.equals("SunPKCS11-Solaris") && i > 4096)) {
                // OK
            } else {
                throw ipe;
            }
        }

        // key size must be multiples of 64 though
        checkUnsupportedKeySize(kpg, 2048 + 63);
        checkUnsupportedKeySize(kpg, 3072 + 32);
    }

    public static void main(String[] args) throws Exception {
        main(new TestDH2048());
    }
}
