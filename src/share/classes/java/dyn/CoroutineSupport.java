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

import com.alibaba.wisp.engine.WispTask;
import sun.misc.Contended;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;
import sun.reflect.generics.reflectiveObjects.NotImplementedException;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;

@Contended
public class CoroutineSupport {

    private static final boolean CHECK_LOCK = true;
    private static final int SPIN_BACKOFF_LIMIT = 2 << 8;

    private final static AtomicInteger idGen = new AtomicInteger();

    // The thread that this CoroutineSupport belongs to. There's only one CoroutineSupport per Thread
    private final Thread thread;
    // The initial coroutine of the Thread
    private final Coroutine threadCoroutine;

    // The currently executing coroutine
    private Coroutine currentCoroutine;

    private volatile Thread lockOwner = null; // also protect double link list of JavaThread->coroutine_list()
    private int lockRecursive; // volatile is not need

    private final int id;
    private boolean terminated = false;

    static {
        registerNatives();
    }

    public CoroutineSupport(Thread thread) {
        JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
        if (jla != null && jla.getCoroutineSupport(thread) != null) {
            throw new IllegalArgumentException("Cannot instantiate CoroutineThreadSupport for existing Thread");
        }
        id = idGen.incrementAndGet();
        this.thread = thread;
        threadCoroutine = new Coroutine(this, getNativeThreadCoroutine());
        markThreadCoroutine(threadCoroutine.nativeCoroutine, threadCoroutine);
        currentCoroutine = threadCoroutine;
    }

    public Coroutine threadCoroutine() {
        return threadCoroutine;
    }

    void addCoroutine(Coroutine coroutine, long stacksize) {
        assert currentCoroutine != null;
        lock();
        try {
            coroutine.nativeCoroutine = createCoroutine(coroutine, stacksize);
        } finally {
            unlock();
        }
    }

    Thread getThread() {
        return thread;
    }

    public static void checkAndThrowException(Coroutine coroutine) {
        checkAndThrowException0(coroutine.nativeCoroutine);
    }

    public void drain() {
        if (Thread.currentThread() != thread) {
            throw new IllegalArgumentException("Cannot drain another threads CoroutineThreadSupport");
        }

        lock();
        try {
            // drain all coroutines
            Coroutine next = null;
            while ((next = getNextCoroutine(currentCoroutine.nativeCoroutine)) != currentCoroutine) {
                symmetricExitInternal(next);
            }

            CoroutineBase coro;
            while ((coro = cleanupCoroutine()) != null) {
                System.out.println(coro);
                throw new NotImplementedException();
            }
        } catch (Throwable t) {
            t.printStackTrace();
        } finally {
            assert lockOwner == thread && lockRecursive == 0;
            terminated = true;
            unlock();
        }
    }

    /**
     * optimized version of symmetricYieldTo based on assumptions:
     * 1. we won't simultaneously steal a {@link Coroutine} from other threads
     * 2. we won't switch to a {@link Coroutine} that's being stolen
     * 3. we won't steal a running {@link Coroutine}
     * this function should only be called in
     * {@link com.alibaba.wisp.engine.WispTask#switchTo(WispTask, WispTask, boolean)},
     * we skipped unnecessary lock to improve performance.
     * @param target
     */
    public boolean unsafeSymmetricYieldTo(Coroutine target) {
        if (target.threadSupport != this) {
            return false;
        }
        final Coroutine current = currentCoroutine;
        currentCoroutine = target;
        switchTo(current, target);
        //check if locked by exiting coroutine
        beforeResume(current);
        return true;
    }

    public void symmetricYieldTo(Coroutine target) {
        lock();
        if (target.threadSupport != this) {
            unlock();
            return;
        }
        moveCoroutine(currentCoroutine.nativeCoroutine, target.nativeCoroutine);
        unlockLater(target);
        unsafeSymmetricYieldTo(target);
    }


    public void symmetricStopCoroutine(Coroutine target) {
        Coroutine current;
        lock();
        try {
            if (target.threadSupport != this) {
                unlock();
                return;
            }
            current = currentCoroutine;
            currentCoroutine = target;
            moveCoroutine(current.nativeCoroutine, target.nativeCoroutine);
        } finally {
            unlock();
        }
        switchToAndExit(current, target);
    }


    /**
     * switch to coroutine and throw Exception in coroutine
     */
    void symmetricExitInternal(Coroutine coroutine) {
        assert currentCoroutine != coroutine;
        assert coroutine.threadSupport == this;

        if (!testDisposableAndTryReleaseStack(coroutine.nativeCoroutine)) {
            moveCoroutine(currentCoroutine.nativeCoroutine, coroutine.nativeCoroutine);

            final Coroutine current = currentCoroutine;
            currentCoroutine = coroutine;
            switchToAndExit(current, coroutine);
            beforeResume(current);
        }
    }

    /**
     * terminate current coroutine and yield forward
     */
    public void terminateCoroutine(Coroutine target) {
        assert currentCoroutine != threadCoroutine : "cannot exit thread coroutine";
        assert currentCoroutine != getNextCoroutine(currentCoroutine.nativeCoroutine) :
                "last coroutine shouldn't call coroutineexit";

        lock();
        Coroutine old = currentCoroutine;
        Coroutine forward = target;
        if (forward == null) {
            forward = getNextCoroutine(old.nativeCoroutine);
        }
        currentCoroutine = forward;
        unlockLater(forward);
        switchToAndTerminate(old, forward);

        // should never run here.
        assert false;
    }

