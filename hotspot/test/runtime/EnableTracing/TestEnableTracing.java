/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
 */

/*
 * @test TestEnableTracing
 * @summary Test if tracing produces the expected output when enabled.
 * @bug 8209863 8145788
 * @library /testlibrary
 * @run main TestEnableTracing
 */

import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.OutputAnalyzer;

public class TestEnableTracing {
    public static final String OPENJDK_MARK = "OpenJDK";

    public static void main(String[] args) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-XX:+EnableTracing", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        if (output.getStderr().contains(OPENJDK_MARK)) {
            output.shouldMatch("^Class Load");
            output.shouldContain("Loaded Class ="); // verify TraceStream print_val Klass*
        }
        output.shouldHaveExitValue(0);

        pb = ProcessTools.createJavaProcessBuilder("-XX:+EnableTracing", "-XX:+UseLockedTracing", "-Xcomp ", "-version");
        output = new OutputAnalyzer(pb.start());
        if (output.getStderr().contains(OPENJDK_MARK)) {
            output.shouldMatch("^Class Load");
            output.shouldMatch("^Compilation");
            output.shouldContain("Java Method ="); // verify TraceStream print_val Method*
        }
        output.shouldHaveExitValue(0);
    }
}
