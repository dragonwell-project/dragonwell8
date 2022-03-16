/*
 * Copyright (c) 2021, Huawei Technologies Co., Ltd. All rights reserved.
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
import java.io.InputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

import org.testng.annotations.Test;
import org.testng.Assert;

/*
 * @test
 * @bug 8260875
 * @summary Reading a closed SocketChannel should throw a ClosedChannelException
 * @run testng AdaptSocketReadAfterClose
 */

public class AdaptSocketReadAfterClose {

    @Test
    public void testReadAfterClose() throws InterruptedException, IOException {
        ServerSocketChannel ssc = ServerSocketChannel.open();
        ssc.bind(new InetSocketAddress(InetAddress.getLocalHost(), 0));
        SocketAddress saddr = ssc.getLocalAddress();

        SocketChannel channel = SocketChannel.open(saddr);
        channel.configureBlocking(false);
        Socket s = channel.socket();
        InputStream is = s.getInputStream();
        channel.close();

        byte[] buf = new byte[10];
        Throwable ex = Assert.expectThrows(ClosedChannelException.class, () -> is.read(buf));
        Assert.assertEquals(ex.getClass(), ClosedChannelException.class);
        ssc.close();
    }
}
