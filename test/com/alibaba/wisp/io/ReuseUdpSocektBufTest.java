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
 * @summary test reuse WispUdpSocket buffer
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true ReuseUdpSocektBufTest
 */

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

import static jdk.testlibrary.Asserts.assertTrue;


public class ReuseUdpSocektBufTest {

	static String msgs[] = {"Hello World", "Java", "Good Bye"};
	static int port;

	static boolean success = true;

	static class ServerThread extends Thread{
		DatagramSocket ds;
		public ServerThread() {
			try {
				ds = new DatagramSocket();
				port = ds.getLocalPort();
			} catch (Exception e) {
				throw new RuntimeException(e.getMessage());
			}
		}

		public void run() {
			byte b[] = new byte[100];
			DatagramPacket dp = new DatagramPacket(b,b.length);
			while (true) {
				try {
					ds.receive(dp);
					String reply = new String(dp.getData(), dp.getOffset(), dp.getLength());
					ds.send(new DatagramPacket(reply.getBytes(),reply.length(),
							dp.getAddress(),dp.getPort()));
					if (reply.equals(msgs[msgs.length-1])) {
						break;
					}
				} catch (Exception e) {
					success = false;
				}
			}
			ds.close();
		}
	}

	public static void main(String args[]) throws Exception {
		ServerThread st = new ServerThread();
		st.start();
		DatagramSocket ds = new DatagramSocket();
		byte b[] = new byte[100];
		DatagramPacket dp = new DatagramPacket(b,b.length);
		for (int i = 0; i < msgs.length; i++) {
			ds.send(new DatagramPacket(msgs[i].getBytes(),msgs[i].length(),
					InetAddress.getLocalHost(),
					port));
			ds.receive(dp);
			if (!msgs[i].equals(new String(dp.getData(), dp.getOffset(), dp.getLength()))) {
				success = false;
			}
		}
		ds.close();
		assertTrue(success);
		System.out.println("Test Passed!!!");
	}
}
