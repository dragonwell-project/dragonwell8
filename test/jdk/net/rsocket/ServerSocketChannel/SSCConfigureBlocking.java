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
 * @summary Tests blocking configuration of server socket channels
 * @requires (os.family == "linux")
 * @library ../ /test/lib
 * @build RsocketTest
 * @run testng/othervm SSCConfigureBlocking
 */

import java.io.IOException;
import java.net.ProtocolFamily;
import java.nio.channels.IllegalBlockingModeException;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import org.testng.SkipException;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;
import static java.net.StandardProtocolFamily.INET;
import static java.net.StandardProtocolFamily.INET6;
import static java.nio.channels.SelectionKey.OP_ACCEPT;
import static jdk.net.RdmaSockets.*;
import static org.testng.Assert.assertThrows;
import static org.testng.Assert.assertTrue;

public class SSCConfigureBlocking {

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
    public void testServerSocketChannel(ProtocolFamily family)
        throws IOException
    {
        try (ServerSocketChannel ssc = openServerSocketChannel(family)) {
            assertTrue(ssc.isBlocking(), "Newly created channel is not blocking");
            ssc.configureBlocking(false);
            assertTrue(!ssc.isBlocking(), "Expected non-blocking");
            ssc.configureBlocking(true);
            assertTrue(ssc.isBlocking(), "Expected blocking");
        }
    }

    static final Class<IllegalBlockingModeException> IBME = IllegalBlockingModeException.class;

    @Test(dataProvider = "families")
    public void testServerSocketChannelRegister(ProtocolFamily family)
        throws IOException
    {
        try (ServerSocketChannel ssc = openServerSocketChannel(family);
             Selector selector = Selector.open()) {
            assertThrows(IBME, () -> ssc.register(selector, OP_ACCEPT));
            assertThrows(IBME, () -> ssc.register(selector, OP_ACCEPT, new Object()));
            ssc.configureBlocking(false);
            ssc.register(selector, OP_ACCEPT);
            assertThrows(IBME, () -> ssc.configureBlocking(true));
        }
    }
}
