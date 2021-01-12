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

import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

public abstract class CoroutineBase {
    transient long nativeCoroutine;

    boolean finished = false;

    boolean needsUnlock = false;

    transient CoroutineSupport threadSupport;

    CoroutineBase() {
        JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
        Thread thread = jla.currentThread0();
        assert jla.getCoroutineSupport(thread) != null;
        this.threadSupport = jla.getCoroutineSupport(thread);
    }

    // creates the initial coroutine for a new thread
    CoroutineBase(CoroutineSupport threadSupport, long nativeCoroutine) {
        this.threadSupport = threadSupport;
        this.nativeCoroutine = nativeCoroutine;
    }

    protected abstract void run();

    @SuppressWarnings({"unused"})
    private final void startInternal() {
        assert threadSupport.getThread() == SharedSecrets.getJavaLangAccess().currentThread0();
        try {
            // When we symmetricYieldTo a newly created coroutine,
            // we'll expect the new coroutine release lock as soon as possible
            threadSupport.beforeResume(this);
            run();
        } catch (Throwable t) {
            if (!(t instanceof CoroutineExitException)) {
                t.printStackTrace();
            }
        } finally {
            finished = true;
            // threadSupport is fixed by steal()
            threadSupport.beforeResume(this);

            threadSupport.terminateCoroutine(null);
        }
        assert threadSupport.getThread() == SharedSecrets.getJavaLangAccess().currentThread0();
    }

    /**
     * Returns true if this coroutine has reached its end. Under normal circumstances this happens when the {@link #run()} method returns.
     */
    public final boolean isFinished() {
        return finished;
    }

    /**
     * @return the thread that this coroutine is associated with
     * @throws NullPointerException if the coroutine has been terminated
     */
    public Thread getThread() {
        return threadSupport.getThread();
    }

    public static CoroutineBase current() {
        JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
        return jla.getCoroutineSupport(jla.currentThread0()).getCurrent();
    }
}
