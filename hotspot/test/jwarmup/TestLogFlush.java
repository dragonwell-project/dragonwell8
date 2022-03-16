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

import java.util.*;
import java.io.File;
import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;

/*
 * @test TestLogFlush
 * @library /testlibrary
 * @run main TestLogFlush
 * @summary test flushing profiling information into log in jitwarmup recording model
 */
public class TestLogFlush {
    public static void testFlush(String className) throws Exception {
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
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpRecordTime=10",
                "-cp", classPath,
                className);
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("[JitWarmUp] output profile info has done");
        output.shouldContain("process is done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        logfile = new File("./jitwarmup.log");
        if (!logfile.exists()) {
            throw new Error("jit log not exist");
        }

        try {
            logfile.delete();
        } catch (Throwable e) {
            e.printStackTrace();
        }

        // test file name is not manually specified
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-XX:+CompilationWarmUpRecording",
                "-XX:-ClassUnloading",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:-UseSharedSpaces",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpRecordTime=10",
                "-cp", classPath,
                className);
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("[JitWarmUp] output profile info has done");
        output.shouldContain("process is done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        logfile = null;
        File path = new File(".");
        if (path.isDirectory()) {
            File[] fs = path.listFiles();
            for (File f : fs) {
                if (f.isFile() && f.getName().startsWith("jwarmup_")) {
                    logfile = f;
                }
            }
        }
        assertTrue(logfile != null);
        try {
            logfile.delete();
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) throws Exception {
        testFlush(InnerA.class.getName());
    }

    public static class InnerA {
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

        public static void main(String[] args) throws Exception {
            InnerA a = new InnerA();
            a.foo();
            Thread.sleep(3000);
            a.foo();
            Thread.sleep(15000);
            System.out.println("process is done!");
        }
    }
}
