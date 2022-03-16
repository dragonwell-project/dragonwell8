/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8061651
 * @summary -Dsun.cds.enableSharedLookupCache specified on the command-line
 *          should have no effect.
 * @run main/othervm -Dsun.cds.enableSharedLookupCache=true -Xshare:off -Dfoo.foo.bar=xyz EnableLookupCache
 */

public class EnableLookupCache {
    public static void main(String[] args) throws Exception {
        // If JVM is started with -Xshare:off, the sun.cds.enableSharedLookupCache
        // should never be true, even if it has been explicitly set in the
        // command-line.
        String prop = "sun.cds.enableSharedLookupCache";
        String value = System.getProperty(prop);
        System.out.println("System.getProperty(\"" + prop + "\") = \"" + value+ "\"");

        if ("true".equals(value)) {
            System.out.println("Test FAILED: system property " + prop +
                               " is \"true\" (unexpected)");
            throw new RuntimeException(prop + " should not be " + value);
        }

        // Make sure the -D... arguments in the @run tag are indeed used.
        prop = "foo.foo.bar";
        value = System.getProperty(prop);
        System.out.println("System.getProperty(\"" + prop + "\") = \"" + value+ "\"");
        if (!"xyz".equals(value)) {
            System.out.println("Test FAILED: system property " + prop +
                               " should be \"xyz\" -- is JTREG set up properly?");
            throw new RuntimeException(prop + " should not be " + value);
        }


        // We should be able to load the other classes without issue.
        A.test();
        B.test();
        System.out.println("Test PASSED");
    }

    static class A {static void test() {}}
    static class B {static void test() {}}
}

