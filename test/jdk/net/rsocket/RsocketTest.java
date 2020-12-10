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

/*
 * @test
 * @bug 8195160
 * @summary Test to check if rsocket is available
 * @requires (os.family == "linux")
 * @library /test/lib
 * @build jdk.test.lib.Platform
 * @run main/native RsocketTest
 */

import java.io.IOException;
import java.net.SocketException;
import java.net.StandardProtocolFamily;

public class RsocketTest {
    private static boolean libInstalled;
    private static boolean supported;
    private static boolean checked;

    static {
        System.loadLibrary("RsocketTest");
    }

    public static boolean isRsocketAvailable()
            throws IOException {
        if (!checked) {
            checked = true;
            supported = isRsocketAvailable0();
        }
        if (!supported) {
            checkUnsupportedWithParameter();
            checkUnsupportedNoParameter();
        }
        return supported;
    }

    static void checkUnsupportedWithParameter() throws IOException {
        checkThrowsUOEWithParameter(jdk.net.RdmaSockets::openSocket);
        checkThrowsUOEWithParameter(jdk.net.RdmaSockets::openServerSocket);
        checkThrowsUOEWithParameter(jdk.net.RdmaSockets::openSocketChannel);
        checkThrowsUOEWithParameter(
                jdk.net.RdmaSockets::openServerSocketChannel);
    }

    static void checkUnsupportedNoParameter() throws IOException {
        checkThrowsUOENoParameter(jdk.net.RdmaSockets::openSelector);
    }

    static void checkThrowsUOEWithParameter(
            ThrowingRunnableWithParameter runnable) throws IOException {
        try {
            runnable.run(StandardProtocolFamily.INET);
            runnable.run(StandardProtocolFamily.INET6);
            if (!libInstalled)
                throw new RuntimeException("Unexpected creation");
        }
        catch (UnsupportedOperationException | SocketException expected) { }
    }

    static void checkThrowsUOENoParameter(ThrowingRunnableNoParameter runnable)
            throws IOException {
        try {
            runnable.run();
            if (!libInstalled)
                throw new RuntimeException("Unexpected creation");
        }
        catch (UnsupportedOperationException expected) { }
    }

    interface ThrowingRunnableWithParameter {
        void run(StandardProtocolFamily family) throws IOException;
    }

    interface ThrowingRunnableNoParameter { void run() throws IOException; }

    private static native boolean isRsocketAvailable0();

    public static void main(String[] args) throws Exception {
         System.out.println("testing rsocket!");
    }
}
