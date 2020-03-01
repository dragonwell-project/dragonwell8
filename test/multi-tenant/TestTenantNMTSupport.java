
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
 * @summary Verify PrintNMTStatistics with TenantHeapThrottling enabled
 * @library /testlibrary
 */

import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.oracle.java.testlibrary.*;

public class TestTenantNMTSupport {

    public static void main(String args[]) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                "-XX:+UnlockDiagnosticVMOptions",
                "-XX:+IgnoreUnrecognizedVMOptions",
                "-XX:+MultiTenant",
                "-XX:+TenantHeapThrottling",
                "-XX:+UseG1GC",
                "-XX:+PrintNMTStatistics",
                "-XX:NativeMemoryTracking=detail",
                TestingTenant.class.getName());

        OutputAnalyzer output_detail = new OutputAnalyzer(pb.start());
        output_detail.shouldHaveExitValue(0);

        output_detail.shouldContain("Virtual memory map:");
        output_detail.shouldContain("Details:");
        output_detail.shouldContain("Tenant (reserved=");
        output_detail.shouldContain("G1TenantAllocationContexts::initialize");
        output_detail.shouldNotContain("error");
    }

    static class TestingTenant {
        public static void main(String[] args) throws Exception{
            TenantConfiguration config = new TenantConfiguration().limitHeap(32 * 1024 * 1024);
            TenantContainer tenant = TenantContainer.create(config);
            tenant.run(() -> System.out.println("Hello"));
        }
    }
}
