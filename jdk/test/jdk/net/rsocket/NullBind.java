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

/*
 * @test
 * @bug 8195160
 * @summary Tests binding with null ( automatically assigned address )
 * @requires (os.family == "linux")
 * @library /test/lib
 * @build RsocketTest
 * @run testng/othervm NullBind
 * @run testng/othervm -Djava.net.preferIPv6Addresses=true NullBind
 */

import java.io.IOException;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ProtocolFamily;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import org.testng.SkipException;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;
import static java.lang.String.format;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;
import static jdk.net.RdmaSockets.*;
import static org.testng.Assert.assertTrue;

public class NullBind {

    @BeforeTest
    public void setup() throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkipException("rsocket is not available");
    }

    @DataProvider(name = "families")
    public Object[][] families() {
        return new Object[][] {
            { INET,  Inet4Address.class },
            { INET6, Inet6Address.class }
        };
    }

    @Test(dataProvider = "families")
    public void testSocket(ProtocolFamily family,
                           Class<? extends InetAddress> expectedClass)
        throws IOException
    {
        try (Socket s = openSocket(family)) {
            s.bind(null);
            InetAddress addr = s.getLocalAddress();
            //TODO: Argh! still a family issue in Socket.getLocalAddress
            //assertTrue(expectedClass.isAssignableFrom(addr.getClass()),
            //           format("Excepted %s got: %s", expectedClass, addr));
            assertTrue(addr.isAnyLocalAddress(),
                       format("Expected any local address, got: %s", addr));
        }
    }

    @Test(dataProvider = "families")
    public void testServerSocket(ProtocolFamily family,
                                 Class<? extends InetAddress> expectedClass)
        throws IOException
    {
        try (ServerSocket ss = openServerSocket(family)) {
            ss.bind(null);
            InetAddress addr = ss.getInetAddress();
            assertTrue(expectedClass.isAssignableFrom(addr.getClass()),
                       format("Excepted %s got: %s", expectedClass, addr));
            assertTrue(addr.isAnyLocalAddress(),
                       format("Expected any local address, got: %s", addr));
        }
    }

    @Test(dataProvider = "families")
    public void testSocketChannel(ProtocolFamily family,
                                  Class<? extends InetAddress> expectedClass)
        throws IOException
    {
        try (SocketChannel sc = openSocketChannel(family)) {
            sc.bind(null);
            InetAddress addr = ((InetSocketAddress)sc.getLocalAddress()).getAddress();
            assertTrue(expectedClass.isAssignableFrom(addr.getClass()),
                      format("Excepted %s got: %s", expectedClass, addr));
            assertTrue(addr.isAnyLocalAddress(),
                       format("Expected any local address, got: %s", addr));
        }
    }

    @Test(dataProvider = "families")
    public void testServerSocketChannel(ProtocolFamily family,
                                        Class<? extends InetAddress> expectedClass)
        throws IOException
    {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            ssc.bind(null);
            InetAddress addr = ((InetSocketAddress)ssc.getLocalAddress()).getAddress();
            assertTrue(expectedClass.isAssignableFrom(addr.getClass()),
                       format("Excepted %s got: %s", expectedClass, addr));
            assertTrue(addr.isAnyLocalAddress(),
                       format("Expected any local address, got: %s", addr));
        }
    }
}
