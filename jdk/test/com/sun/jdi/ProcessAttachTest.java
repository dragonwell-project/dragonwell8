/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import jdk.testlibrary.JDKToolFinder;
import jdk.testlibrary.OutputAnalyzer;
import jdk.testlibrary.ProcessTools;

import com.sun.jdi.Bootstrap;
import com.sun.jdi.ThreadReference;
import com.sun.jdi.VirtualMachine;
import com.sun.jdi.connect.AttachingConnector;
import com.sun.jdi.connect.Connector;
import com.sun.jdi.connect.IllegalConnectorArgumentsException;

/**
 * @test
 * @bug 4527279
 * @summary Unit test for ProcessAttachingConnector
 *
 * @library /lib/testlibrary
 * @build jdk.testlibrary.* ProcessAttachTest
 * @run driver ProcessAttachTest
 */

class ProcessAttachTestTarg {
    public static void main(String args[]) throws Exception {
        // Write something that can be read by the driver
        System.out.println("Debuggee started");
        System.out.flush();
        for (;;) {
            Thread.sleep(100);
        }
    }
}

public class ProcessAttachTest {

    public static final String TESTCLASSES = System.getProperty("test.classes");

    public static void main(String[] args) throws Throwable {

        System.out.println("Test 1: Debuggee start with suspend=n");
        runTest("-agentlib:jdwp=transport=dt_socket,server=y,suspend=n");

        System.out.println("Test 2: Debuggee start with suspend=y");
        runTest("-agentlib:jdwp=transport=dt_socket,server=y,suspend=y");

    }

    private static void runTest(String jdwpArg) throws Throwable {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                jdwpArg,
                "-classpath", TESTCLASSES,
                "ProcessAttachTestTarg");
        Process p = null;
        try {
            // Get pid
            String jps = JDKToolFinder.getJDKTool("jps");
            OutputAnalyzer output = ProcessTools.executeProcess(jps);
            Pattern ptn1 = Pattern.compile("(\\d+) ProcessAttachTestTarg");
            Pattern ptn2 = Pattern.compile("(\\d+) -- main class information unavailable"); // The target process is displayed as such in Test 2.
            Matcher m1 = ptn1.matcher(output.getOutput());
            Matcher m2 = ptn2.matcher(output.getOutput());
            List<String> pids1 = new ArrayList<>();
            while (m1.find()) {
                pids1.add(m1.group(1));
            }
            while (m2.find()) {
                pids1.add(m2.group(1));
            }

            p = pb.start();

            // Wait for the process to start
            InputStream is = p.getInputStream();
            is.read();

            // Get pid
            output = ProcessTools.executeProcess(jps);
            m1 = ptn1.matcher(output.getOutput());
            m2 = ptn2.matcher(output.getOutput());
            List<String> pids2 = new ArrayList<>();
            while (m1.find()) {
                pids2.add(m1.group(1));
            }
            while (m2.find()) {
                pids2.add(m2.group(1));
            }
            pids2.removeAll(pids1);
            if (pids2.size() != 1) {
                throw new RuntimeException("Did not find pid");
            }

            // Attach a debugger
            tryDebug(Long.parseLong(pids2.get(0)));
        } finally {
            p.destroyForcibly();
        }
    }

    private static void tryDebug(long pid) throws IOException,
            IllegalConnectorArgumentsException {
        AttachingConnector ac = Bootstrap.virtualMachineManager().attachingConnectors()
                .stream()
                .filter(c -> c.name().equals("com.sun.jdi.ProcessAttach"))
                .findFirst()
                .orElseThrow(() -> new RuntimeException("Unable to locate ProcessAttachingConnector"));

        Map<String, Connector.Argument> args = ac.defaultArguments();
        Connector.StringArgument arg = (Connector.StringArgument) args
                .get("pid");
        arg.setValue("" + pid);

        System.out.println("Debugger is attaching to: " + pid + " ...");
        VirtualMachine vm = ac.attach(args);

        // list all threads
        System.out.println("Attached! Now listing threads ...");
        vm.allThreads().stream().forEach(System.out::println);

        System.out.println("Debugger done.");
        vm.dispose();
    }
}
