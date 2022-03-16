/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
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

import com.oracle.java.testlibrary.*;
import java.io.File;

/*
 * @test
 * @summary Test Wisp2 flag Compatibility
 * @requires os.family == "linux"
 * @library /testlibrary
 * @run main Wisp2FlagCompatibilityCheckTest
 */
public class Wisp2FlagCompatibilityCheckTest {
    public static void main(String[] args) throws Exception {
        ProcessBuilder pb;
        OutputAnalyzer output;

        pb = ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockExperimentalVMOptions",
                "-XX:+UseWisp2",
                "-XX:-UseWispMonitor",
                "-version");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Wisp2 needs to enable -XX:+UseWispMonitor");
        System.out.println(output.getOutput());

        pb = ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockExperimentalVMOptions",
                "-XX:+UseWisp2",
                "-XX:-EnableCoroutine",
                "-version");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Wisp2 needs to enable -XX:+EnableCoroutine");
        System.out.println(output.getOutput());

        pb = ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockExperimentalVMOptions",
                "-XX:+UseWisp2",
                "-XX:+UseBiasedLocking",
                "-version");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Biased Locking is not supported with Wisp2");
        System.out.println(output.getOutput());

        pb = ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockExperimentalVMOptions",
                "-XX:+EnableCoroutine",
                "-XX:+UseBiasedLocking",
                "-version");
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Biased Locking is not supported with Wisp");
        System.out.println(output.getOutput());
    }
}
