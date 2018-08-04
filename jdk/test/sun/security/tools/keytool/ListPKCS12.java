/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8193171
 * @summary keytool -list displays "JKS" for a PKCS12 keystore.
 * @library /lib/testlibrary
 * @run main/othervm ListPKCS12
 */

import jdk.testlibrary.SecurityTools;
import jdk.testlibrary.OutputAnalyzer;

public class ListPKCS12 {

    public static void main(String[] args) throws Throwable {

        kt("-genkey -alias a -dname CN=A -keystore ks" +
           " -storetype pkcs12 -storepass changeit")
                .shouldHaveExitValue(0);

        kt("-list -keystore ks -storepass changeit")
                .shouldNotContain("Keystore type: jks")
                .shouldNotContain("Keystore type: JKS")
                .shouldContain("Keystore type: PKCS12")
                .shouldHaveExitValue(0);
    }

    static OutputAnalyzer kt(String arg) throws Exception {
        return SecurityTools.keytool(arg);
    }
}
