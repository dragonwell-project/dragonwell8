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
 * @summary WispInitShutdownTest
 * @requires os.family == "linux"
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.wisp.transparentWispSwitch=true -Dcom.alibaba.wisp.version=2 WispInitShutdownTest
 */

import com.alibaba.wisp.engine.WispEngine;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.assertEQ;
import static jdk.testlibrary.Asserts.assertTrue;

public class WispInitShutdownTest {
	public static CountDownLatch shutddownLatch = new CountDownLatch(1);
	public static AtomicBoolean suc = new AtomicBoolean(false);

	public static void main(String[] args) throws Exception {
		CountDownLatch finish = new CountDownLatch(1);
		CountDownLatch cinit = new CountDownLatch(1);
		WispEngine engine = WispEngine.createEngine(1, new ThreadFactory() {
            @Override
            public Thread newThread(Runnable r) {
                Thread t = new Thread(r);
                t.setName("test-thread");
                return t;
            }
        });

		engine.execute(new Runnable() {
			@Override
			public void run() {
				try {
				    cinit.countDown();
					Class.forName("SomeC");
					suc.set(true);
				} catch (Exception e) {
					e.printStackTrace();
					assertTrue(false);
				} finally {
					finish.countDown();
				}
			}
		});

		shutddownLatch.countDown();
		cinit.await();
		engine.shutdown();
		finish.await();
		assertTrue(suc.get());
	}
}

class SomeC {
	static {
		try {
			WispInitShutdownTest.shutddownLatch.await();
			for (int i = 0; i < 10; i++) {
				Thread.sleep(40);
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}