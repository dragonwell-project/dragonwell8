/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @summary ensure nio program call SelectionKey.is{}able() and got correct result.
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true WispSelectorReadyOpsTest
 */

import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

public class WispSelectorReadyOpsTest {
    public static void main(String[] args) throws Exception {
        Selector selector = Selector.open();
        ServerSocketChannel sch = ServerSocketChannel.open();
        sch.configureBlocking(false);
        sch.bind(new InetSocketAddress(54442));
        sch.register(selector, sch.validOps());

        Socket so = new Socket("127.0.0.1", 54442);

        selector.selectNow();
        SelectionKey sk = (SelectionKey) selector.selectedKeys().toArray()[0];
        if (!sk.isAcceptable()) {
            throw new Error("fail");
        }

        SocketChannel sc = sch.accept();
        sc.configureBlocking(false);
        sc.register(selector, sc.validOps());

        so.getOutputStream().write("123".getBytes());

        selector.selectedKeys().clear();
        selector.selectNow();
        sk = (SelectionKey) selector.selectedKeys().toArray()[0];
        if (!sk.isReadable()) {
            throw new Error("fail");
        }
    }

}
