/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8154009
 * @summary make sure getProviders() doesn't require additional permissions
 * @run main/othervm/policy=EmptyPolicy.policy GetProviders
 */

import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public class GetProviders {

    private static final String serviceAlgFilter = "Signature.SHA1withRSA";
    private static final String emptyServAlgFilter = "wrongSig.wrongAlg";

    public static void main(String[] args) throws Exception {
        try {
            Provider[] providers1 = Security.getProviders();
            System.out.println("Amount of providers1: " + providers1.length);

            Provider[] providers2 = Security.getProviders(serviceAlgFilter);
            System.out.println("Amount of providers2: " + providers2.length);

            Map<String, String> filter = new HashMap<String, String>();
            filter.put(serviceAlgFilter, "");
            Provider[] providers3 = Security.getProviders(filter);
            System.out.println("Amount of providers3: " + providers3.length);

            Provider[] emptyProv1 = Security.getProviders(emptyServAlgFilter);
            if (emptyProv1 != null) {
                throw new RuntimeException("Empty Filter returned: " +
                    emptyProv1.length + " providers");
            }
            System.out.println("emptyProv1 is empty as expected");

            Map<String, String> emptyFilter = new HashMap<String, String>();
            emptyFilter.put(emptyServAlgFilter, "");
            Provider[] emptyProv2 = Security.getProviders(emptyFilter);
            if (emptyProv2 != null) {
                throw new RuntimeException("Empty Filter returned: " +
                    emptyProv2.length + " providers");
            }
            System.out.println("emptyProv2 is empty as expected");

        } catch(ExceptionInInitializerError e) {
            e.printStackTrace(System.out);
            throw new RuntimeException("Provider initialization error due to "
                    + e.getCause());
        }
        System.out.println("Test passed");
    }

}
