/*
 * Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

/* @test
 * @summary unit tests for coroutines
 * @run junit/othervm -XX:+EnableCoroutine test.java.dyn.CoroutineTest
 */

package test.java.dyn;

import java.dyn.Coroutine;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import static org.junit.Assert.*;

import org.junit.Before;
import org.junit.Test;

public class CoroutineTest {
	private StringBuilder seq;

	@Before
	public void before() {
		seq = new StringBuilder();
		seq.append("a");
	}

	@Test
	public void symSequence() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		Coroutine coro = new Coroutine() {
			protected void run() {
				seq.append("c");
				for (int i = 0; i < 3; i++) {
					Coroutine.yieldTo(threadCoro);
					seq.append("e");
				}
			}
		};
		seq.append("b");
		assertFalse(coro.isFinished());
		Coroutine.yieldTo(coro);
		for (int i = 0; i < 3; i++) {
			seq.append("d");
			assertFalse(coro.isFinished());
			Coroutine.yieldTo(coro);
		}
		seq.append("f");
		assertTrue(coro.isFinished());
		seq.append("g");
		assertEquals("abcdededefg", seq.toString());
	}


	@Test
	public void gcTest1() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		Coroutine coro =  new Coroutine() {
			protected void run() {
				seq.append("c");
				Integer v1 = 1;
				Integer v2 = 14555668;
				yieldTo(threadCoro);
				seq.append("e");
				seq.append("(" + v1 + "," + v2 + ")");
			}
		};
		seq.append("b");
		System.gc();
		Coroutine.yieldTo(coro);
		System.gc();
		seq.append("d");
		Coroutine.yieldTo(coro);
		seq.append("fg");
		assertEquals("abcde(1,14555668)fg", seq.toString());
	}

	@Test
	public void exceptionTest1() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		Coroutine coro = new Coroutine() {
			protected void run() {
				seq.append("c");
				long temp = System.nanoTime();
				if (temp != 0)
					throw new RuntimeException();
				seq.append("e");
			}
		};
		seq.append("b");
		assertFalse(coro.isFinished());
		Coroutine.yieldTo(coro);
		seq.append("d");
		seq.append("f");
		assertEquals("abcdf", seq.toString());
	}

	@Test
	public void largeStackframeTest() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		Coroutine coro =  new Coroutine() {
			protected void run() {
				seq.append("c");
				Integer v0 = 10000;
				Integer v1 = 10001;
				Integer v2 = 10002;
				Integer v3 = 10003;
				Integer v4 = 10004;
				Integer v5 = 10005;
				Integer v6 = 10006;
				Integer v7 = 10007;
				Integer v8 = 10008;
				Integer v9 = 10009;
				Integer v10 = 10010;
				Integer v11 = 10011;
				Integer v12 = 10012;
				Integer v13 = 10013;
				Integer v14 = 10014;
				Integer v15 = 10015;
				Integer v16 = 10016;
				Integer v17 = 10017;
				Integer v18 = 10018;
				Integer v19 = 10019;
				yieldTo(threadCoro);
				int sum = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19;
				seq.append("e" + sum);
			}
		};
		seq.append("b");
		System.gc();
		Coroutine.yieldTo(coro);
		System.gc();
		seq.append("d");
		Coroutine.yieldTo(coro);
		seq.append("f");
		assertEquals("abcde200190f", seq.toString());
	}

	@Test
	public void shaTest() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		Coroutine coro = new Coroutine(65536) {
			protected void run() {
				try {
					MessageDigest digest = MessageDigest.getInstance("SHA");
					digest.update("TestMessage".getBytes());
					seq.append("b");
					yieldTo(threadCoro);
					seq.append(digest.digest()[0]);
				} catch (NoSuchAlgorithmException e) {
					e.printStackTrace();
				}
			}
		};
		Coroutine.yieldTo(coro);
		seq.append("c");
		assertFalse(coro.isFinished());
		Coroutine.yieldTo(coro);
		assertTrue(coro.isFinished());
		assertEquals("abc72", seq.toString());
	}

	public void stackoverflowTest() {
		Coroutine threadCoro = Thread.currentThread().getCoroutineSupport().threadCoroutine();
		new Coroutine(255) {
			int i = 0;

			protected void run() {
				System.out.println("start");
				try {
					iter();
				} catch (StackOverflowError e) {
					System.out.println("i: " + i);
				}
				System.out.println("asdf");
			}

			private void iter() {
				System.out.print(".");
				i++;
				iter();
			}
		};
		Coroutine.yieldTo(threadCoro);
	}

  @Test
	public void destroyNonInitedTest() {
    new Coroutine();
  }
}
