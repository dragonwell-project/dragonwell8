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
 *
 */


import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
import java.lang.management.ManagementFactory;
import com.sun.management.HotSpotDiagnosticMXBean;

/*
 * @test
 * @summary Test large array allocation warning "-XX:ArrayAllocationWarningSize=4M"
 * @library /testlibrary
 * @build TestArrayAllocationWarning
 * @run main TestArrayAllocationWarning
 */
public class TestArrayAllocationWarning {
    public static void main(String[] args) throws Exception {
        ProcessBuilder pb;
        OutputAnalyzer output;
        String srcPath = System.getProperty("test.class.path");

        pb = ProcessTools.createJavaProcessBuilder("-Xbootclasspath/a:.",
                "-cp", ".:" + srcPath,
                TestArrayAllocationWarningRunner.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("==WARNING==  allocating large array--");
        output.shouldContain("done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());

        pb = ProcessTools.createJavaProcessBuilder("-Xbootclasspath/a:.",
                "-cp", ".:" + srcPath,
                TestArrayAllocationWarningRunner.class.getName(),
                "ArrayAllocationWarningSize",
                "4M");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("==WARNING==  allocating large array--");
        output.shouldContain("done!");
        output.shouldHaveExitValue(0);
        System.out.println(output.getOutput());
    }

    public static class TestArrayAllocationWarningRunner {
        public static void allocateArray(int size) {
            Object[] array = new Object[size];
            // avoid optimized out
            System.out.println(array);
        }
        public static void main(String[] args) throws Exception {
            if (args.length == 0) {
                allocateArray(8 * 1000 * 1000);
            } else {
                assertTrue(args.length == 2);
                String key = args[0];
                String value = args[1].replace("M", "000000");
                System.out.println(key);
                System.out.println(value);
                HotSpotDiagnosticMXBean bean = (HotSpotDiagnosticMXBean)
                        ManagementFactory.getPlatformMXBean(HotSpotDiagnosticMXBean.class);
                System.out.println(bean.getVMOption(key).getValue());
                bean.setVMOption(key, value);
                allocateArray(8 * 1000 * 1000);
            }
            System.out.println("done!");
        }
    }
}

