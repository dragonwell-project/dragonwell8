/*
 * Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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

package java.dyn;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * Implementation of asymmetric coroutines. A AsymCoroutine can be called by any coroutine (Coroutine and AsymCoroutine) and will return to
 * its caller upon {@link #ret()}.
 * <p>
 * Similar to {@link Thread} there are two ways to implement a AsymCoroutine: either by implementing a subclass of AsymCoroutine (and
 * overriding {@link #run()}) or by providing a {@link Runnable} to the AsymCoroutine constructor.
 * <p>
 * An implementation of a simple AsymCoroutine that always returns the average of all its previous inputs might look like this:
 * <p>
 * <hr>
 * <blockquote>
 *
 * <pre>
 * class Average extends AsymCoroutine&lt;Integer, Integer&gt; {
 * 	public Integer run(Integer value) {
 * 		int sum = value;
 * 		int count = 1;
 * 		while (true) {
 * 			sum += ret(sum / count++);
 * 		}
 * 	}
 * }
 * </pre>
 *
 * </blockquote>
 * <hr>
 * <p>
 * This AsymCoroutine can be invoked either by reading and writing the {@link #output} and {@link #input} fields and invoking the
 * {@link #call()} method:
 * <p>
 * <blockquote>
 *
 * <pre>
 * Average avg = new Average();
 * avg.input = 10;
 * avg.call();
 * System.out.println(avg.output);
 * </pre>
 *
 * </blockquote>
 * <p>
 * Another way to invoke this AsymCoroutine is by using the shortcut {@link #call(Object)} methods:
 * <p>
 * <blockquote>
 *
 * <pre>
 * Average avg = new Average();
 * System.out.println(avg.call(10));
 * </pre>
 *
 * </blockquote>
 * <p>
 *
 * @author Lukas Stadler
 *
 * @param <InT>
 *            input type of this AsymCoroutine, Void if no input value is expected
 * @param <OutT>
 *            output type of this AsymCoroutine, Void if no output is produced
 */
public class AsymCoroutine<InT, OutT> extends CoroutineBase implements Iterable<OutT> {
	CoroutineBase caller;

	private final AsymRunnable<? super InT, ? extends OutT> target;

	private InT input;
	private OutT output;

	public AsymCoroutine() {
		target = null;
		threadSupport.addCoroutine(this, -1);
	}

	public AsymCoroutine(long stacksize) {
		target = null;
		threadSupport.addCoroutine(this, stacksize);
	}

	public AsymCoroutine(AsymRunnable<? super InT, ? extends OutT> target) {
		this.target = target;
		threadSupport.addCoroutine(this, -1);
	}

	public AsymCoroutine(AsymRunnable<? super InT, ? extends OutT> target, long stacksize) {
		this.target = target;
		threadSupport.addCoroutine(this, stacksize);
	}

	public final OutT call(final InT input) {
		this.input = input;
		Thread.currentThread().getCoroutineSupport().asymmetricCall(this);
		return output;
	}

	public final OutT call() {
		Thread.currentThread().getCoroutineSupport().asymmetricCall(this);
		return output;
	}

	public final InT ret(final OutT value) {
		output = value;
		Thread.currentThread().getCoroutineSupport().asymmetricReturn(this);
		return input;
	}

	public final InT ret() {
		Thread.currentThread().getCoroutineSupport().asymmetricReturn(this);
		return input;
	}

	protected OutT run(InT value) {
		return target.run(this, value);
	}

	private static class Iter<OutT> implements Iterator<OutT> {
		private final AsymCoroutine<?, OutT> fiber;

		public Iter(final AsymCoroutine<?, OutT> fiber) {
			this.fiber = fiber;
		}

		@Override
		public boolean hasNext() {
			return !fiber.isFinished();
		}

		@Override
		public OutT next() {
			if (fiber.isFinished())
				throw new NoSuchElementException();
			return fiber.call();
		}

		@Override
		public void remove() {
			throw new UnsupportedOperationException();
		}

	}

	@Override
	public final Iterator<OutT> iterator() {
		return new Iter<OutT>(this);
	}

	protected final void run() {
		output = run(input);
	}
}
