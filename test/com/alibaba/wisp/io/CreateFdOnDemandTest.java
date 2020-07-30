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
 * @summary Test fix of unconnected Socket fd leak.
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true CreateFdOnDemandTest
 */

import java.io.File;
import java.net.DatagramSocket;
import java.net.ServerSocket;
import java.net.Socket;

import com.alibaba.wisp.engine.WispEngine;

import static jdk.testlibrary.Asserts.assertEQ;
import static jdk.testlibrary.Asserts.assertTrue;

public class CreateFdOnDemandTest {
    public static void main(String[] args) throws Exception {

        assertEQ(countFd(), countFd());
        Socket so = new Socket();
        so.setReuseAddress(true);
        so.close();
        ServerSocket sso = new ServerSocket();
        sso.setReuseAddress(true);
        sso.close();
        DatagramSocket ds = new DatagramSocket(null);
        ds.setReuseAddress(true);
        ds.close();


        final int nfd0 = countFd();
        so = new Socket();
        assertEQ(countFd(), nfd0);
        sso = new ServerSocket();
        assertEQ(countFd(), nfd0);
        ds = new DatagramSocket(null);
        assertTrue(WispEngine.transparentWispSwitch() && countFd() == nfd0); // if -Dcom.alibaba.wisp.transparentWispSwitch=false, fail

        so.setReuseAddress(true);
        assertEQ(countFd(), nfd0 + 1);
        sso.setReuseAddress(true);
        assertEQ(countFd(), nfd0 + 2);
        ds.setReuseAddress(true);
        assertEQ(countFd(), nfd0 + 3);

        so.close();
        assertEQ(countFd(), nfd0 + 2);
        sso.close();
        assertEQ(countFd(), nfd0 + 1);
        ds.close();
        assertEQ(countFd(), nfd0);
    }

    private static int countFd() {
        File f = new File("/proc/self/fd");
        return f.list().length;
    }
}
