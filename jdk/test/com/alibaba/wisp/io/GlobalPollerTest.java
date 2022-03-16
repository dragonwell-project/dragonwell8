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
 * @summary Test for Global Poller
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.transparentAsync=true GlobalPollerTest
*/

import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;
import sun.nio.ch.SelChImpl;

import java.lang.reflect.Field;
import java.net.Socket;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.util.Properties;

import static jdk.testlibrary.Asserts.assertNotNull;
import static jdk.testlibrary.Asserts.assertTrue;

public class GlobalPollerTest {
    private static WispEngineAccess access = SharedSecrets.getWispEngineAccess();

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


        Socket so = new Socket(socketAddr, 80);
        so.getOutputStream().write("NOP\n\r\n\r".getBytes());
        // now server returns the data..
        // so is readable
        // current task is interested in read event.
        SocketChannel ch = so.getChannel();
        access.registerEvent(ch, SelectionKey.OP_READ);

        Class<?> clazz = Class.forName("com.alibaba.wisp.engine.WispEventPump$Pool");
        Field pumps = clazz.getDeclaredField("pumps");
        pumps.setAccessible(true);
        Object[] a = (Object[]) pumps.get(clazz.getEnumConstants()[0]);
        WispTask[] fd2TaskLow = null;
        int fd = ((SelChImpl) ch).getFDVal();
        for (Object pump : a) {
            Field f = Class.forName("com.alibaba.wisp.engine.WispEventPump").getDeclaredField("fd2ReadTaskLow");
            f.setAccessible(true);
            WispTask[] map = (WispTask[]) f.get(pump);
            if (map[fd] != null) {
                fd2TaskLow = map;
            }
        }
        assertNotNull(fd2TaskLow);

        access.park(-1);

        assertTrue(fd2TaskLow[fd] == null);

        so.close();
    }
}
