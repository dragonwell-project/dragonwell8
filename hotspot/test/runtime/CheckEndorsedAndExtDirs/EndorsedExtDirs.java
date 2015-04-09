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

/*
 * @test
 * @bug 8064667
 * @summary Sanity test for -XX:+CheckEndorsedAndExtDirs
 * @library /testlibrary
 * @run main/othervm EndorsedExtDirs
 */

import com.oracle.java.testlibrary.*;
import java.io.File;
import java.io.IOException;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class EndorsedExtDirs {
    static final String cpath = System.getProperty("test.classes", ".");
    public static void main(String arg[]) throws Exception {
        fatalError("-XX:+CheckEndorsedAndExtDirs", "-Djava.endorsed.dirs=foo");
        fatalError("-XX:+CheckEndorsedAndExtDirs", "-Djava.ext.dirs=bar");
        testNonEmptySystemExtDirs();
    }

    static void testNonEmptySystemExtDirs() throws Exception {
        String home = System.getProperty("java.home");
        Path ext = Paths.get(home, "lib", "ext");
        String extDirs = System.getProperty("java.ext.dirs");
        String[] dirs = extDirs.split(File.pathSeparator);
        long count = 0;
        for (String d : dirs) {
            Path path = Paths.get(d);
            if (Files.notExists(path) || path.equals(ext)) continue;
            count += Files.find(path, 1, (Path p, BasicFileAttributes attr)
                                       -> p.getFileName().toString().endsWith(".jar"))
                          .count();
        }
        if (count > 0) {
            fatalError("-XX:+CheckEndorsedAndExtDirs");
        }
    }

    static ProcessBuilder newProcessBuilder(String... args) {
        List<String> commands = new ArrayList<>();
        String java = System.getProperty("java.home") + "/bin/java";
        commands.add(java);
        for (String s : args) {
            commands.add(s);
        }
        commands.add("-cp");
        commands.add(cpath);
        commands.add("EndorsedExtDirs");

        System.out.println("Process " + commands);
        return new ProcessBuilder(commands);
    }

    static void fatalError(String... args) throws Exception {
        fatalError(newProcessBuilder(args));
    }

    static void fatalError(ProcessBuilder pb) throws Exception {
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Could not create the Java Virtual Machine");
        output.shouldHaveExitValue(1);
    }
}
