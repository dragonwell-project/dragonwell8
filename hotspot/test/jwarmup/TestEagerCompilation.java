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

import java.io.*;
import java.lang.reflect.Method;
import java.util.*;
import java.util.regex.*;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
/*
 * @test TestEagerCompilation
 * @library /testlibrary /testlibrary/whitebox
 * @build TestEagerCompilation
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestEagerCompilation
 * @summary test eager compilation for method through ClassInitChain
 */
public class TestEagerCompilation {
    private static String classPath;

    private static final int HEADER_SIZE = 32;
    private static final int APPID_OFFSET = 16;

    public static String generateOriginLogfile() throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        File logfile = new File("./jitwarmup.log");
        classPath = System.getProperty("test.class.path");
        try {
            output = ProcessTools.executeProcess("pwd");
            System.out.println(output.getOutput());
        } catch (Throwable e) {
            e.printStackTrace();
        }

        String className = InnerA.class.getName();
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./" + logfile.getName(),
                "-XX:CompilationWarmUpRecordTime=10",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                className, "recording");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("[JitWarmUp] output profile info has done");
        output.shouldContain("process is done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        if (!logfile.exists()) {
            throw new Error("jit log not exist");
        }
        return logfile.getName();
    }

    public static String generateOriginLogfileWithG1() throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        File logfile = new File("./jitwarmup.log");
        classPath = System.getProperty("test.class.path");
        try {
            output = ProcessTools.executeProcess("pwd");
            System.out.println(output.getOutput());
        } catch (Throwable e) {
            e.printStackTrace();
        }

        String className = InnerA.class.getName();
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-ClassUnloading",
                "-XX:+UseG1GC",
                "-XX:-ClassUnloadingWithConcurrentMark",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUpRecording",
                "-XX:CompilationWarmUpLogfile=./" + logfile.getName(),
                "-XX:CompilationWarmUpRecordTime=10",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                className, "recording");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("[JitWarmUp] output profile info has done");
        output.shouldContain("process is done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        if (!logfile.exists()) {
            throw new Error("jit log not exist");
        }
        return logfile.getName();
    }

    public static OutputAnalyzer testJitWarmUpJavaAPI(String filename) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:CompilationWarmUpLogfile=./" + filename,
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "compilation");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    public static OutputAnalyzer testJitWarmUpJavaAPIwithEagerResolve(String filename) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:CompilationWarmUpLogfile=./" + filename,
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-XX:+CompilationWarmUpResolveClassEagerly",
                "-cp", classPath,
                InnerA.class.getName(), "compilation");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    public static OutputAnalyzer testJitWarmUpJavaAPIWithG1(String filename) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:+UseG1GC",
                "-XX:-ClassUnloadingWithConcurrentMark",
                "-XX:CompilationWarmUpLogfile=./" + filename,
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "compilation");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    public static OutputAnalyzer testJitWarmDeopt(String filename) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:-UseSharedSpaces",
                "-XX:+CompilationWarmUp",
                "-XX:CompilationWarmUpLogfile=./" + filename,
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:CompilationWarmUpDeoptTime=5",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "deopt");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    private static String methodName = "TestEagerCompilation$InnerA.foo2([Ljava/lang/String;)V";

    public static void main(String[] args) throws Exception {
        OutputAnalyzer output = null;
        String fileName = generateOriginLogfile();
        // test origin log file
        output = testJitWarmUpJavaAPI(fileName);
        output.shouldContain("Test Eager Compilation OK");
        output.shouldContain(methodName);
        output.shouldHaveExitValue(0);

        output = testJitWarmUpJavaAPIwithEagerResolve(fileName);
        output.shouldContain("Test Eager Compilation OK");
        output.shouldContain(methodName);
        output.shouldHaveExitValue(0);

        fileName = generateOriginLogfileWithG1();
        output = testJitWarmUpJavaAPIWithG1(fileName);
        output.shouldContain("Test Eager Compilation OK");
        output.shouldContain(methodName);
        output.shouldHaveExitValue(0);
    }

    public static class InnerB {
        static {
            System.out.println("InnerB initialize");
        }
        public Object content;
    }

    public static class InnerA {
        private static WhiteBox whiteBox;
        static {
            System.out.println("InnerA initialize");
        }

        public static String[] aa = new String[0];
        public static List<String> ls = new ArrayList<String>();
        public String foo() {
            for (int i = 0; i < 12000; i++) {
                foo2(aa);
            }
            ls.add("x");
            return ls.get(0);
        }
        public void foo2(String[] a) {
            String s = "aa";
            if (ls.size() > 100 && a.length < 100) {
                ls.clear();
            } else {
                ls.add(s);
            }
        }

        public static void doBiz() throws Exception {
            InnerA a = new InnerA();
            a.foo();
            InnerB b = new InnerB();
            System.out.println(b);
            Thread.sleep(15000);
            a.foo();
            System.out.println("process is done!");
        }

        public static void main(String[] args) throws Exception {
            if (args[0].equals("recording")) {
                doBiz();
            } else if (args[0].equals("compilation")) {
                Class c = Class.forName("com.alibaba.jwarmup.JWarmUp");
                Method m2 = c.getMethod("notifyApplicationStartUpIsDone");
                m2.invoke(null);
                System.out.println("invoke to notifyApplicationStartUpIsDone is Done");
                // wait for compilation
                Thread.sleep(5000);
                System.out.println("Test Eager Compilation OK");
            } else if (args[0].equals("optimistic")) {
                // wait for compilation
                Thread.sleep(5000);
                System.out.println("Test Eager Compilation OK");
            } else if (args[0].equals("deopt")) {
                Class c = Class.forName("com.alibaba.jwarmup.JWarmUp");
                Method m2 = c.getMethod("notifyApplicationStartUpIsDone");
                m2.invoke(null);
                System.out.println("invoke to notifyApplicationStartUpIsDone is Done");
                // wait for compilation
                Thread.sleep(1000);
                doBiz();
                Thread.sleep(5000);
                System.out.println("Test Eager Compilation OK");
            }
        }
    }
}
