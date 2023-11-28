/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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
 * @test
 * @bug 6515172
 * @summary Check that availableProcessors reports the correct value when running in a cpuset on linux
 * @requires os.family == "linux"
 * @library /testlibrary /test/lib
 * @build com.oracle.java.testlibrary.*
 * @run driver AvailableProcessors
 */

import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.OutputAnalyzer;
import jtreg.SkippedException;

import java.util.ArrayList;
import java.io.File;

public class AvailableProcessors {

    static final String SUCCESS_STRING = "Found expected processors: ";

    public static void main(String[] args) throws Throwable {
        if (args.length > 0)
            checkProcessors(Integer.parseInt(args[0]));
        else {
            // run ourselves under different cpu configurations
            // using the taskset command
            String taskset;
            final String taskset1 = "/bin/taskset";
            final String taskset2 = "/usr/bin/taskset";
            if (new File(taskset1).exists()) {
                taskset = taskset1;
            } else if (new File(taskset2).exists()) {
                taskset = taskset2;
            } else {
                throw new SkippedException("Could not find taskset command");
            }

            int available = Runtime.getRuntime().availableProcessors();

            if (available == 1) {
                throw new SkippedException("only one processor available");
            }

            // Get the java command we want to execute
            // Enable logging for easier failure diagnosis
            ProcessBuilder master =
                    ProcessTools.createJavaProcessBuilder(false,
                                                          "-XX:+UnlockDiagnosticVMOptions",
                                                          "-XX:+PrintActiveCpus",
                                                          "AvailableProcessors");

            int[] expected = new int[] { 1, available/2, available-1, available };

            for (int i : expected) {
                System.out.println("Testing for " + i + " processors ...");
                int max = i - 1;
                ArrayList<String> cmdline = new ArrayList<>(master.command());
                // prepend taskset command
                cmdline.add(0, "0-" + max);
                cmdline.add(0, "-c");
                cmdline.add(0, taskset);
                // append expected processor count
                cmdline.add(String.valueOf(i));
                ProcessBuilder pb = new ProcessBuilder(cmdline);
                System.out.println("Final command line: " +
                                   ProcessTools.getCommandLine(pb));
                OutputAnalyzer output = ProcessTools.executeProcess(pb);
                output.shouldContain(SUCCESS_STRING);
            }
        }
    }

    static void checkProcessors(int expected) {
        int available = Runtime.getRuntime().availableProcessors();
        if (available != expected)
            throw new Error("Expected " + expected + " processors, but found "
                            + available);
        else
            System.out.println(SUCCESS_STRING + available);
    }
}
