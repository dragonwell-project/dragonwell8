/*
 * Copyright (c) 2022 Alibaba Group Holding Limited. All Rights Reserved.
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

import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

import java.util.ArrayList;
import java.util.List;


/*
 * @test
 * @summary Test the enviorment config
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main TestVendorVMOptions
 */
public class TestVendorVMOptions {
    public static void main(String[] args) throws Exception {
        ProcessBuilder pb;
        OutputAnalyzer outputAnalyzer;

        // default
        pb = createProcessBuilder();
        outputAnalyzer = new OutputAnalyzer(pb.start());
        outputAnalyzer.shouldHaveExitValue(0);

        // enable DRAGONWELL_JAVA_TOOL_OPTIONS
        pb = createProcessBuilder();
        updateEnvironment(pb, "DRAGONWELL_JAVA_TOOL_OPTIONS", "-XX:+UnlockExperimentalVMOptions -XX:+UseWisp2");
        outputAnalyzer = new OutputAnalyzer(pb.start());
        outputAnalyzer.shouldHaveExitValue(0);

        // enable invalid DRAGONWELL_JAVA_TOOL_OPTIONS
        pb = createProcessBuilder();
        updateEnvironment(pb, "DRAGONWELL_JAVA_TOOL_OPTIONS", "-XX:+UnlockExperimentalVMOptions -XX:+SomeUnknownOptions");
        outputAnalyzer = new OutputAnalyzer(pb.start());
        outputAnalyzer.shouldHaveExitValue(1);

        // enable invalid DRAGONWELL_JAVA_TOOL_OPTIONS
        pb = createProcessBuilder();
        updateEnvironment(pb, "DRAGONWELL_JAVA_TOOL_OPTIONS", "-XX:+UnlockExperimentalVMOptions -XX:+IgnoreUnrecognizedVMOptions -XX:+UnlockDiagnosticVMOptions -XX:+SomeUnknownOptions");
        outputAnalyzer = new OutputAnalyzer(pb.start());
        outputAnalyzer.shouldHaveExitValue(0);

        // enable JAVA_OPTIONS and DRAGONWELL_JAVA_TOOL_OPTIONS_JDK_ONLY version
        pb = createProcessBuilder();
        updateEnvironment(pb, "JAVA_TOOL_OPTIONS", "-XX:+UnlockExperimentalVMOptions -XX:+SomeUnknownOptions");
        updateEnvironment(pb, "DRAGONWELL_JAVA_TOOL_OPTIONS_JDK_ONLY", "true");
        outputAnalyzer = new OutputAnalyzer(pb.start());
        outputAnalyzer.shouldHaveExitValue(0);

        // enable a invalid JAVA_OPTIONS and DRAGONWELL_JAVA_TOOL_OPTIONS_JDK_ONLY jps
        Process p = Runtime.getRuntime().exec(System.getProperty("test.jdk") + "/bin/jps ", new String[]{"JAVA_OPTIONS=invliad", "DRAGONWELL_JAVA_TOOL_OPTIONS_JDK_ONLY=true"});
        outputAnalyzer = new OutputAnalyzer(p);
        outputAnalyzer.shouldHaveExitValue(0);

    }

    private static void updateEnvironment(ProcessBuilder pb, String name, String value) {
        pb.environment().put(name, value);
    }

    private static ProcessBuilder createProcessBuilder() throws Exception {
        ProcessBuilder pb;
        List<String> runJava = new ArrayList<>();
        runJava.add("-version");
        pb = ProcessTools.createJavaProcessBuilder(runJava.toArray(new String[0]));
        return pb;
    }
}