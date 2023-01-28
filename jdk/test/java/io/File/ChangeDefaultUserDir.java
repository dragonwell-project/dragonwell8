/*
 * Copyright Amazon.com Inc. or its affiliates. All Rights Reserved.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

import java.io.File;

/**
 * @test
 * @bug 8290832
 * @summary It should be possible to change the "user.dir" property
 * @run main/othervm ChangeDefaultUserDir
 */
public final class ChangeDefaultUserDir {
    public static void main(String[] args) throws Exception {
        String keyUserDir = "user.dir";
        String userDirNew = "/home/user/";
        String fileName = "file";

        String userDir = System.getProperty(keyUserDir);
        System.out.format("%24s %48s%n", "Default 'user.dir' = ", userDir);
        File file = new File(fileName);
        String canFilePath = file.getCanonicalPath();

        System.setProperty(keyUserDir, userDirNew);
        String newCanFilePath = file.getCanonicalPath();
        System.out.format("%24s %48s%n", "Canonical Path = ", canFilePath);
        System.out.format("%24s %48s%n", "new Canonical Path = ", newCanFilePath);
        if (canFilePath.equals(newCanFilePath)) {
            throw new RuntimeException("Cannot change user.dir property");
        }
    }
}
