/*
 * Copyright (c) 2002, 2014, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.java.accessibility;

@jdk.Exported(false)
abstract class AccessBridgeLoader {

    /**
     * Load JavaAccessBridge.DLL (our native half)
     */
    static {
        java.security.AccessController.doPrivileged(
            new java.security.PrivilegedAction<Object>() {
                public Object run() {
                    System.loadLibrary("JavaAccessBridge-32");
                    return null;
                }
            }, null, new java.lang.RuntimePermission("loadLibrary.JavaAccessBridge-32")
        );
    }

    boolean useJAWT_DLL = false;

    /**
     * AccessBridgeLoader constructor
     */
    AccessBridgeLoader() {
        // load JAWTAccessBridge.DLL on JDK 1.4.1 or later
        // determine which version of the JDK is running
        String version = System.getProperty("java.version");
        if (version != null)
            useJAWT_DLL = (version.compareTo("1.4.1") >= 0);

        if (useJAWT_DLL) {
            // Note that we have to explicitly load JAWT.DLL
            java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Object>() {
                    public Object run() {
                        System.loadLibrary("JAWT");
                        System.loadLibrary("JAWTAccessBridge-32");
                        return null;
                    }
                }, null, new RuntimePermission("loadLibrary.JAWT"),
                         new RuntimePermission("loadLibrary.JAWTAccessBridge-32")
            );
        }
    }
}
