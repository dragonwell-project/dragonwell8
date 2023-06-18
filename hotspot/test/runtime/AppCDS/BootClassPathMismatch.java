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
 * @summary bootclasspath mismatch test.
 * AppCDS does not support uncompressed oops
 * @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 * @library /testlibrary
 * @compile test-classes/Hello.java
 * @run main BootClassPathMismatch
 */

// import jdk.test.lib.process.OutputAnalyzer;
import com.oracle.java.testlibrary.*;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.StandardCopyOption;
import java.nio.file.Paths;


public class BootClassPathMismatch {
    private static final String mismatchMessage = "shared class paths mismatch";

    public static void main(String[] args) throws Exception {
        JarBuilder.getOrCreateHelloJar();
        copyHelloToNewDir();

        BootClassPathMismatch test = new BootClassPathMismatch();
        test.testBootClassPathMismatch();
        test.testBootClassPathMatch();
    }

    /* Error should be detected if:
     * dump time: -Xbootclasspath/a:${testdir}/hello.jar
     * run-time : -Xbootclasspath/a:${testdir}/newdir/hello.jar
     */
    public void testBootClassPathMismatch() throws Exception {
        String appJar = JarBuilder.getOrCreateHelloJar();
        String appClasses[] = {"Hello"};
        OutputAnalyzer dumpOutput = TestCommon.dump(
            appJar, appClasses, "-Xbootclasspath/a:" + appJar);
        String testDir = TestCommon.getTestDir("newdir");
        String otherJar = testDir + File.separator + "hello.jar";
        OutputAnalyzer execOutput = TestCommon.exec(
            appJar, "-verbose:class", "-Xbootclasspath/a:" + otherJar, "Hello");
        try {
            TestCommon.checkExec(execOutput, mismatchMessage);
        } catch (java.lang.RuntimeException re) {
          String cause = re.getMessage();
          if (!mismatchMessage.equals(cause)) {
              throw re;
          }
        }
    }

    /* No error if:
     * dump time: -Xbootclasspath/a:${testdir}/hello.jar
     * run-time : -Xbootclasspath/a:${testdir}/hello.jar
     */
    public void testBootClassPathMatch() throws Exception {
        String appJar = TestCommon.getTestJar("hello.jar");
        String appClasses[] = {"Hello"};
        OutputAnalyzer dumpOutput = TestCommon.dump(
            appJar, appClasses, "-Xbootclasspath/a:" + appJar);
        OutputAnalyzer execOutput = TestCommon.exec(
            appJar, "-verbose:class",
            "-Xbootclasspath/a:" + appJar, "Hello");
        TestCommon.checkExec(execOutput,
                "[Loaded Hello from shared objects file");
    }

    private static void copyHelloToNewDir() throws Exception {
        String classDir = System.getProperty("test.classes");
        String dstDir = classDir + File.separator + "newdir";
        try {
            Files.createDirectory(Paths.get(dstDir));
        } catch (FileAlreadyExistsException e) { }

        Files.copy(Paths.get(classDir, "hello.jar"),
            Paths.get(dstDir, "hello.jar"),
            StandardCopyOption.REPLACE_EXISTING);
    }
}
