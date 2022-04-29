/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

package jdk.testlibrary;

import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class SecurityTools {

    public static final String RESPONSE_FILE = "security_tools_response.txt";

    private static ProcessBuilder getProcessBuilder(String tool, List<String> args) {
        JDKToolLauncher launcher = JDKToolLauncher.createUsingTestJDK(tool)
                .addVMArg("-Duser.language=en")
                .addVMArg("-Duser.country=US")
                .addVMArg("-Djava.security.egd=file:/dev/./urandom");
        for (String arg : args) {
            if (arg.startsWith("-J")) {
                launcher.addVMArg(arg.substring(2));
            } else {
                launcher.addToolArg(arg);
            }
        }
        return new ProcessBuilder(launcher.getCommand());
    }

    // keytool

    public static OutputAnalyzer keytool(List<String> args)
            throws Exception {

        ProcessBuilder pb = getProcessBuilder("keytool", args);

        Path p = Paths.get(RESPONSE_FILE);
        if (!Files.exists(p)) {
            Files.createFile(p);
        }
        pb.redirectInput(ProcessBuilder.Redirect.from(new File(RESPONSE_FILE)));

        try {
            return execute(pb);
        } catch (Throwable t) {
            throw new RuntimeException("keytool failure: " + t);
        } finally {
            Files.delete(p);
        }
    }

    // Only call this if there is no white space in every argument
    public static OutputAnalyzer keytool(String args) throws Exception {
        return keytool(args.split("\\s+"));
    }

    public static OutputAnalyzer keytool(String... args) throws Exception {
        return keytool(Arrays.asList(args));
    }

    public static void setResponse(String... responses) throws IOException {
        String text;
        if (responses.length > 0) {
            text = Stream.of(responses).collect(
                    Collectors.joining("\n", "", "\n"));
        } else {
            text = "";
        }
        Files.write(Paths.get(RESPONSE_FILE), text.getBytes());
    }

    // jarsigner

    public static OutputAnalyzer jarsigner(List<String> args)
            throws Exception {
        return execute(getProcessBuilder("jarsigner", args));
    }

    private static OutputAnalyzer execute(ProcessBuilder pb) throws Exception {
        try {
            OutputAnalyzer oa = ProcessTools.executeCommand(pb);
            System.out.println("Exit value: " + oa.getExitValue());
            return oa;
        } catch (Throwable t) {
            if (t instanceof Exception) {
                throw (Exception) t;
            } else {
                throw new Exception(t);
            }
        }
    }

    // Only call this if there is no white space in every argument
    public static OutputAnalyzer jarsigner(String args) throws Exception {

        return jarsigner(args.split("\\s+"));
    }

    public static OutputAnalyzer jarsigner(String... args) throws Exception {
        return jarsigner(Arrays.asList(args));
    }

    // Create a temporary keychain in macOS and use it. The original
    // keychains will be restored when the object is closed.
    public static class TemporaryKeychain implements Closeable {
        // name of new keychain
        private final String newChain;
        // names of the original keychains
        private final List<String> oldChains;

        public TemporaryKeychain(String name) {
            File f = new File(name + ".keychain-db");
            newChain = f.getAbsolutePath();
            try {
                oldChains = new ArrayList<String>();
                List<String> ls = ProcessTools.executeProcess("security", "list-keychains")
                        .shouldHaveExitValue(0)
                        .asLines();
                for (String l: ls) {
                    l = l.trim();
                    if (l.startsWith("\"")) {
                        oldChains.add(l.substring(1, l.length() - 1));
                    } else {
                        oldChains.add(l);
                    }
                }
                if (!f.exists()) {
                    ProcessTools.executeProcess("security", "create-keychain", "-p", "changeit", newChain)
                            .shouldHaveExitValue(0);
                }
                ProcessTools.executeProcess("security", "unlock-keychain", "-p", "changeit", newChain)
                        .shouldHaveExitValue(0);
                ProcessTools.executeProcess("security", "list-keychains", "-s", newChain)
                        .shouldHaveExitValue(0);
            } catch (Throwable t) {
                if (t instanceof RuntimeException) {
                    throw (RuntimeException)t;
                } else {
                    throw new RuntimeException(t);
                }
            }
        }

        public String chain() {
            return newChain;
        }

        @Override
        public void close() throws IOException {
            List<String> cmds = new ArrayList<String>(Arrays.asList("security", "list-keychains", "-s"));
            cmds.addAll(Collections.unmodifiableList(oldChains));
            try {
                ProcessTools.executeProcess(cmds.toArray(new String[0]))
                        .shouldHaveExitValue(0);
            } catch (Throwable t) {
                if (t instanceof RuntimeException) {
                    throw (RuntimeException)t;
                } else {
                    throw new RuntimeException(t);
                }
            }
        }
    }
}
