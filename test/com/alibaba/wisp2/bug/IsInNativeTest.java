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
 * @library /lib/testlibrary
 * @summary test Thread.isInNative() is correct
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine IsInNativeTest
 */

import sun.misc.SharedSecrets;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.util.Properties;

import static jdk.testlibrary.Asserts.assertFalse;
import static jdk.testlibrary.Asserts.assertTrue;

public class IsInNativeTest {

    static Properties p;
    static String socketAddr;
    static {
        p = java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Properties>() {
                    public Properties run() {
                        return System.getProperties();
                    }
                }
        );
        socketAddr = (String)p.get("test.wisp.socketAddress");
        if (socketAddr == null) {
            socketAddr = "www.example.com";
        }
    }

    public static void main(String[] args) throws Exception {
        Thread nthread = new Thread(() -> {
            try {
                SocketChannel ch = SocketChannel.open(new InetSocketAddress(socketAddr, 80));
                ch.read(ByteBuffer.allocate(4096));
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
        nthread.start();
        Thread thread = new Thread(() -> {
            while (true) {

            }
        });
        thread.start();
        Thread thread2 = new Thread(() -> {
        });
        thread2.start();
        Thread.sleep(500);
        assertFalse(SharedSecrets.getJavaLangAccess().isInSameNative(nthread));
        assertFalse(SharedSecrets.getJavaLangAccess().isInSameNative(thread));
        assertFalse(SharedSecrets.getJavaLangAccess().isInSameNative(thread2));
        Thread.sleep(1000);
        assertFalse(SharedSecrets.getJavaLangAccess().isInSameNative(thread));
    }
}
