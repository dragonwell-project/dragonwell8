/*
 * Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 6994413
 * @summary Check the JDK and JVM version returned by sun.misc.Version
 *          matches the versions defined in the system properties
 * @compile -XDignore.symbol.file Version.java
 * @run main Version
 */

import java.util.regex.*;
import static sun.misc.Version.*;

public class Version {

    public static void main(String[] args) throws Exception {
        VersionInfo jdk = jdkVersionInfo(System.getProperty("java.runtime.version"));
        VersionInfo v1 = new VersionInfo(jdkMajorVersion(),
                                         jdkMinorVersion(),
                                         jdkMicroVersion(),
                                         jdkUpdateVersion(),
                                         jdkSpecialVersion(),
                                         jdkBuildNumber());
        System.out.println("JDK version = " + jdk + "  " + v1);
        if (!jdk.equals(v1)) {
            throw new RuntimeException("Unmatched version: " + jdk + " vs " + v1);
        }
        VersionInfo jvm = jvmVersionInfo(System.getProperty("java.vm.version"));
        VersionInfo v2 = new VersionInfo(jvmMajorVersion(),
                                         jvmMinorVersion(),
                                         jvmMicroVersion(),
                                         jvmUpdateVersion(),
                                         jvmSpecialVersion(),
                                         jvmBuildNumber());
        System.out.println("JVM version = " + jvm + " " + v2);
        if (!jvm.equals(v2)) {
            throw new RuntimeException("Unmatched version: " + jvm + " vs " + v2);
        }
    }

    static class VersionInfo {
        final int major;
        final int minor;
        final int micro;
        final int update;
        final String special;
        final int build;
        VersionInfo(int major, int minor, int micro,
                    int update, String special, int build) {
            this.major = major;
            this.minor = minor;
            this.micro = micro;
            this.update = update;
            this.special = special;
            this.build = build;
        }

        public boolean equals(VersionInfo v) {
            return (this.major == v.major && this.minor == v.minor &&
                    this.micro == v.micro && this.update == v.update &&
                    this.special.equals(v.special) && this.build == v.build);
        }

        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append(major + "." + minor + "." + micro);
            if (update > 0) {
                sb.append("_" + update);
            }

            if (!special.isEmpty()) {
                sb.append(special);
            }
            sb.append("-b" + build);
            return sb.toString();
        }
    }

    private static VersionInfo jdkVersionInfo(String version) throws Exception {
        // valid format of the version string is:
        // <major>.<minor>[.<micro>][_uu[c]][-<identifier>]-bxx
        int major = 0;
        int minor = 0;
        int micro = 0;
        int update = 0;
        String special = "";
        int build = 0;

        String regex = "^([0-9]{1,2})";     // major
        regex += "\\.";                     // separator
        regex += "([0-9]{1,2})";            // minor
        regex += "(\\.";                    // separator
        regex +=   "([0-9]{1,2})";          // micro
        regex += ")?";                      // micro is optional
        regex += "(_";
        regex +=   "([0-9]{2,3})";          // update
        regex +=   "([a-z])?";              // special char (optional)
        regex += ")?";                      // _uu[c] is optional
        regex += ".*";                      // -<identifier>
        regex += "(\\-b([0-9]{1,3}$))";     // JDK -bxx

        Pattern p = Pattern.compile(regex);
        Matcher m = p.matcher(version);
        m.matches();

        major = Integer.parseInt(m.group(1));
        minor = Integer.parseInt(m.group(2));
        micro = (m.group(4) == null) ? 0 : Integer.parseInt(m.group(4));
        update = (m.group(6) == null) ? 0 : Integer.parseInt(m.group(6));
        special = (m.group(7) == null) ? "" : m.group(7);
        build = Integer.parseInt(m.group(9));

        VersionInfo vi = new VersionInfo(major, minor, micro, update, special, build);
        System.out.printf("jdkVersionInfo: input=%s output=%s\n", version, vi);
        return vi;
    }

    private static VersionInfo jvmVersionInfo(String version) throws Exception {
        try {
            // valid format of the version string is:
            // <major>.<minor>-bxx[-<identifier>][-<debug_flavor>]
            int major = 0;
            int minor = 0;
            int build = 0;

            String regex = "^([0-9]{1,2})";     // major
            regex += "\\.";                     // separator
            regex += "([0-9]{1,2})";            // minor
            regex += "(\\-b([0-9]{1,3}))";      // JVM -bxx
            regex += ".*";

            Pattern p = Pattern.compile(regex);
            Matcher m = p.matcher(version);
            m.matches();

            major = Integer.parseInt(m.group(1));
            minor = Integer.parseInt(m.group(2));
            build = Integer.parseInt(m.group(4));

            VersionInfo vi = new VersionInfo(major, minor, 0, 0, "", build);
            System.out.printf("jvmVersionInfo: input=%s output=%s\n", version, vi);
            return vi;
        } catch (IllegalStateException e) {
            // local builds may also follow the jdkVersionInfo format
            return jdkVersionInfo(version);
        }
    }
}
