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

/* @test
 * @bug 8195160
 * @summary Unit test for SocketOption
 * @requires (os.family == "linux")
 * @requires !vm.graal.enabled
 * @library .. /test/lib
 * @build RsocketTest
 * @run main/othervm OptionsTest
 */

import java.net.StandardProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketOption;
import java.net.StandardSocketOptions;
import java.util.Formatter;
import java.util.Set;
import jdk.net.RdmaSockets;
import jdk.net.RdmaSocketOptions;

import jtreg.SkippedException;

public class OptionsTest {

    static class Test {
        Test(SocketOption<?> option, Object testValue) {
            this.option = option;
            this.testValue = testValue;
        }
        static Test create (SocketOption<?> option, Object testValue) {
            return new Test(option, testValue);
        }
        Object option;
        Object testValue;
    }

    // The tests set the option using the new API, read back the set value
    // which could be diferent, and then use the legacy get API to check
    // these values are the same

    static Test[] socketTests = new Test[] {
        Test.create(StandardSocketOptions.SO_SNDBUF, Integer.valueOf(10 * 100)),
        Test.create(StandardSocketOptions.SO_RCVBUF, Integer.valueOf(8 * 100)),
        Test.create(StandardSocketOptions.SO_REUSEADDR, Boolean.FALSE),
    };

    static Test[] serverSocketTests = new Test[] {
        Test.create(StandardSocketOptions.SO_RCVBUF, Integer.valueOf(8 * 100)),
        Test.create(StandardSocketOptions.SO_REUSEADDR, Boolean.FALSE),
    };

    static Test[] rdmaSocketOptionTests = new Test[] {
        Test.create(RdmaSocketOptions.RDMA_SQSIZE, Integer.valueOf(8 * 100)),
        Test.create(RdmaSocketOptions.RDMA_RQSIZE, Integer.valueOf(10 * 100)),
        Test.create(RdmaSocketOptions.RDMA_INLINE, Integer.valueOf(20 * 100)),
    };

    static void doSocketTests() throws Exception {
        try {
            Socket c = RdmaSockets.openSocket(StandardProtocolFamily.INET);

            for (int i = 0; i < socketTests.length; i++) {
                Test test = socketTests[i];
                c.setOption((SocketOption)test.option, test.testValue);
                Object getval = c.getOption((SocketOption)test.option);
                Object legacyget = legacyGetOption(Socket.class, c, test.option);
                if (!getval.equals(legacyget)) {
                    Formatter f = new Formatter();
                    f.format("S Err %d: %s/%s", i, getval, legacyget);
                    throw new RuntimeException(f.toString());
                }
            }

            for (int i = 0; i < rdmaSocketOptionTests.length; i++) {
                Test test = rdmaSocketOptionTests[i];
                c.setOption((SocketOption)test.option, test.testValue);
                Object getval = c.getOption((SocketOption)test.option);
                if (((Integer)getval).intValue() !=
                        ((Integer)test.testValue).intValue())
                    throw new RuntimeException("Test Failed");
            }
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed");
        }
    }

    static void doServerSocketTests() throws Exception {
        try {
            ServerSocket srv = RdmaSockets.openServerSocket(
                StandardProtocolFamily.INET);
            for (int i = 0; i < serverSocketTests.length; i++) {
                Test test = serverSocketTests[i];
                srv.setOption((SocketOption)test.option, test.testValue);
                Object getval = srv.getOption((SocketOption)test.option);
                Object legacyget = legacyGetOption(
                    ServerSocket.class, srv, test.option
                );
                if (!getval.equals(legacyget)) {
                    Formatter f = new Formatter();
                    f.format("SS Err %d: %s/%s", i, getval, legacyget);
                    throw new RuntimeException(f.toString());
                }
            }

            for (int i = 0; i < rdmaSocketOptionTests.length; i++) {
                Test test = rdmaSocketOptionTests[i];
                srv.setOption((SocketOption)test.option, test.testValue);
                Object getval = srv.getOption((SocketOption)test.option);
                if (((Integer)getval).intValue() !=
                        ((Integer)test.testValue).intValue())
                    throw new RuntimeException("Test Failed");
            }
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException("Test Failed");
        }
    }

    static Object legacyGetOption(
        Class<?> type, Object s, Object option)

        throws Exception
    {
        if (type.equals(Socket.class)) {
            Socket socket = (Socket)s;
            Set<SocketOption<?>> options = socket.supportedOptions();

            if (option.equals(StandardSocketOptions.SO_SNDBUF)) {
                return Integer.valueOf(socket.getSendBufferSize());
            } else if (option.equals(StandardSocketOptions.SO_RCVBUF)) {
                return Integer.valueOf(socket.getReceiveBufferSize());
            } else if (option.equals(StandardSocketOptions.SO_REUSEADDR)) {
                return Boolean.valueOf(socket.getReuseAddress());
            } else if (option.equals(StandardSocketOptions.TCP_NODELAY)) {
                return Boolean.valueOf(socket.getTcpNoDelay());
            } else {
                throw new RuntimeException("unexecpted socket option");
            }
        } else if  (type.equals(ServerSocket.class)) {
            ServerSocket socket = (ServerSocket)s;
            Set<SocketOption<?>> options = socket.supportedOptions();

            if (option.equals(StandardSocketOptions.SO_RCVBUF)) {
                return Integer.valueOf(socket.getReceiveBufferSize());
            } else if (option.equals(StandardSocketOptions.SO_REUSEADDR)) {
                return Boolean.valueOf(socket.getReuseAddress());
            } else {
                throw new RuntimeException("unexecpted socket option");
            }
        }
        throw new RuntimeException("unexecpted socket type");
    }

    public static void main(String args[]) throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkippedException("rsocket is not available");

        doSocketTests();
        doServerSocketTests();
    }
}
