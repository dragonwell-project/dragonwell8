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
 *  @test
 *  @summary If -Djava.system.class.loader=xxx is specified in command-line, disable UseAppCDS
 *  AppCDS does not support uncompressed oops
 *  @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 *  @library /testlibrary
 *  @compile test-classes/TestClassLoader.java
 *  @compile test-classes/ReportMyLoader.java
 *  @compile test-classes/TrySwitchMyLoader.java
 *  @run main SpecifySysLoaderProp
 */

import java.io.*;
import com.oracle.java.testlibrary.OutputAnalyzer;

public class SpecifySysLoaderProp {

  public static void main(String[] args) throws Exception {
    JarBuilder.build("sysloader", "TestClassLoader", "ReportMyLoader", "TrySwitchMyLoader");

    String jarFileName = "sysloader.jar";
    String appJar = TestCommon.getTestJar(jarFileName);
    TestCommon.testDump(appJar, TestCommon.list("ReportMyLoader"));
    String warning = "VM warning: UseAppCDS is disabled because the java.system.class.loader property is specified";


    // (0) Baseline. Do not specify -Djava.system.class.loader
    //     The test class should be loaded from archive
    OutputAnalyzer output = TestCommon.execCommon(
        "-verbose:class",
        "-cp", appJar,
        "ReportMyLoader");
    TestCommon.checkExec(output,
                         "[Loaded ReportMyLoader from shared objects file",
                         "ReportMyLoader's loader = sun.misc.Launcher$AppClassLoader@");

    // (1) Try to execute the archive with -Djava.system.class.loader=no.such.Klass,
    //     it should fail
    output = TestCommon.execCommon(
        "-cp", appJar,
        "-Djava.system.class.loader=no.such.Klass",
        "ReportMyLoader");
    try {
        output.shouldContain(warning);
        // output.shouldContain("ClassNotFoundException: no.such.Klass");
    } catch (Exception e) {
        TestCommon.checkCommonExecExceptions(output, e);
    }

    // (2) Try to execute the archive with -Djava.system.class.loader=TestClassLoader,
    //     it should run, but AppCDS should be disabled
    output = TestCommon.execCommon(
        "-verbose:class",
        "-cp", appJar,
        "-Djava.system.class.loader=TestClassLoader",
        "ReportMyLoader");
    try {
        output.shouldContain(warning);
    } catch (Exception e) {
        TestCommon.checkCommonExecExceptions(output, e);
    }

    // (3) Try to change the java.system.class.loader programmatically after
    //     the app's main method is executed. This should have no effect in terms of
    //     changing or switching the actual system class loader that's already in use.
    output = TestCommon.execCommon(
        "-verbose:class",
        "-cp", appJar,
        "TrySwitchMyLoader");
    TestCommon.checkExec(output,
                         "[Loaded ReportMyLoader from shared objects file",
                         "TrySwitchMyLoader's loader = sun.misc.Launcher$AppClassLoader@",
                         "ReportMyLoader's loader = sun.misc.Launcher$AppClassLoader@",
                         "TestClassLoader.called = false");
  }
}
