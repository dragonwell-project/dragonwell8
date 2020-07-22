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

import sun.reflect.generics.reflectiveObjects.NotImplementedException;

public class CoroutineSupport {
	// Controls debugging and tracing, for maximum performance the actual if(DEBUG/TRACE) code needs to be commented out
	static final boolean DEBUG = false;
	static final boolean TRACE = false;

	static final Object TERMINATED = new Object();

	// The thread that this CoroutineSupport belongs to. There's only one CoroutineSupport per Thread
	private final Thread thread;
	// The initial coroutine of the Thread
	private final Coroutine threadCoroutine;

	// The currently executing, symmetric or asymmetric coroutine
	CoroutineBase currentCoroutine;
	// The anchor of the doubly-linked ring of coroutines
	Coroutine scheduledCoroutines;

	static {
		registerNatives();
	}

	public CoroutineSupport(Thread thread) {
		if (thread.getCoroutineSupport() != null) {
			throw new IllegalArgumentException("Cannot instantiate CoroutineThreadSupport for existing Thread");
		}
		this.thread = thread;
		threadCoroutine = new Coroutine(this, getThreadCoroutine());
		threadCoroutine.next = threadCoroutine;
		threadCoroutine.last = threadCoroutine;
		currentCoroutine = threadCoroutine;
		scheduledCoroutines = threadCoroutine;
	}

  public Coroutine threadCoroutine() {
    return threadCoroutine;
  }

	void addCoroutine(Coroutine coroutine, long stacksize) {
		assert scheduledCoroutines != null;
		assert currentCoroutine != null;

		coroutine.data = createCoroutine(coroutine, stacksize);
		if (DEBUG) {
			System.out.println("add Coroutine " + coroutine + ", data" + coroutine.data);
		}

		// add the coroutine into the doubly linked ring
		coroutine.next = scheduledCoroutines.next;
		coroutine.last = scheduledCoroutines;
		scheduledCoroutines.next = coroutine;
		coroutine.next.last = coroutine;
	}

	void addCoroutine(AsymCoroutine<?, ?> coroutine, long stacksize) {
		coroutine.data = createCoroutine(coroutine, stacksize);
		if (DEBUG) {
			System.out.println("add AsymCoroutine " + coroutine + ", data" + coroutine.data);
		}

		coroutine.caller = null;
	}

	Thread getThread() {
		return thread;
	}

	public void drain() {
		if (Thread.currentThread() != thread) {
			throw new IllegalArgumentException("Cannot drain another threads CoroutineThreadSupport");
		}

		if (DEBUG) {
			System.out.println("draining");
		}
		try {
			// drain all scheduled coroutines
			while (scheduledCoroutines.next != scheduledCoroutines) {
				symmetricExitInternal(scheduledCoroutines.next);
			}

			CoroutineBase coro;
			while ((coro = cleanupCoroutine()) != null) {
				System.out.println(coro);
				throw new NotImplementedException();
			}
		} catch (Throwable t) {
			t.printStackTrace();
		}
	}

	void symmetricYield() {
		if (scheduledCoroutines != currentCoroutine) {
			throw new IllegalThreadStateException("Cannot call yield from within an asymmetric coroutine");
		}
		assert currentCoroutine instanceof Coroutine;

		if (TRACE) {
			System.out.println("locking for symmetric yield...");
		}

		Coroutine next = scheduledCoroutines.next;
		if (next == scheduledCoroutines) {
			return;
		}

		if (TRACE) {
			System.out.println("symmetric yield to " + next);
		}

		final Coroutine current = scheduledCoroutines;
		scheduledCoroutines = next;
		currentCoroutine = next;

		switchTo(current, next);
	}

	public void symmetricYieldTo(Coroutine target) {
		if (scheduledCoroutines != currentCoroutine) {
			throw new IllegalThreadStateException("Cannot call yield from within an asymmetric coroutine");
		}
		assert currentCoroutine instanceof Coroutine;

		moveCoroutine(scheduledCoroutines, target);

		final Coroutine current = scheduledCoroutines;
		scheduledCoroutines = target;
		currentCoroutine = target;

		switchTo(current, target);
	}

	private void moveCoroutine(Coroutine a, Coroutine position) {
		// remove a from the ring
		a.last.next = a.next;
		a.next.last = a.last;

		// ... and insert at the new position
		a.next = position.next;
		a.last = position;
		a.next.last = a;
		position.next = a;
	}