    /**
     * Steal coroutine from it's carrier thread to current thread.
     *
     * @param failOnContention steal fail if there's too much lock contention
     *
     * @param coroutine to be stolen
     */
    Coroutine.StealResult steal(Coroutine coroutine, boolean failOnContention) {
        assert coroutine.threadSupport.threadCoroutine() != coroutine;
        CoroutineSupport source = this;
        JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
        CoroutineSupport target = jla.getCoroutineSupport(jla.currentThread0());

        if (source == target) {
            return Coroutine.StealResult.SUCCESS;
        }

        if (source.id < target.id) { // prevent dead lock
            if (!source.lockInternal(failOnContention)) {
                return Coroutine.StealResult.FAIL_BY_CONTENTION;
            }
            target.lock();
        } else {
            target.lock();
            if (!source.lockInternal(failOnContention)) {
                target.unlock();
                return Coroutine.StealResult.FAIL_BY_CONTENTION;
            }
        }

        try {
            if (source.terminated || coroutine.finished ||
                    coroutine.threadSupport != source || // already been stolen
                    source.currentCoroutine == coroutine) {
                return Coroutine.StealResult.FAIL_BY_STATUS;
            }
            if (!stealCoroutine(coroutine.nativeCoroutine)) { // native frame
                return Coroutine.StealResult.FAIL_BY_NATIVE_FRAME;
            }
            coroutine.threadSupport = target;
        } finally {
            source.unlock();
            target.unlock();
        }

        return Coroutine.StealResult.SUCCESS;
    }

    /**
     * Can not be stolen while executing this, because lock is held
     */
    void beforeResume(CoroutineBase source) {
        if (source.needsUnlock) {
            source.needsUnlock = false;
            source.threadSupport.unlock();
        }
    }

    private void unlockLater(CoroutineBase next) {
        if (CHECK_LOCK && next.needsUnlock) {
            throw new InternalError("pending unlock");
        }
        next.needsUnlock = true;
    }

    private void lock() {
        boolean success = lockInternal(false);
        assert success;
    }

    private boolean lockInternal(boolean tryingLock) {
        final Thread th = SharedSecrets.getJavaLangAccess().currentThread0();
        if (lockOwner == th) {
            lockRecursive++;
            return true;
        }
        for (int spin = 1; ; ) {
            if (lockOwner == null && LOCK_UPDATER.compareAndSet(this, null, th)) {
                return true;
            }
            for (int i = 0; i < spin; ) {
                i++;
            }
            if (spin == SPIN_BACKOFF_LIMIT) {
                if (tryingLock) {
                    return false;
                }
                SharedSecrets.getJavaLangAccess().yield0(); // yield safepoint
            } else { // back off
                spin *= 2;
            }
        }
    }

    private void unlock() {
        if (CHECK_LOCK && SharedSecrets.getJavaLangAccess().currentThread0() != lockOwner) {
            throw new InternalError("unlock from non-owner thread");
        }
        if (lockRecursive > 0) {
            lockRecursive--;
        } else {
            LOCK_UPDATER.lazySet(this, null);
        }
    }

    private static final AtomicReferenceFieldUpdater<CoroutineSupport, Thread> LOCK_UPDATER;

    static {
        LOCK_UPDATER = AtomicReferenceFieldUpdater.newUpdater(CoroutineSupport.class, Thread.class, "lockOwner");
    }

    public boolean isCurrent(CoroutineBase coroutine) {
        return coroutine == currentCoroutine;
    }

    public CoroutineBase getCurrent() {
        return currentCoroutine;
    }


    private static native void registerNatives();

    private static native long getNativeThreadCoroutine();

    /**
     * need lock because below methods will operate on thread->coroutine_list()
     */
    private static native long createCoroutine(CoroutineBase coroutine, long stacksize);

    private static native void switchToAndTerminate(CoroutineBase current, CoroutineBase target);

    private static native boolean testDisposableAndTryReleaseStack(long coroutine);

    private static native boolean stealCoroutine(long coroPtr);
    // end of locking

    /**
     * get next {@link Coroutine} from current thread's doubly linked {@link Coroutine} list
     * @param coroPtr hotspot coroutine
     * @return java Coroutine
     */
    private static native Coroutine getNextCoroutine(long coroPtr);

    /**
     * move coroPtr to targetPtr's next field in underlying hotspot coroutine list
     * @param coroPtr current threadCoroutine
     * @param targetPtr coroutine that is about to exit
     */
    private static native void moveCoroutine(long coroPtr, long targetPtr);

    /**
     * track hotspot couroutine with java coroutine.
     * @param coroPtr threadCoroutine in hotspot
     * @param threadCoroutine threadCoroutine in java
     */
    private static native void markThreadCoroutine(long coroPtr, CoroutineBase threadCoroutine);

    private static native void switchTo(CoroutineBase current, CoroutineBase target);

    private static native void switchToAndExit(CoroutineBase current, CoroutineBase target);

    private static native CoroutineBase cleanupCoroutine();

    public static native void setWispBooted();

    /**
     * this will turn on a safepoint to stop all threads.
     * @param coroPtr coroutine pointer used in VM.
     * @return target coroutine's stack
     */
    public static native StackTraceElement[] getCoroutineStack(long coroPtr);

    private static native void checkAndThrowException0(long coroPtr);
}
