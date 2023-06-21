/*
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

/*
 * @test
 * @summary AppCDS handling of prohibited package.
 * AppCDS does not support uncompressed oops
 * @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 * @library /testlibrary
 * @compile test-classes/ProhibitedHelper.java test-classes/Prohibited.jasm
 * @run main ProhibitedPackage
 */

import com.oracle.java.testlibrary.Platform;
import com.oracle.java.testlibrary.OutputAnalyzer;

public class ProhibitedPackage {

    public static void main(String[] args) throws Exception {
        JarBuilder.build("prohibited_pkg", "java/lang/Prohibited", "ProhibitedHelper");

        String appJar = TestCommon.getTestJar("prohibited_pkg.jar");

        // AppCDS for custom loader is only supported on linux-x64 and
        // Solaris 64-bit platforms.
        if ((Platform.isLinux() || Platform.isSolaris()) &&
            Platform.is64bit()) {
            String classlist[] = new String[] {
                "java/lang/Object id: 1",
                "java/lang/Prohibited id: 2 super: 1 source: " + appJar
            };

            // Make sure a class in a prohibited package for a custom loader
            // will be ignored during dumping.
            TestCommon.dump(appJar,
                            classlist)
                .shouldContain("Prohibited package for non-bootstrap classes: java/lang/Prohibited.class")
                .shouldHaveExitValue(0);
        }


        // Make sure a class in a prohibited package for a non-custom loader
        // will be ignored during dumping.
        TestCommon.dump(appJar,
                        TestCommon.list("java/lang/Prohibited", "ProhibitedHelper"),
                        "-verbose")
            .shouldNotContain("java.lang.Prohibited")
            .shouldHaveExitValue(0);

        // Try loading the class in a prohibited package with various -Xshare
        // modes. The class shouldn't be loaded and appropriate exceptions
        // are expected.

        OutputAnalyzer output;

        // -Xshare:on
        output = TestCommon.execCommon(
            "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-cp", appJar, "-verbose", "ProhibitedHelper");
        TestCommon.checkExec(output, "Prohibited package name: java.lang");

        // -Xshare:auto
        output = TestCommon.execAuto(
            "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-cp", appJar, "-verbose", "ProhibitedHelper");
        TestCommon.checkExec(output, "Prohibited package name: java.lang");

        // -Xshare:off
        output = TestCommon.execOff(
            "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-cp", appJar, "-verbose", "ProhibitedHelper");
        output.shouldContain("Prohibited package name: java.lang");
    }
}
