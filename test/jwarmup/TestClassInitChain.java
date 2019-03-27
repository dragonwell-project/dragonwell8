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
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.*;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
/*
 * @test TestClassInitChain
 * @library /testlibrary /testlibrary/whitebox
 * @build TestClassInitChain
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestClassInitChain
 * @summary test ClassInitChain structure
 */
public class TestClassInitChain {
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
                "-XX:CompilationWarmUpRecordTime=15",
                "-XX:CompilationWarmUpAppID=123",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                className, "collection");
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

    public static OutputAnalyzer testReadLogfileAndGetResult(String filename) throws Exception {
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
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "compilation");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        return output;
    }

    public static void main(String[] args) throws Exception {
        OutputAnalyzer output = null;
        String fileName = generateOriginLogfile();
        // test origin log file
        output = testReadLogfileAndGetResult(fileName);
        output.shouldContain("Test Class Init Chain OK");
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
            // avoid optimization
            System.out.println("b is" + b);
            Thread.sleep(20000);
            a.foo();
            System.out.println("process is done!");
        }

        public static void main(String[] args) throws Exception {
            if (args[0].equals("collection")) {
                doBiz();
            } else if (args[0].equals("compilation")) {
                whiteBox = WhiteBox.getWhiteBox();
                String classAName = InnerA.class.getName();
                String classBName = InnerB.class.getName();
                String[] classList = whiteBox.getClassChainSymbolList();
                System.out.println("class list length is " + classList.length);
                int[] stateList = whiteBox.getClassChainStateList();
                int orderA = -1;
                int orderB = -1;
                int stateA = -1;
                int stateB = -1;
                for (int i = 0; i < classList.length; i++) {
                    if (classList[i] == null) {
                        continue;
                    }
                    if (classList[i].equals(classAName)) {
                        orderA = i;
                        stateA = stateList[i];
                    }
                    if (classList[i].equals(classBName)) {
                        orderB = i;
                        stateB = stateList[i];
                    }
                }
                assertTrue(orderA != -1);
                assertTrue(orderB != -1);
                assertTrue(orderB > orderA);
                assertTrue(stateA >= 2);
                assertTrue(stateB >= 2);

                System.out.println("Test Class Init Chain OK");
            }
        }
    }
}
