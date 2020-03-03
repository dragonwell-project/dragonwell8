/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package jdk.jfr.jvm;

import static jdk.test.lib.Asserts.assertEquals;
import java.lang.management.ManagementFactory;

import jdk.jfr.internal.JVM;

/**
 * @test TestPid
 * @key jfr
 *
 * @library /lib /
 *
 * @run main/othervm jdk.jfr.jvm.TestPid
 */
public class TestPid {

    private static long getProcessId() {

        // something like '<pid>@<hostname>', at least in SUN / Oracle JVMs
        final String jvmName = ManagementFactory.getRuntimeMXBean().getName();

        final int index = jvmName.indexOf('@');

        if (index < 1) {
            // part before '@' empty (index = 0) / '@' not found (index = -1)
            return 42;
        }

        try {
            return Long.parseLong(jvmName.substring(0, index));
        } catch (NumberFormatException e) {
            // ignore
        }
        return 42;
    }

    public static void main(String... args) throws InterruptedException {

        JVM jvm = JVM.getJVM();
        String pid = jvm.getPid();

        try {
            String managementPid = String.valueOf(getProcessId());
            assertEquals(pid, managementPid, "Pid doesn't match value returned by RuntimeMXBean");
        } catch (NumberFormatException nfe) {
            throw new AssertionError("Pid must be numeric, but was '" + pid + "'");
        }
    }

}
