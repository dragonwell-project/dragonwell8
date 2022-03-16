
/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
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
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary Test GC cause in GC log for tenant container
 * @library /testlibrary
 * @build TestTenantGCCause
 * @run main/othervm/timeout=120 TestTenantGCCause
 */

import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;
import java.util.LinkedList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import static com.oracle.java.testlibrary.Asserts.*;

public class TestTenantGCCause {

    private static final int K = 1024;

    // used by child process to create a new TenantContainer with given heap limit,
    // and do allocation of given amount of bytes inside the TenantContainer;
    static class AllocWorker {
        // limit of TenantContainer, parsed from args[0];
        private static long tenantLimit;
        // total amount of bytes to be allocated, parsed from args[1];
        private static long bytesToAlloc;

        public static void main(String[] args) {
            parseArgs(args);
            TenantConfiguration config = new TenantConfiguration();
            config.limitHeap(tenantLimit);
            TenantContainer tenant = TenantContainer.create(config);
            try {
                tenant.run(()->{
                    List<byte[]> refs = new LinkedList<>(); // keep objects alive
                    long totalAllocated = 0;
                    while (totalAllocated < bytesToAlloc) {
                        byte[] obj = new byte[4 * K];
                        refs.add(obj);
                        totalAllocated += (obj.length + 16);
                    }
                });
            } catch (TenantException e) {
                e.printStackTrace();
                fail();
            }
        }

        // parse size, can be '1234', '1K', '4M'
        private static void parseArgs(String[] args) {
            assertEquals(args.length, 2);
            tenantLimit = parseSize(args[0]);
            assertGreaterThan(tenantLimit, 0L);
            bytesToAlloc = parseSize(args[1]);
        }

        private static final Pattern UNIT_PATTERN = Pattern.compile("(\\d+)([MmKkGg]{0,1})");

        private static long parseSize(String s) {
            Matcher m = UNIT_PATTERN.matcher(s);
            long size = 0;
            if (m.matches()) {
                size = Long.parseLong(m.group(1));
                if (m.groupCount() == 2) {
                    switch (m.group(2).toLowerCase()) {
                        case "k":
                            return (size * K);
                        case "m":
                            return (size * K * K);
                        case "g":
                            return (size * K * K * K);
                        default:
                            fail();
                    }
                }
            }
            throw new IllegalArgumentException("Bad size format: " + s);
        }
    }

    private void testYoungGCCause() {
        try {
            ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-XX:+MultiTenant",
                    "-XX:+TenantHeapThrottling",
                    "-XX:+UseG1GC",
                    "-Xmx1g",
                    "-Xms1g",
                    "-XX:G1HeapRegionSize=2m",
                    "-XX:+PrintGCDetails",
                    AllocWorker.class.getName(),
                    "4m",
                    "8m");
            OutputAnalyzer out = new OutputAnalyzer(pb.start());
            assertTrue(out.asLines().stream()
                    .sequential()
                    .peek(System.out::println)
                    .filter(l -> l.contains("[GC pause"))
                    .anyMatch(l -> l.contains("(tenant-0)")));
            assertNotEquals(out.getExitValue(), 0);
        } catch (Exception e) {
            e.printStackTrace();
            fail();
        }
    }

    private void testFullGCCause() {
        try {
            ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-XX:+MultiTenant",
                    "-XX:+TenantHeapThrottling",
                    "-XX:+UseG1GC",
                    "-Xmx1g",
                    "-Xms1g",
                    "-XX:G1HeapRegionSize=2m",
                    "-XX:+PrintGCDetails",
                    AllocWorker.class.getName(),
                    "4m",
                    "8m");
            OutputAnalyzer out = new OutputAnalyzer(pb.start());
            assertTrue(out.asLines().stream()
                    .sequential()
                    .peek(System.out::println)
                    .filter(l -> l.contains("[Full GC"))
                    .anyMatch(l -> l.contains("(tenant-0)")));
            assertNotEquals(out.getExitValue(), 0);
        } catch (Exception e) {
            e.printStackTrace();
            fail();
        }
    }

    public static void main(String[] args) {
        TestTenantGCCause test = new TestTenantGCCause();
        test.testYoungGCCause();
        test.testFullGCCause();
    }
}
