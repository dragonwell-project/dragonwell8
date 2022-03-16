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
 * @test TestThrowInitializaitonException
 * @library /testlibrary /testlibrary/whitebox
 * @build TestThrowInitializaitonException
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestThrowInitializaitonException
 * @summary test throwing ExceptionInInitializerError in notifyApplicationStartUpIsDone API
 */
public class TestThrowInitializaitonException {

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

        pb = ProcessTools.createJavaProcessBuilder("-Xbootclasspath/a:.",
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-XX:-Inline",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpRecordTime=10",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions",
                "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "recording");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());

        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:-Inline",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                // "-XX:+DeoptimizeBeforeWarmUp",
                "-cp", classPath,
                InnerA.class.getName(), "startup");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("catched java.lang.ExceptionInInitializerError");
        output.shouldHaveExitValue(0);

        logfile = new File("./jitwarmup.log");
        if (logfile.exists()) {
            try {
                logfile.delete();
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    public static class InnerB {
        static {
            System.out.println("InnerB initialize");
            if (InnerA.flag) {
                throw new NullPointerException();
            }
        }
    }

    public static class InnerA {
        static {
            System.out.println("InnerA initialize");
        }

        public static boolean flag = true;

        public static void main(String[] args) throws Exception {
            if ("recording".equals(args[0])) {
                System.out.println("begin recording!");
                try {
                    InnerA a = new InnerA();
                    InnerB b = new InnerB();
                } catch (Throwable e) {
                    // ignore
                }
                Thread.sleep(10000);
                System.out.println("done!");
            } else {
                try {
                    System.out.println("begin startup!");
                    JWarmUp.notifyApplicationStartUpIsDone();
                    Thread.sleep(3000);
                } catch (Throwable e) {
                    System.out.println("catched " + e.getClass().getName());
                }
            }
        }
    }
}
