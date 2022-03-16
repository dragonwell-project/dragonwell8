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
 * @summary Tests blocking configuration of socket channels
 * @requires (os.family == "linux")
 * @library ../ /test/lib
 * @build RsocketTest
 * @run testng/othervm SCConfigureBlocking
 */

import java.io.IOException;
import java.net.ProtocolFamily;
import java.nio.channels.IllegalBlockingModeException;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import org.testng.SkipException;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;
import static java.nio.channels.SelectionKey.OP_READ;
import static java.nio.channels.SelectionKey.OP_WRITE;
import static jdk.net.RdmaSockets.*;
import static org.testng.Assert.assertThrows;
import static org.testng.Assert.assertTrue;

public class SCConfigureBlocking {

    @BeforeTest
    public void setup() throws Exception {
        if (!RsocketTest.isRsocketAvailable())
            throw new SkipException("rsocket is not available");
    }

    @DataProvider(name = "families")
    public Object[][] families() {
        return new Object[][] { { INET }, { INET6 } };
    }

     // Newly-created selectable channels are always in blocking mode.

    @Test(dataProvider = "families")
    public void testSocketChannel(ProtocolFamily family)
        throws IOException
    {
        try (SocketChannel sc = openSocketChannel(family)) {
            assertTrue(sc.isBlocking(), "Newly created channel is not blocking");
            sc.configureBlocking(false);
            assertTrue(!sc.isBlocking(), "Expected non-blocking");
            sc.configureBlocking(true);
            assertTrue(sc.isBlocking(), "Expected blocking");
        }
    }

    static final Class<IllegalBlockingModeException> IBME = IllegalBlockingModeException.class;

    @Test(dataProvider = "families")
    public void testSocketChannelRegister(ProtocolFamily family)
        throws IOException
    {
        try (SocketChannel sc = openSocketChannel(family);
             Selector selector = Selector.open()) {
            assertThrows(IBME, () -> sc.register(selector, OP_READ));
            assertThrows(IBME, () -> sc.register(selector, OP_READ, new Object()));
            sc.configureBlocking(false);
            sc.register(selector, OP_READ | OP_WRITE);
            assertThrows(IBME, () -> sc.configureBlocking(true));
        }
    }
}