	public void symmetricStopCoroutine(Coroutine target) {
		if (scheduledCoroutines != currentCoroutine) {
			throw new IllegalThreadStateException("Cannot call yield from within an asymmetric coroutine");
		}
		assert currentCoroutine instanceof Coroutine;

		moveCoroutine(scheduledCoroutines, target);

		final Coroutine current = scheduledCoroutines;
		scheduledCoroutines = target;
		currentCoroutine = target;

		switchToAndExit(current, target);
	}

	void symmetricExitInternal(Coroutine coroutine) {
		if (scheduledCoroutines != currentCoroutine) {
			throw new IllegalThreadStateException("Cannot call exitNext from within an unscheduled coroutine");
		}
		assert currentCoroutine instanceof Coroutine;
		assert currentCoroutine != coroutine;

		// remove the coroutine from the ring
		coroutine.last.next = coroutine.next;
		coroutine.next.last = coroutine.last;

		if (!isDisposable(coroutine.data)) {
			// and insert it before the current coroutine
			coroutine.last = scheduledCoroutines.last;
			coroutine.next = scheduledCoroutines;
			coroutine.last.next = coroutine;
			scheduledCoroutines.last = coroutine;

			final Coroutine current = scheduledCoroutines;
			scheduledCoroutines = coroutine;
			currentCoroutine = coroutine;
			switchToAndExit(current, coroutine);
		}
	}

	void asymmetricCall(AsymCoroutine<?, ?> target) {
		if (target.threadSupport != this) {
			throw new IllegalArgumentException("Cannot activate a coroutine that belongs to another thread");
		}
		if (target.caller != null) {
			throw new IllegalArgumentException("Coroutine already in use");
		}
		if (target.data == 0) {
			throw new IllegalArgumentException("Target coroutine has already finished");
		}
		if (TRACE) {
			System.out.println("yieldCall " + target + " (" + target.data + ")");
		}

		final CoroutineBase current = currentCoroutine;
		target.caller = current;
		currentCoroutine = target;
		switchTo(target.caller, target);
	}

	void asymmetricReturn(final AsymCoroutine<?, ?> current) {
		if (current != currentCoroutine) {
			throw new IllegalThreadStateException("cannot return from non-current fiber");
		}
		final CoroutineBase caller = current.caller;
		if (TRACE) {
			System.out.println("yieldReturn " + caller + " (" + caller.data + ")");
		}

		current.caller = null;
		currentCoroutine = caller;
		switchTo(current, currentCoroutine);
	}

	void asymmetricReturnAndTerminate(final AsymCoroutine<?, ?> current) {
		if (current != currentCoroutine) {
			throw new IllegalThreadStateException("cannot return from non-current fiber");
		}
		final CoroutineBase caller = current.caller;
		if (TRACE) {
			System.out.println("yieldReturn " + caller + " (" + caller.data + ")");
		}

		current.caller = null;
		currentCoroutine = caller;
		switchToAndTerminate(current, currentCoroutine);
	}

	void terminateCoroutine() {
		assert currentCoroutine == scheduledCoroutines;
		assert currentCoroutine != threadCoroutine : "cannot exit thread coroutine";
		assert scheduledCoroutines != scheduledCoroutines.next : "last coroutine shouldn't call coroutineexit";

		Coroutine old = scheduledCoroutines;
		Coroutine forward = old.next;
		currentCoroutine = forward;
		scheduledCoroutines = forward;
		old.last.next = old.next;
		old.next.last = old.last;

		if (DEBUG) {
			System.out.println("to be terminated: " + old);
		}
		switchToAndTerminate(old, forward);
	}

	void terminateCallable() {
		assert currentCoroutine != scheduledCoroutines;
		assert currentCoroutine instanceof AsymCoroutine<?, ?>;

		if (DEBUG) {
			System.out.println("to be terminated: " + currentCoroutine);
		}
		asymmetricReturnAndTerminate((AsymCoroutine<?, ?>) currentCoroutine);
	}

	public boolean isCurrent(CoroutineBase coroutine) {
		return coroutine == currentCoroutine;
	}

	public CoroutineBase getCurrent() {
		return currentCoroutine;
	}

	private static native void registerNatives();

	private static native long getThreadCoroutine();

	private static native long createCoroutine(CoroutineBase coroutine, long stacksize);

	private static native void switchTo(CoroutineBase current, CoroutineBase target);

	private static native void switchToAndTerminate(CoroutineBase current, CoroutineBase target);

	private static native void switchToAndExit(CoroutineBase current, CoroutineBase target);

	private static native boolean isDisposable(long coroutine);

	private static native CoroutineBase cleanupCoroutine();

}
