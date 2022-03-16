/*
 * Copyright (c) 2021, Alibaba Group Holding Limited. All rights reserved.
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

import static com.oracle.java.testlibrary.Asserts.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.lang.reflect.Field;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.oracle.java.testlibrary.JDKToolLauncher;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

/**
 * @test metaspace dump
 * @library /testlibrary
 * @run main/othervm HeapDumpPathTest
 */
public class HeapDumpPathTest {

    private static String               DEFAULT_DUMP_FILE_NAME = "java_pid%d.hprof";

    private static String               CUSTOM_DUMP_FILE = "dump.bin";

    public static void main(String[] args) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xms10m", "-Xmx10m", "-Xmn5m",
                                                                  "-XX:+HeapDumpBeforeFullGC",
                                                                  "-XX:+HeapDumpAfterFullGC",
                                                                  "-XX:+HeapDumpOnOutOfMemoryError",
                                                                  "OOM");
        Process p = pb.start();
        OutputAnalyzer analyzer = new OutputAnalyzer(p);
        int pid = getPid(p);
        String fileName = String.format(DEFAULT_DUMP_FILE_NAME, pid);
        checkFile(fileName);
        checkFile(fileName + ".1");
        checkFile(fileName + ".2");

        pb = ProcessTools.createJavaProcessBuilder("-Xms10m", "-Xmx10m", "-Xmn5m",
                                                   "-XX:+HeapDumpBeforeFullGC",
                                                   "-XX:+HeapDumpAfterFullGC",
                                                   "-XX:+HeapDumpOnOutOfMemoryError",
                                                   "-XX:HeapDumpPath=" + CUSTOM_DUMP_FILE,
                                                   "OOM");
        p = pb.start();
        analyzer = new OutputAnalyzer(p);
        fileName = CUSTOM_DUMP_FILE;
        checkFile(fileName);
        checkFile(fileName + ".1");
        checkFile(fileName + ".2");
    }

    private static void checkFile(String fileName) throws Exception {
        File file = new File(fileName);
        assertTrue(file.exists());
    }

    private static int getPid(Process process) throws Exception {

        Class<?> ProcessImpl = process.getClass();
        Field field = ProcessImpl.getDeclaredField("pid");
        field.setAccessible(true);
        return field.getInt(process);
    }
}

class OOM {
    public static void main(String[] args) throws Exception {
        List<byte[]> list = new ArrayList<>();
        for (int i = 0; i < 1000000; i++) {
            list.add(new byte[1024*1024]);
        }
    }
}
