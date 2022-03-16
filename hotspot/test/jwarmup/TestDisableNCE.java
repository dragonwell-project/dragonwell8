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
 * @test TestDisableNCE
 * @library /testlibrary /testlibrary/whitebox
 * @build TestDisableNCE
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm TestDisableNCE
 * @summary verify the methods submitted via JWarmup are not de-optimized because of NCE
 */
public class TestDisableNCE {

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
                "-XX:-Inline",
                "-XX:-UseSharedSpaces",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-CMSClassUnloadingEnabled",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:CompilationWarmUpRecordTime=10",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions",
                "-XX:+WhiteBoxAPI",
                "-cp", classPath,
                InnerA.class.getName(), "recording");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("foo");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        // test read logfile
        pb = ProcessTools.createJavaProcessBuilder("-XX:-TieredCompilation",
                "-Xbootclasspath/a:.",
                "-XX:+CompilationWarmUp",
                "-XX:-Inline",
                "-XX:-UseSharedSpaces",
                "-XX:CompilationWarmUpLogfile=./jitwarmup.log",
                "-XX:+PrintCompilationWarmUpDetail",
                "-XX:+PrintCompilation",
                "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
                "-XX:+CompilationWarmUpResolveClassEagerly",
                "-cp", classPath,
                InnerA.class.getName(), "startup");
        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        // output.shouldNotContain("made not entrant");
        output.shouldMatch("InnerA.foo.*success compiled");
        output.shouldNotMatch("foo.*made not entrant");
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
      public String cc;
    }

    public static class InnerA {
        static {
            System.out.println("InnerA initialize");
        }

        public Object[] aa;
        public Object bb;
        public static List<String> ls = new ArrayList<String>();

        public static void foo(InnerA a) {
            // null check test
            Object[] oo = a.aa;
            // class check test
            String[] ss = (String[])a.aa;
            // instanceof test
            if (a.bb instanceof InnerB) {
                System.out.println("avoid opitimize");
            }
            if (ss.length > 1) {
                System.out.println("avoid opitimize");
            }
        }

        public static void main(String[] args) throws Exception {
            if ("recording".equals(args[0])) {
                System.out.println("begin recording!");
                // initialize InnerA
                InnerA a = new InnerA();
                a.aa = new String[0];
                a.bb = new String("xx");
                for (int i = 0; i < 20000; i++) {
                    foo(a);
                }
                Thread.sleep(1000);
                foo(a);
                Thread.sleep(10000);
                System.out.println("done!");
            } else {
                System.out.println("begin startup!");
                JWarmUp.notifyApplicationStartUpIsDone();
                while (!JWarmUp.checkIfCompilationIsComplete()) {
                    Thread.sleep(1000);
                }
                try {
                    InnerA a = new InnerA();
                    a.aa = null;
                    a.bb = null;
                    foo(a);
                    foo(null);
                    // The above NULL values should not cause the de-optimization for JWarmup compiled methods.
                } catch (Exception e) {
                    System.out.println("NPE!");
                }
                System.out.println("done!");
            }
        }
    }
}
