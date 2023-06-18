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
 * @test CommandLineFlagComboNegative
 * @summary Test command line flag combinations that differ between
 *          the dump and execute steps, in such way that they cause errors
 *          E.g. use compressed oops for creating and archive, but then
 *               execute w/o compressed oops
 * AppCDS does not support uncompressed oops
 * @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 * @library /testlibrary
 * @compile test-classes/Hello.java
 * @run main CommandLineFlagComboNegative
 */

import java.util.ArrayList;
import com.oracle.java.testlibrary.*;
public class CommandLineFlagComboNegative {

    private class TestVector {
        public String testOptionForDumpStep;
        public String testOptionForExecuteStep;
        public String expectedErrorMsg;
        public int expectedErrorCode;

        public TestVector(String testOptionForDumpStep, String testOptionForExecuteStep,
                          String expectedErrorMsg, int expectedErrorCode) {
            this.testOptionForDumpStep=testOptionForDumpStep;
            this.testOptionForExecuteStep=testOptionForExecuteStep;
            this.expectedErrorMsg=expectedErrorMsg;
            this.expectedErrorCode=expectedErrorCode;
        }
    }

    private ArrayList<TestVector> testTable = new ArrayList<TestVector>();

    private void initTestTable() {
        // These options are not applicable on 32-bit platforms
        if (Platform.is64bit()) {
            testTable.add( new TestVector("-XX:ObjectAlignmentInBytes=8", "-XX:ObjectAlignmentInBytes=16",
                "An error has occurred while processing the shared archive file", 1) );
            testTable.add( new TestVector("-XX:ObjectAlignmentInBytes=64", "-XX:ObjectAlignmentInBytes=32",
                "An error has occurred while processing the shared archive file", 1) );
            testTable.add( new TestVector("-XX:+UseCompressedOops", "-XX:-UseCompressedOops",
                "Class data sharing is inconsistent with other specified options", 1) );
            testTable.add( new TestVector("-XX:+UseCompressedClassPointers", "-XX:-UseCompressedClassPointers",
                "Class data sharing is inconsistent with other specified options", 1) );
        }
    }

    private void runTests() throws Exception
    {
        for (TestVector testEntry : testTable) {
            System.out.println("CommandLineFlagComboNegative: dump = " + testEntry.testOptionForDumpStep);
            System.out.println("CommandLineFlagComboNegative: execute = " + testEntry.testOptionForExecuteStep);

            String appJar = JarBuilder.getOrCreateHelloJar();
            OutputAnalyzer dumpOutput = TestCommon.dump(
               appJar, new String[] {"Hello"}, testEntry.testOptionForDumpStep);

            TestCommon.checkDump(dumpOutput, "Loading classes to share");

            OutputAnalyzer execOutput = TestCommon.exec(appJar, testEntry.testOptionForExecuteStep, "Hello");
            execOutput.shouldContain(testEntry.expectedErrorMsg);
            execOutput.shouldHaveExitValue(testEntry.expectedErrorCode);
        }
    }

    public static void main(String[] args) throws Exception {
        CommandLineFlagComboNegative thisClass = new CommandLineFlagComboNegative();
        thisClass.initTestTable();
        thisClass.runTests();
    }
}
