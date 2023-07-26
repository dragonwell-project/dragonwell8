/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.LinkedList;
import java.util.List;
import static com.oracle.java.testlibrary.Asserts.*;

/**
 * @test TestNoMinidumpAtFullGC
 * @summary Test to enasure mini-heap-dump not triggered when -XX:+HeapDumpBeforeFullGC
 * @library /testlibrary
 * @build TestNoMinidumpAtFullGC
 * @run main/othervm TestNoMinidumpAtFullGC
 */

public class TestNoMinidumpAtFullGC {

    private static final int M = 1024 * 1024;

    // child process which creates a lot of primitive array and will OOM anyway
    public static class OOMWorker {
        public static void main(String[] args) {
            List<Object> holder = new LinkedList<>();
            while (true) {
                holder.add(new int[4096]);
            }
        }
    }

    public static void main(String[] args) {
        String[] heapDumpOomOpts = {
                "-XX:+HeapDumpBeforeFullGC",
                "-XX:+HeapDumpAfterFullGC",
                "-XX:+HeapDumpOnOutOfMemoryError",
        };
        try {
            for (String oomOpt : heapDumpOomOpts) {
                ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                        "-Xmx128m",
                        "-Xms128m",
                        "-XX:+UseConcMarkSweepGC",
                        oomOpt,
                        OOMWorker.class.getName()
                );
                OutputAnalyzer output = new OutputAnalyzer(pb.start());
                output.shouldContain("java.lang.OutOfMemoryError: Java heap space");
                output.shouldContain("Dumping heap to");
                assertNotEquals(0, output.getExitValue());
                Files.walk(Paths.get("."))
                        .filter(p -> p.toString().contains("java_pid"))
                        // if mini-heap-dump triggered, the dump size is only several mega-bytes;
                        // otherwise, it would be definitely more than 100MB
                        .forEach(p -> assertGreaterThan(p.toFile().length(), 100L * M));
            }
        } catch (Exception e) {
            e.printStackTrace();
            fail();
        }
    }
}
