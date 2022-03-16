/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

package java.net;

import java.io.IOException;

class SocketSecrets {

    /* accessed by reflection from jdk.net.Sockets */

    /* obj must be a Socket or ServerSocket */

    private static <T> void setOption(Object obj, SocketOption<T> name, T value) throws IOException {
        SocketImpl impl;

        if (obj instanceof Socket) {
            impl = ((Socket)obj).getImpl();
        } else if (obj instanceof ServerSocket) {
            impl = ((ServerSocket)obj).getImpl();
        } else {
            throw new IllegalArgumentException();
        }
        impl.setOption(name, value);
    }

    private static <T> T getOption(Object obj, SocketOption<T> name) throws IOException {
        SocketImpl impl;

        if (obj instanceof Socket) {
            impl = ((Socket)obj).getImpl();
        } else if (obj instanceof ServerSocket) {
            impl = ((ServerSocket)obj).getImpl();
        } else {
            throw new IllegalArgumentException();
        }
        return impl.getOption(name);
    }

    private static <T> void setOption(DatagramSocket s, SocketOption<T> name, T value) throws IOException {
        s.getImpl().setOption(name, value);
    }

    private static <T> T getOption(DatagramSocket s, SocketOption<T> name) throws IOException {
        return s.getImpl().getOption(name);
    }

}
