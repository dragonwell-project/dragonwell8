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
 * @summary Test WispEngine's Socket
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true SocketTest
 */

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.*;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import static jdk.testlibrary.Asserts.assertTrue;


public class SocketTest {

    public static void main(String[] args) throws IOException {
        System.out.println(mkSocketPair()); // test accept() and connect()
        testData();
        testBlock();
        testCreation();
    }

    static private void testCreation() throws IOException {
        int expectionCnt = 0;
        try {
            new Socket((Proxy) null);
        } catch (Exception e) {
            expectionCnt++;
        }
        try {
            new Socket(Proxy.NO_PROXY);
        } catch (Exception e) {
            expectionCnt++;
        }
        assertTrue(expectionCnt == 2);
        new SocketWrapper();
        assertTrue(expectionCnt == 2);
    }

    static class SocketWrapper extends Socket {
        SocketWrapper() throws SocketException {
            super((SocketImpl)null);
        }
    }

    static private void testBlock() throws IOException {
        List<Socket> sop = mkSocketPair();
        new Thread(() -> {
            try {
                Socket so = sop.get(0);
                Thread.sleep(100);
                so.getOutputStream().write(new byte[2]);

                so.close();
            } catch (Exception e) {
                throw new Error(e);
            }
        }).start();

        Socket so = sop.get(1);

        long now = System.currentTimeMillis();
        if (2 != so.getInputStream().read(new byte[10])) {
            throw new Error("read error");
        }
        if (Math.abs(System.currentTimeMillis() - now - 100) > 5)
            throw new Error("not wake as expected");
    }

    static private void testData() throws IOException {
        List<Socket> sop = mkSocketPair();

        new Thread(() -> {
            try {
                Socket so = sop.get(0);
                OutputStream os = so.getOutputStream();
                byte buf[] = new byte[4];
                ByteBuffer bb = ByteBuffer.wrap(buf);
                for (int i = 0; i < 10; i++) {
                    bb.clear();
                    bb.putInt(i);
                    os.write(buf);
                }
                so.close();
            } catch (Exception e) {
                throw new Error(e);
            }
        }).start();
        Socket so = sop.get(1);
        InputStream is = so.getInputStream();
        byte buf[] = new byte[4];
        ByteBuffer bb = ByteBuffer.wrap(buf);
        for (int i = 0; true; i++) {
            bb.clear();
            if (4 != is.read(buf)) {
                if (i == 10) {
                    so.close();
                    break; // ok here
                } else {
                    throw new Error("read error");
                }
            }
            if (bb.getInt() != i)
                throw new Error("data error");
        }

    }

    static private List<Socket> mkSocketPair() throws IOException {
        ServerSocket ss = new ServerSocket(13000);
        Socket so = new Socket("localhost", 13000);
        Socket so1 = ss.accept();
        ss.close();

        return Arrays.asList(so, so1);
    }
}
