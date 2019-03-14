/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

import sun.hotspot.WhiteBox;

import java.lang.reflect.Method;
import java.util.*;
import java.io.File;
import com.alibaba.jwarmup.*;
import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;

/*
 * @test TestRecordNullMethodCounter
 * @library /testlibrary /testlibrary/whitebox
 * @build TestRecordNullMethodCounter
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestRecordNullMethodCounter
 * @summary test jwarmup record doesn't crash if method counter is null
 */
public class TestRecordNullMethodCounter {

    public static void main(String[] args) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        File logfile = null;
        String classPath = System.getProperty("test.class.path");
        try {
            output = ProcessTools.executeProcess("pwd");
            System.out.println(output.getOutput());
        } catch (Throwable e) {
            e.printStackTrace();
        }

        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-Xcomp",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpRecordTime=5",
                "-XX:+UnlockDiagnosticVMOptions",
                "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldHaveExitValue(0);
        output.shouldContain("[JitWarmUp] WARNING : method counter is NULL for method");
        System.out.println(output.getOutput());
    }

    public static class InnerA {
        public static void main(String[] args) throws Exception {
            java.lang.Thread.sleep(10000); // sleep 10s
        }
    }
}
