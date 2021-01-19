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

package com.alibaba.wisp.engine;

import sun.misc.SharedSecrets;
import sun.misc.UnsafeAccess;

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.dyn.Coroutine;
import java.dyn.CoroutineExitException;
import java.nio.channels.SelectableChannel;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerFieldUpdater;
import java.util.concurrent.atomic.AtomicLongFieldUpdater;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;


/**
 * {@link WispTask} provides high-level semantics of {link @Coroutine}
 * <p>
 * Create {@link WispTask} via {@link WispEngine#dispatch(Runnable)} (Callable, String)} to make
 * blocking IO operation in {@link WispTask}s to become concurrent.
 * <p>
 * The creator and a newly created {@link WispTask} will automatically have parent-children relationship.
 * When the child gets blocked on something, the {@link WispCarrier} will try to execute parent first.
 * <p>
 * A {@link WispTask}'s exit will wake up the waiting parent.
 */
public class WispTask implements Comparable<WispTask> {
    private final static AtomicInteger idGenerator = new AtomicInteger();

    static final Map<Integer, WispTask> id2Task = new ConcurrentHashMap<>(360);
    // global table used for all WispCarriers

    static WispTask fromId(int id) {
        WispCarrier carrier = WispCarrier.current();
        boolean isInCritical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            return id2Task.get(id);
        } finally {
            carrier.isInCritical = isInCritical0;
        }
    }

    static void cleanExitedTasks(List<WispTask> tasks) {
        if (!tasks.isEmpty()) {
            WispCarrier carrier = tasks.get(0).carrier;
            boolean isInCritical0 = carrier.isInCritical;
            carrier.isInCritical = true;
            try {
                for (WispTask t : tasks) {
                    id2Task.remove(t.id);
                    t.cleanup();
                }
            } finally {
                carrier.isInCritical = isInCritical0;
            }
        }
    }

    static void cleanExitedTask(WispTask task) {
        WispCarrier carrier = WispCarrier.current();
        boolean isInCritical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            // cleanup shouldn't be executed for thread task
            id2Task.remove(task.id);
        } finally {
            carrier.isInCritical = isInCritical0;
        }
    }

    static void trackTask(WispTask task) {
        WispCarrier carrier = WispCarrier.current();
        boolean isInCritical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            id2Task.put(task.id, task);
        } finally {
            carrier.isInCritical = isInCritical0;
        }
    }

    private final int id;

    enum Status {
        ALIVE,      // ALIVE
        CACHED,     // exited
        DEAD        // quited and never be used
    }

    private Runnable runnable; // runnable for created task

    /**
     * Task is running in that carrier.
     */
    volatile WispCarrier carrier;

    private String name;
    final Coroutine ctx;                // the low-level coroutine implement
    Status status = Status.ALIVE;
    SelectableChannel ch;               // the interesting channel
    TimeOut timeOut;                    // related timer
    ClassLoader ctxClassLoader;

    private final boolean isThreadTask;
    private boolean isThreadAsWisp;

    private Thread threadWrapper;       // thread returned by Thread::currentThread()
    private volatile int interrupted;   // 0 means not interrupted
    private volatile int alreadyCheckNativeInterrupt;

    private volatile int jdkParkStatus;
    private volatile int jvmParkStatus;
    volatile int stealLock;
    private WispTask from;
    /**
     * WispTask execution wrapper for schduler should only be used in wakupTask
     */
    final StealAwareRunnable resumeEntry;
    // counter printed by jstack
    private int activeCount;
    int stealCount;
    int stealFailureCount;
    private int preemptCount;
    long ttr;
    // perf monitor
    private long enqueueTime;
    private long parkTime;
    private long blockingTime;
    private long registerEventTime;
    long enterTs;
    long totalTs;

    // monolithic epoll support
    private volatile long epollArray;
    private volatile int epollEventNum;
    int epollArraySize;

    WispControlGroup controlGroup;

    WispTask(WispCarrier carrier, Coroutine ctx, boolean isRealTask, boolean isThreadTask) {
        this.isThreadTask = isThreadTask;
        this.id = isRealTask ? idGenerator.addAndGet(1) : -1;
        setCarrier(carrier);
        if (isRealTask) {
            this.ctx = ctx != null ? ctx : new CacheableCoroutine(WispConfiguration.STACK_SIZE);
            this.ctx.setWispTask(id, this, carrier);
        } else {
            this.ctx = null;
        }
        resumeEntry = isThreadTask ? null : carrier.createResumeEntry(this);
    }

    void reset(Runnable runnable, String name, Thread thread, ClassLoader ctxLoader) {
        assert ctx != null;
        this.status       = Status.ALIVE;
        this.runnable     = runnable;
        this.name         = name;
        this.controlGroup = null;
        interrupted       = 0;
        ctxClassLoader    = ctxLoader;
        ch                = null;
        enqueueTime       = 0;
        parkTime          = 0;
        blockingTime      = 0;
        registerEventTime = 0;

        activeCount       = 0;
        stealCount        = 0;
        stealFailureCount = 0;
        preemptCount      = 0;
        totalTs           = 0;
        enterTs           = 0;

        // thread status
        if (thread != null) { // calling from Thread.start()
            NATIVE_INTERRUPTED_UPDATER.lazySet(this, 1);
            isThreadAsWisp = true;
            WispEngine.JLA.setWispTask(thread, this);
            threadWrapper = thread;
        } else {
            // for WispThreadWrapper, skip native interrupt check
            NATIVE_INTERRUPTED_UPDATER.lazySet(this, 0);
            isThreadAsWisp = false;
            if (threadWrapper == null) {
                threadWrapper = new WispThreadWrapper(this);
            }
            WispEngine.JLA.setWispAlive(threadWrapper, true);
        }
        assert WispEngine.JLA.getWispTask(threadWrapper) == this;

        if (!isThreadTask() && name != null && !threadWrapper.getName().equals(name)) {
            threadWrapper.setName(name);
        }
    }

    void setCarrier(WispCarrier carrier) {
        CARRIER_UPDATER.lazySet(this, carrier);
    }

    private void cleanup() {
        setCarrier(null);
        threadWrapper = null;
        ctxClassLoader = null;
    }

    class CacheableCoroutine extends Coroutine {
        CacheableCoroutine(long stacksize) {
            super(stacksize);
        }

        @Override
        protected void run() {
            while (true) {
                assert WispCarrier.current() == carrier;
                assert carrier.current == WispTask.this;
                if (runnable != null) {
                    Throwable throwable = null;
                    try {
                        runCommand();
                    } catch (Throwable t) {
                        dispatchUncaughtException(t);
                        throwable = t;
                    } finally {
                        assert timeOut == null;
                        assert controlGroup == null; // detached
                        runnable = null;
                        WispEngine.JLA.setWispAlive(threadWrapper, false);
                        if (isThreadAsWisp) {
                            ThreadAsWisp.exit(threadWrapper);
                        }
                        if (throwable instanceof CoroutineExitException) {
                            throw (CoroutineExitException) throwable;
                        }
                        carrier.taskExit();

                    }
                } else {
                    carrier.schedule(false);
                }
            }
        }
    }

    private void runCommand() {
        ResourceContainer rc = WispEngine.JLA.getInheritedResourceContainer(threadWrapper);
        if (rc != null) {
            rc.run(wrapRunOutsideWisp(runnable));
        } else {
            runOutsideWisp(runnable);
        }
    }

    private void dispatchUncaughtException(Throwable t) {
        try {
            threadWrapper.getUncaughtExceptionHandler()
                    .uncaughtException(threadWrapper, t);
        } catch (Throwable e) {
            // the default error log is generated with the format
            // from hotspot JavaThread::exit
            String defaultErrorLog = String.format(
                    "\nException: %s thrown from the UncaughtExceptionHandler in thread \"%s\"",
                    e.getClass().getName(), threadWrapper.getName());
            System.err.println(defaultErrorLog);
        }
    }

    /**
     * Mark if wisp is running internal scheduling code or user code, this would
     * be used in preempt to identify if it's okay to preempt
     * Modify Coroutine::is_usermark_frame accordingly if you need to change this
     * method, because it's name and sig are used
     */
    static void runOutsideWisp(Runnable runnable) {
        runnable.run();
    }

    static Runnable wrapRunOutsideWisp(Runnable runnable) {
        return () -> runOutsideWisp(runnable);
    }

    /**
     * Switch task. we need the information of {@code from} task param
     * to do classloader switch etc..
     * <p>
     * @param terminal indicate terminal current coroutine
     * {@link #stealLock} is used in {@link WispCarrier#steal(WispTask)} .
     */
    static boolean switchTo(WispTask current, WispTask next, boolean terminal) {
        assert next.ctx != null;
        assert WispCarrier.current() == current.carrier;
        assert current.carrier == next.carrier;
        next.activeCount++;
        assert current.isThreadTask() || next.isThreadTask();
        next.from = current;
        STEAL_LOCK_UPDATER.lazySet(next, 1);
        // store load barrier is not necessary
        assert current.carrier.thread == WispEngine.JLA.currentThread0();
        boolean res = false;
        if (terminal == true) {
            WispEngine.JLA.getCoroutineSupport(current.carrier.thread).terminateCoroutine(next.ctx);
            // should never run here.
            assert false: "should not reach here";
        } else {
            res = WispEngine.JLA.getCoroutineSupport(current.carrier.thread).unsafeSymmetricYieldTo(next.ctx);
        }
        assert current.stealLock != 0;
        STEAL_LOCK_UPDATER.lazySet(current.from, 0);
        assert WispCarrier.current() == current.carrier;
        assert current.carrier.current == current;
        return res;
    }

    /**
     * @return {@code false} if current {@link WispTask} is thread-emulated.
     */
    boolean isThreadTask() {
        return isThreadTask;
    }

    /**
     * Let currently executing task sleep for specified number of milliseconds.
     * <p>
     * May be wakened up early by an available IO.
     */
    static void sleep(long ms) {
        if (ms < 0) throw new IllegalArgumentException();

        if (ms == 0) {
            WispCarrier.current().yield();
        } else {
            WispCarrier.current().unregisterEvent();
            jdkPark(TimeUnit.MILLISECONDS.toNanos(ms));
        }
    }

    @Override
    public String toString() {
        return "WispTask" + id + "(" +
                "name=" + name + ')' +
                "{status=" + status + "/" +
                jdkParkStatus + ", " +
                '}';
    }

    public String getName() {
        return name;
    }


    private static final int
            WAITING = -1,   // was blocked
            FREE = 0,       // the Initial Park status
            PERMITTED = 1;  // another task give a permit to make the task not block at next park()

    static final String SHUTDOWN_TASK_NAME = "SHUTDOWN_TASK";

    boolean isAlive() {
        return status == Status.ALIVE;
    }

    /**
     * If a permit is available, it will be consumed and this function returns
     * immediately; otherwise
     * current task will become blocked until {@link #unpark()} ()} happens.
     *
     * @param timeoutNano <= 0 park forever
     *                    else park with given timeout
     */
    private void parkInternal(long timeoutNano, boolean fromJvm) {
        if (timeoutNano > 0 && timeoutNano < WispConfiguration.MIN_PARK_NANOS) {
            carrier.yield();
            return;
        }
        final AtomicIntegerFieldUpdater<WispTask> statusUpdater = fromJvm ? JVM_PARK_UPDATER : JDK_PARK_UPDATER;
        final boolean isInCritical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            carrier.getCounter().incrementParkCount();
            for (;;) {
                int s = statusUpdater.get(this);
                assert s != WAITING; // if parkStatus == WAITING, should already blocked

                if (s == FREE && statusUpdater.compareAndSet(this, FREE, WAITING)) {
                    // may become PERMITTED here; need retry.
                    // another thread unpark here is ok:
                    // current task is put to unpark queue,
                    // and will wake up eventually
                    if (WispEngine.runningAsCoroutine(threadWrapper) && timeoutNano > 0) {
                        carrier.addTimer(timeoutNano + System.nanoTime(), fromJvm ? TimeOut.Action.JVM_UNPARK : TimeOut.Action.JDK_UNPARK);
                    }
                    carrier.isInCritical = isInCritical0;
                    try {
                        if (WispEngine.runningAsCoroutine(threadWrapper)) {
                            setParkTime();
                            carrier.schedule(false);
                        } else {
                            UA.park0(false, timeoutNano < 0 ? 0 : timeoutNano);
                        }
                    } finally {
                        carrier.isInCritical = true;
                        if (timeoutNano > 0) {
                            carrier.cancelTimer();
                        }
                        // we'may direct wakeup by current carrier
                        // the statue may be still WAITING..
                        statusUpdater.lazySet(this, FREE);
                    }
                    break;
                } else if (s == PERMITTED &&
                        (statusUpdater.compareAndSet(this, PERMITTED, FREE))) {
                    // consume the permit
                    break;
                }
            }
        } finally {
            carrier.isInCritical = isInCritical0;
        }
    }

    /**
     * If the thread was blocked on {@link #park(long)} then it will unblock.
     * Otherwise, its next call to {@link #park(long)} is guaranteed not to block.
     */
    private void unparkInternal(boolean fromJvm) {
        AtomicIntegerFieldUpdater<WispTask> statusUpdater = fromJvm ? JVM_PARK_UPDATER : JDK_PARK_UPDATER;
        for (;;) {
            int s = statusUpdater.get(this);
            if (s == WAITING && statusUpdater.compareAndSet(this, WAITING, FREE)) {
                if (WispEngine.runningAsCoroutine(threadWrapper)) {
                    recordOnUnpark(fromJvm);
                    carrier.wakeupTask(this);
                } else {
                    UA.unpark0(threadWrapper);
                }
                break;
            } else if (s == PERMITTED ||
                    (s == FREE && statusUpdater.compareAndSet(this, FREE, PERMITTED))) {
                // add a permit
                break;
            }
        }
    }

    /**
     * Park Invoked by jdk, include IO, JUC etc..
     */
    static void jdkPark(long timeoutNano) {
        WispCarrier.current().getCurrentTask().parkInternal(timeoutNano, false);
    }

    void jdkUnpark() {
        unparkInternal(false);
    }

    /**
     * Invoked by VM to support coroutine switch in object monitor case.
     */
    private static void park(long timeoutNano) {
        WispCarrier.current().getCurrentTask().parkInternal(timeoutNano, true);
    }

    void unpark() {
        unparkInternal(true);
    }

    // direct called by jvm runtime if UseDirectUnpark
    static void unparkById(int id) {
        WispTask t = fromId(id);
        if (t != null) {
            t.unpark();
        }
    }

    void interrupt() {
        // For JSR166. Unpark even if interrupt status was already set.
        interrupted = 1;
        unpark();
        jdkUnpark();
    }

    private static void interruptById(int id) {
        WispTask t = fromId(id);
        if (t != null) {
            t.interrupt();
        }
    }

    boolean isInterrupted() {
        return interrupted != 0;
    }

    boolean inDestoryedGroup() {
        return controlGroup != null && controlGroup.destroyed;
    }

    boolean inheritedFromNonRootContainer() {
        return WispEngine.JLA.getInheritedResourceContainer(threadWrapper) != ResourceContainer.root();
    }

    boolean testInterruptedAndClear(boolean clear) {
        boolean nativeInterrupt = false;
        if (alreadyCheckNativeInterrupt == 0 && // only do it once
                NATIVE_INTERRUPTED_UPDATER.compareAndSet(this, 0, 1) &&
                !isInterrupted()) {
            nativeInterrupt = checkAndClearNativeInterruptForWisp(threadWrapper);
        }
        boolean res = interrupted != 0 || nativeInterrupt;
        if (res && clear) {
            INTERRUPTED_UPDATER.lazySet(this, 0);
        }
        // return old interrupt status.
        return res;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        WispTask t = (WispTask) o;
        return Objects.equals(id, t.id);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id);
    }

    public Thread getThreadWrapper() {
        return threadWrapper;
    }

    void setThreadWrapper(Thread thread) {
        threadWrapper = thread;
        WispEngine.JLA.setWispTask(thread, this);
    }

    void resetThreadWrapper() {
        if (isThreadAsWisp) {
            threadWrapper = null;
        }
    }

    @Override
    public int compareTo(WispTask o) {
        return Integer.compare(this.id, o.id);
    }

    long getEpollArray() {
        return epollArray;
    }

    void setEpollArray(long epollArray) {
        EPOLL_ARRAY_UPDATER.lazySet(this, epollArray);
    }

    int getEpollEventNum() {
        return epollEventNum;
    }

    void setEpollEventNum(int epollEventNum) {
        EPOLL_EVENT_NUM_UPDATER.lazySet(this, epollEventNum);
    }

    void updateEnqueueTime() {
        if (!WispConfiguration.WISP_PROFILE) {
            return;
        }
        // in wisp2, if the task is stealed unsuccessfully, it will be put into queue again
        if (enqueueTime != 0) {
            return;
        }
        enqueueTime = System.nanoTime();
    }

    long getEnqueueTime() {
        return enqueueTime;
    }

    void resetEnqueueTime() {
        enqueueTime = 0;
    }

    void setRegisterEventTime() {
        // only count the time which is spent on WispTask by service
        registerEventTime = (!WispConfiguration.WISP_PROFILE || isThreadTask) ? 0 : System.nanoTime();
    }

    void resetRegisterEventTime() {
        registerEventTime = 0;
    }

    void countWaitSocketIOTime() {
        if (registerEventTime != 0) {
            carrier.counter.incrementTotalWaitSocketIOTime(System.nanoTime() - registerEventTime);
            registerEventTime = 0;
        }
    }

    private void setParkTime() {
        parkTime = (!WispConfiguration.WISP_PROFILE || isThreadTask) ? 0 : System.nanoTime();
    }

    /* When unpark is called, the time is set.
     * Since the unpark may be called by non-worker thread, the count is delayed.
     */
    private void recordOnUnpark(boolean fromJVM) {
        if (!WispConfiguration.WISP_PROFILE) {
            return;
        }
        if (parkTime != 0) {
            blockingTime = System.nanoTime() - parkTime;
            if (blockingTime < 0) {
                blockingTime = 0;
            }
            parkTime = 0;
        }
        if (fromJVM) {
            carrier.counter.incrementUnparkFromJvmCount();
        }
    }

    void countExecutionTime(long beginTime) {
        // TaskExit set beginTime to 0, and calls schedule,
        // then beginTime is 0. It need to skip it.
        if (!WispConfiguration.WISP_PROFILE || beginTime == 0) {
            return;
        }
        carrier.counter.incrementTotalExecutionTime(System.nanoTime() - beginTime);
        if (blockingTime != 0) {
            carrier.counter.incrementTotalBlockingTime(blockingTime);
            blockingTime = 0;
        }
    }

    StackTraceElement[] getStackTrace() {
        return this.threadWrapper.getStackTrace();
    }

    long getCpuTime() {
        // concurrent inaccurate is acceptable
        long currentEnterTs = enterTs;
        return totalTs + (currentEnterTs == 0 ? 0 : System.nanoTime() - currentEnterTs);
    }

    private static final AtomicReferenceFieldUpdater<WispTask, WispCarrier> CARRIER_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> JVM_PARK_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> JDK_PARK_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> INTERRUPTED_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> NATIVE_INTERRUPTED_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> STEAL_LOCK_UPDATER;
    private static final AtomicLongFieldUpdater<WispTask> EPOLL_ARRAY_UPDATER;
    private static final AtomicIntegerFieldUpdater<WispTask> EPOLL_EVENT_NUM_UPDATER;
    private static final UnsafeAccess UA = SharedSecrets.getUnsafeAccess();

    private static native void registerNatives();

    // only for wisp to clear the native interrupt, for parallel interrupt problem.
    private static native boolean checkAndClearNativeInterruptForWisp(Thread cur);

    static {
        CARRIER_UPDATER = AtomicReferenceFieldUpdater.newUpdater(WispTask.class, WispCarrier.class, "carrier");
        JVM_PARK_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "jvmParkStatus");
        JDK_PARK_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "jdkParkStatus");
        INTERRUPTED_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "interrupted");
        NATIVE_INTERRUPTED_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "alreadyCheckNativeInterrupt");
        STEAL_LOCK_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "stealLock");
        EPOLL_ARRAY_UPDATER = AtomicLongFieldUpdater.newUpdater(WispTask.class, "epollArray");
        EPOLL_EVENT_NUM_UPDATER = AtomicIntegerFieldUpdater.newUpdater(WispTask.class, "epollEventNum");
        registerNatives();
    }
}
