/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

/** @test
 *  @bug 8068517
 *  @summary Verify that nested enums have correct abstract flag in the InnerClasses attribute.
 *  @library /tools/javac/lib
 *  @build ToolBox T8068517
 *  @run main T8068517
 */

import com.sun.tools.javac.util.Assert;
import java.io.File;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import javax.tools.JavaCompiler;
import javax.tools.ToolProvider;

public class T8068517 {

    public static void main(String[] args) throws Exception {
        new T8068517().run();
    }

    void run() throws Exception {
        runTest("class A {\n" +
                "    enum AInner implements Runnable {\n" +
                "        A {\n" +
                "            public void run() {}\n" +
                "        };\n" +
                "    }\n" +
                "}\n",
                "class B {\n" +
                "    A.AInner a;\n" +
                "}");
        runTest("class A {\n" +
                "    enum AInner implements Runnable {\n" +
                "        A {\n" +
                "            public void run() {}\n" +
                "        };\n" +
                "    }\n" +
                "    AInner aInner;\n" +
                "}\n",
                "class B {\n" +
                "    void test(A a) {;\n" +
                "        switch (a.aInner) {\n" +
                "            case A: break;\n" +
                "        }\n" +
                "    };\n" +
                "}");
        runTest("class A {\n" +
                "    enum AInner implements Runnable {\n" +
                "        A {\n" +
                "            public void run() {}\n" +
                "        };\n" +
                "    }\n" +
                "    AInner aInner;\n" +
                "}\n",
                "class B {\n" +
                "    void test(A a) {;\n" +
                "        System.err.println(a.aInner.toString());\n" +
                "    };\n" +
                "}");
        runTest("class A {\n" +
                "    enum AInner implements Runnable {\n" +
                "        A {\n" +
                "            public void run() {}\n" +
                "        };\n" +
                "    }\n" +
                "    AInner aInner() {\n" +
                "        return null;\n" +
                "    }\n" +
                "}\n",
                "class B {\n" +
                "    void test(A a) {;\n" +
                "        System.err.println(a.aInner().toString());\n" +
                "    };\n" +
                "}");
    }

    JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
    int testN = 0;

    void runTest(String aJava, String bJava) throws Exception {
        File testClasses = new File(System.getProperty("test.classes"));
        File target1 = new File(testClasses, "T8068517s" + testN++);
        doCompile(target1, aJava, bJava);
        File target2 = new File(testClasses, "T8068517s" + testN++);
        doCompile(target2, bJava, aJava);

        Assert.check(Arrays.equals(Files.readAllBytes(new File(target1, "B.class").toPath()),
                                   Files.readAllBytes(new File(target2, "B.class").toPath())));
    }

    void doCompile(File target, String... sources) throws Exception {
        target.mkdirs();
        List<String> options = Arrays.asList("-d", target.getAbsolutePath());
        List<ToolBox.JavaSource> files = Stream.of(sources)
                                               .map(ToolBox.JavaSource::new)
                                               .collect(Collectors.toList());
        compiler.getTask(null, null, null, options, null, files).call();
    }
}
