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

package sun.nio.ch.rdma;

import sun.nio.ch.IOUtil;

import java.io.FileDescriptor;
import java.io.IOException;

public class LinuxRdmaSocketDispatcherImpl {

    static {
        java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Void>() {
                    public Void run() {
                        System.loadLibrary("extnet");
                        return null;
                    }
                });
        IOUtil.load();
        init();
    }

    LinuxRdmaSocketDispatcherImpl() { }


    // -- Native methods --

    public static native int read0(FileDescriptor fd, long address, int len)
            throws IOException;

    public static native long readv0(FileDescriptor fd, long address, int len)
            throws IOException;

    public static native int write0(FileDescriptor fd, long address, int len)
            throws IOException;

    public static native long writev0(FileDescriptor fd, long address, int len)
            throws IOException;

    public static native void close0(FileDescriptor fd) throws IOException;

    static native void init();
}
