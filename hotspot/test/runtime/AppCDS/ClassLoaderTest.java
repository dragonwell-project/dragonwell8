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
 * @summary Initiating and defining classloader test.
 * AppCDS does not support uncompressed oops
 * @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 * @library /testlibrary /testlibrary/whitebox
 * @compile test-classes/Hello.java
 * @compile test-classes/HelloWB.java
 * @compile test-classes/ForNameTest.java
 * @compile test-classes/BootClassPathAppendHelper.java
 * @build sun.hotspot.WhiteBox
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main ClassLoaderTest
 */

import java.io.File;
import com.oracle.java.testlibrary.OutputAnalyzer;
import sun.hotspot.WhiteBox;

public class ClassLoaderTest {
    public static void main(String[] args) throws Exception {
        JarBuilder.build(true, "ClassLoaderTest-WhiteBox", "sun/hotspot/WhiteBox");
        JarBuilder.getOrCreateHelloJar();
        JarBuilder.build("ClassLoaderTest-HelloWB", "HelloWB");
        JarBuilder.build("ClassLoaderTest-ForName", "ForNameTest");
        ClassLoaderTest test = new ClassLoaderTest();
        test.testBootLoader();
        test.testDefiningLoader();
    }

    public void testBootLoader() throws Exception {
        String appJar = TestCommon.getTestJar("ClassLoaderTest-HelloWB.jar");
        String appClasses[] = {"HelloWB"};
        String whiteBoxJar = TestCommon.getTestJar("ClassLoaderTest-WhiteBox.jar");
        String bootClassPath = "-Xbootclasspath/a:" + appJar +
            File.pathSeparator + whiteBoxJar;

        TestCommon.dump(appJar, appClasses, bootClassPath);

        OutputAnalyzer runtimeOutput = TestCommon.execCommon(
            "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-cp", appJar, bootClassPath, "-XX:+TraceClassLoading", "HelloWB");

        if (!TestCommon.isUnableToMap(runtimeOutput)) {
            runtimeOutput.shouldNotContain(
                "[HelloWB source: shared objects file by sun/misc/Launcher$AppClassLoader");
            runtimeOutput.shouldContain("[Loaded HelloWB from shared objects file");
        }
    }

    public void testDefiningLoader() throws Exception {
        // The boot loader should be used to load the class when it's
        // on the bootclasspath, regardless who is the initiating classloader.
        // In this test case, the AppClassLoader is the initiating classloader.
        String helloJar = TestCommon.getTestJar("hello.jar");
        String appJar = helloJar + System.getProperty("path.separator") +
                        TestCommon.getTestJar("ClassLoaderTest-ForName.jar");
        String whiteBoxJar = TestCommon.getTestJar("ClassLoaderTest-WhiteBox.jar");
        String bootClassPath = "-Xbootclasspath/a:" + helloJar +
            File.pathSeparator + whiteBoxJar;

        TestCommon.dump(helloJar, TestCommon.list("Hello"), bootClassPath);

        TestCommon.execCommon("-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-cp", appJar, bootClassPath, "-XX:+TraceClassPaths", "ForNameTest");
    }
}
