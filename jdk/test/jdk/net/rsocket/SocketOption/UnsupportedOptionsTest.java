/*
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

import java.io.IOException;
import java.net.StandardProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketOption;
import java.net.StandardSocketOptions;
import java.util.ArrayList;
import java.util.List;
import jdk.net.RdmaSockets;
import jdk.net.RdmaSocketOptions;

import jtreg.SkippedException;

/* @test
 * @bug 8195160
 * @summary Test checks that UnsupportedOperationException for unsupported
 *          SOCKET_OPTIONS is thrown by both getOption() and setOption() methods.
 * @requires (os.family == "linux")
 * @requires !vm.graal.enabled
 * @library .. /test/lib
 * @build RsocketTest
 * @run main/othervm UnsupportedOptionsTest
 */
public class UnsupportedOptionsTest {

    private static final List<SocketOption<?>> socketOptions = new ArrayList<>();

    static {
        socketOptions.add(StandardSocketOptions.SO_RCVBUF);
        socketOptions.add(StandardSocketOptions.SO_REUSEADDR);
        socketOptions.add(StandardSocketOptions.SO_SNDBUF);
        socketOptions.add(StandardSocketOptions.TCP_NODELAY);
        socketOptions.add(RdmaSocketOptions.RDMA_SQSIZE);
        socketOptions.add(RdmaSocketOptions.RDMA_RQSIZE);
        socketOptions.add(RdmaSocketOptions.RDMA_INLINE);
    }

    public static void main(String[] args) throws IOException {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        Socket s = RdmaSockets.openSocket(StandardProtocolFamily.INET);
        ServerSocket ss = RdmaSockets.openServerSocket(
            StandardProtocolFamily.INET);

        for (SocketOption option : socketOptions) {
            if (!s.supportedOptions().contains(option)) {
                testUnsupportedSocketOption(s, option);
            }

            if (!ss.supportedOptions().contains(option)) {
                testUnsupportedSocketOption(ss, option);
            }
        }
    }

    /*
     * Check that UnsupportedOperationException for unsupported option is
     * thrown from both getOption() and setOption() methods.
     */
    private static void testUnsupportedSocketOption(Object socket,
                                                    SocketOption option) {
        testSet(socket, option);
        testGet(socket, option);
    }

    private static void testSet(Object socket, SocketOption option) {
        try {
            setOption(socket, option);
        } catch (UnsupportedOperationException e) {
            System.out.println("UnsupportedOperationException was throw " +
                    "as expected. Socket: " + socket + " Option: " + option);
            return;
        } catch (Exception e) {
            throw new RuntimeException("FAIL. Unexpected exception.", e);
        }
        throw new RuntimeException("FAIL. UnsupportedOperationException " +
                "hasn't been thrown. Socket: " + socket + " Option: " + option);
    }

    private static void testGet(Object socket, SocketOption option) {
        try {
            getOption(socket, option);
        } catch (UnsupportedOperationException e) {
            System.out.println("UnsupportedOperationException was throw " +
                    "as expected. Socket: " + socket + " Option: " + option);
            return;
        } catch (Exception e) {
            throw new RuntimeException("FAIL. Unexpected exception.", e);
        }
        throw new RuntimeException("FAIL. UnsupportedOperationException " +
                "hasn't been thrown. Socket: " + socket + " Option: " + option);
    }

    private static void getOption(Object socket,
                                  SocketOption option) throws IOException {
        if (socket instanceof Socket) {
            ((Socket) socket).getOption(option);
        } else if (socket instanceof ServerSocket) {
            ((ServerSocket) socket).getOption(option);
        } else {
            throw new RuntimeException("Unsupported socket type");
        }
    }

    private static void setOption(Object socket,
                                  SocketOption option) throws IOException {
        if (socket instanceof Socket) {
            ((Socket) socket).setOption(option, null);
        } else if (socket instanceof ServerSocket) {
            ((ServerSocket) socket).setOption(option, null);
        } else {
            throw new RuntimeException("Unsupported socket type");
        }
    }
}
