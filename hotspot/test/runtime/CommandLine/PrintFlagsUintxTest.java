/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8280963
 * @summary "%-16lu" formatting string is used to format uintx (uintptr_t)
 *          flag values for output. uintx is 64-bit on win64, and "lu" format
 *          is intended to be used with unsigned long that is 32-bit on win64.
 *          Thus flag values that are exact multiple of 4 GiB will be formatted
 *          into 0 in PrintFlags output.
 * @library /testlibrary
 */

import com.oracle.java.testlibrary.*;

public class PrintFlagsUintxTest {
    public static void main(String[] args) throws Exception {
        if (!Platform.is64bit()) {
            System.out.println("Test needs a 4GB heap and can only be run as a 64bit process, skipping.");
            return;
        }

        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                "-Xmx4g", "-XX:+PrintFlagsFinal", "-version");

        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.stdoutShouldMatch(".*MaxHeapSize\\s+:= 4294967296\\s+.*");
    }
}
