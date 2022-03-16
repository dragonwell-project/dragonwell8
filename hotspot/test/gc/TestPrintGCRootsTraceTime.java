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
 *
 */

/*
 * @test TestPrintGCDetailTraceTime
 * @summary test print gc detail time
 * @library /testlibrary
 * @build TestPrintGCRootsTraceTime
 * @run main/othervm TestPrintGCRootsTraceTime
 */

import java.util.ArrayList;
import com.oracle.java.testlibrary.*;

public class TestPrintGCRootsTraceTime {
    public static void main(String[] args) throws Exception {
        ProcessBuilder pb = null;
        OutputAnalyzer output = null;
        // test with the flag on
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmx200m",
                "-XX:+UseParNewGC",
                "-XX:+UseConcMarkSweepGC",
                "-XX:+PrintGCDetails",
                "-verbose:gc",
                "-XX:+PrintGCRootsTraceTime",
                Foo.class.getName()
                );
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Root Processing");
        String [] strOut = output.getStdout().split("\n");
        for (int i = 0; i < 11; i++) {
            System.out.println(strOut[i]);
        }
        output.shouldHaveExitValue(0);

        // test with the flag off
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseParNewGC",
                "-XX:+UseConcMarkSweepGC",
                "-XX:+PrintGCDetails",
                "-verbose:gc",
                "-XX:-PrintGCRootsTraceTime",
                Foo.class.getName()
                );
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test serial gc
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseSerialGC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test g1 gc
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseG1GC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test ps gc
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseParallelGC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test ps young gc
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseParallelGC",
                "-XX:-UseParallelOldGC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing"); 
        output.shouldHaveExitValue(0);

        // test cms without ParNew
         pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseConcMarkSweepGC",
                "-XX:-UseParNewGC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldNotContain("Root Processing"); 
        output.shouldHaveExitValue(0);

        // test cms
         pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseConcMarkSweepGC",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test cms with parallel gc threads = 1
        pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+UseConcMarkSweepGC",
                "-XX:+PrintGCDetails",
                "-XX:ParallelGCThreads=1",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                Foo.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldContain("Root Processing");
        output.shouldHaveExitValue(0);

        // test system gc
         pb = ProcessTools.createJavaProcessBuilder( "-Xmx500m",
                "-Xmn200m",
                "-XX:+PrintGCDetails",
                "-XX:+PrintGCRootsTraceTime",
                "-verbose:gc",
                TestSystemGC.class.getName());
        output = new OutputAnalyzer(pb.start());
        output.shouldHaveExitValue(0);
    }

    // class for testing gc
    public static class Foo {
        public static void main(String []args) {
            int count = 0;
            ArrayList list = new ArrayList();
            for (int i = 0; ; i++) {
                byte[] data = new byte[1024];
                list.add(data);
                if (i % 1024 == 0) {
                    count++;
                    list.clear();
                    if (count > 1024) {
                        break;
                    }
                }
            }
        }
    }
    // class for testing system gc
    public static class TestSystemGC {
        public static void main(String []args) throws Exception {
            System.gc();
        }
    }
}
