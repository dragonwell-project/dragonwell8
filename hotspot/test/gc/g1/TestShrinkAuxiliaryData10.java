/*
 * Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test TestShrinkAuxiliaryData10
 * @bug 8038423 8061715
 * @summary Checks that decommitment occurs for JVM with different
 * G1ConcRSLogCacheSize and ObjectAlignmentInBytes options values
 * @requires vm.gc=="G1" | vm.gc=="null"
 * @library /testlibrary /testlibrary/whitebox /test/lib
 * @build com.oracle.java.testlibrary.* sun.hotspot.WhiteBox
 * @build TestShrinkAuxiliaryData TestShrinkAuxiliaryData10
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 *                              sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run driver/timeout=720 TestShrinkAuxiliaryData10
 */
public class TestShrinkAuxiliaryData10 {

    public static void main(String[] args) throws Exception {
        new TestShrinkAuxiliaryData(10).test();
    }
}
