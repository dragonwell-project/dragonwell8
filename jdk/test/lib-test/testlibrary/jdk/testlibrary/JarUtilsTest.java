/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
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

/* @test
 * @bug 8309841
 * @summary Unit Test for a common Test API in jdk.testlibrary.JarUtils
 * @library /lib/testlibrary
 */

import jdk.testlibrary.Asserts;
import jdk.testlibrary.JarUtils;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashSet;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.stream.Collectors;

public class JarUtilsTest {
    public static void main(String[] args) throws Exception {
        Files.createDirectory(Paths.get("bx"));
        JarUtils.createJarFile(Paths.get("a.jar"),
                Paths.get("."),
                Files.write(Paths.get("a"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("b1"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("b2"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("bx/x"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("c"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("e1"), "".getBytes(StandardCharsets.UTF_8)),
                Files.write(Paths.get("e2"), "".getBytes(StandardCharsets.UTF_8)));
        checkContent("a", "b1", "b2", "bx/x", "c", "e1", "e2");

        JarUtils.deleteEntries(Paths.get("a.jar"), "a");
        checkContent("b1", "b2", "bx/x", "c", "e1", "e2");

        // Note: b* covers everything starting with b, even bx/x
        JarUtils.deleteEntries(Paths.get("a.jar"), "b*");
        checkContent("c", "e1", "e2");

        // d* does not match
        JarUtils.deleteEntries(Paths.get("a.jar"), "d*");
        checkContent("c", "e1", "e2");

        // multiple patterns
        JarUtils.deleteEntries(Paths.get("a.jar"), "d*", "e*");
        checkContent("c");
    }

    static void checkContent(String... expected) throws IOException {
        try (JarFile jf = new JarFile("a.jar")) {
            Asserts.assertEquals(new HashSet<>(Arrays.asList(expected)),
                    jf.stream().map(JarEntry::getName).collect(Collectors.toSet()));
        }
    }
}
