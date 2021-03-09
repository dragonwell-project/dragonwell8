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

import java.dyn.Coroutine;
import java.dyn.CoroutineSupport;
import java.io.IOException;
import java.nio.channels.SelectableChannel;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicIntegerFieldUpdater;

import com.alibaba.wisp.engine.WispTask.Status;

/**
 * {@link WispCarrier} schedules all {@link WispTask} on according worker and control their life cycle
 * {@link WispCarrier} exposed its scheduling function for wisp inner usage and maintained all thread local
 * schedule info, such as current active {@link WispTask} and thread local task cache, etc.
 *
 * <p> A {@link WispCarrier} instance is expected to run in a specific worker. Get per-thread instance by calling
 * {@link WispCarrier#current()}.
 */
final class WispCarrier implements Comparable<WispCarrier> {

    private static final AtomicIntegerFieldUpdater<WispEngine> TASK_COUNT_UPDATER =
            AtomicIntegerFieldUpdater.newUpdater(WispEngine.class, "runningTaskCount");

    /**
     * The user can only get thread-specific carrier by calling this method.
     * <p>
     * We can not use ThreadLocal any more, because if transparentAsync, it behaves as a coroutine local.
     *
     * @return thread-specific carrier
     */
    static WispCarrier current() {
        Thread thread = WispEngine.JLA.currentThread0();
        WispTask current = WispEngine.JLA.getWispTask(thread);
        if (current == null) {
            WispCarrier carrier = new WispCarrier(WispEngine.WISP_ROOT_ENGINE);
            if (carrier.threadTask.ctx != null) {
                WispEngine.JLA.setWispTask(thread, carrier.getCurrentTask());
                carrier.init();
            } // else: fake carrier used in jni attach
            return carrier;
        } else {
            return current.carrier;
        }
    }

    // thread, threadTask and worker are 1:1:1 related
    WispScheduler.Worker worker;
    final Thread thread;
    private final WispTask threadTask;
    WispEngine engine;
    // current running task
    WispTask current;
    private final List<WispTask> taskCache = new ArrayList<>();
    boolean isInCritical;
    WispCounter counter;
    int schedTick;
    int lastSchedTick; // access by Sysmon
    boolean terminated;
    private long switchTimestamp = 0;
    private WispTask yieldingTask;
    private TimeOut pendingTimer;

    private WispCarrier(WispEngine engine) {
        thread = WispEngine.JLA.currentThread0();
        this.engine = engine;
        CoroutineSupport cs = WispEngine.JLA.getCoroutineSupport(thread);
        current = threadTask = new WispTask(this,
                cs == null ? null : cs.threadCoroutine(),
                cs != null, true);
        if (cs == null) { // fake carrier used in jni attach
            threadTask.setThreadWrapper(thread);
        } else {
            threadTask.reset(null, "THREAD: " + thread.getName(), thread, thread.getContextClassLoader());
        }
    }

    /**
     * Use 2nd-phase init after constructor. Because if constructor calls Thread.currentThread(),
     * and recursive calls constructor, then stackOverflow.
     */
    private void init() {
        WispTask.trackTask(threadTask);
        counter = WispCounter.create(this);
    }

    /**
     * @return Currently running WispTask. Ensured by {@link #yieldTo(WispTask, boolean)}
     * If calling in a non-coroutine environment, return a thread-emulated WispTask.
     */
    WispTask getCurrentTask() {
        return current;
    }

    /**
     * Each WispCarrier has a corresponding worker. Thread can't be changed for WispCarrier.
     * Use thread id as WispCarrier id.
     *
     * @return WispCarrier id
     */
    long getId() {
        assert thread != null;
        return thread.getId();
    }

    // ----------------------------------------------- lifecycle

    final WispTask runTaskInternal(Runnable target, String name, Thread thread, ClassLoader ctxLoader) {
        if (engine.hasBeenShutdown && !WispTask.SHUTDOWN_TASK_NAME.equals(name)) {
            throw new RejectedExecutionException("Wisp carrier has been shutdown");
        }
        assert current == threadTask;
        boolean isInCritical0 = isInCritical;
        isInCritical = true;
        WispTask wispTask;
        try {
            counter.incrementCreateTaskCount();
            if ((wispTask = getTaskFromCache()) == null) {
                wispTask = new WispTask(this, null, true, false);
                WispTask.trackTask(wispTask);
            }
            wispTask.reset(target, name, thread, ctxLoader);
            TASK_COUNT_UPDATER.incrementAndGet(engine);
        } finally {
            isInCritical = isInCritical0;
        }
        wispTask.enterTs = System.nanoTime();
        yieldTo(wispTask, false);
        runWispTaskEpilog();

        return wispTask;
    }

    /**
     * The only exit path of a task.
     * WispTask must call {@code taskExit()} to exit safely.
     */
    void taskExit() { // and exit
        current.countExecutionTime(switchTimestamp);
        switchTimestamp = 0;
        current.setEpollArray(0);

        unregisterEvent();
        boolean cached = !current.shutdownPending && returnTaskToCache(current);
        TASK_COUNT_UPDATER.decrementAndGet(engine);
        if (cached) {
            current.status = WispTask.Status.CACHED;
        } else {
            current.status = WispTask.Status.DEAD;
            WispTask.cleanExitedTask(current);
        }

        // reset threadWrapper after call returnTaskToCache,
        // since the threadWrapper will be used in Thread.currentThread()
        current.resetThreadWrapper();
        counter.incrementCompleteTaskCount();

        // In Tenant killing process, we have an pending exception,
        // WispTask.Coroutine's loop will be breaked
        // invoke an explicit reschedule instead of return
        schedule(!cached);
    }

    /**
     * @return task from global cached theScheduler
     */
    private WispTask getTaskFromCache() {
        assert WispCarrier.current() == this;
        if (!taskCache.isEmpty()) {
            return taskCache.remove(taskCache.size() - 1);
        }
        if (engine.hasBeenShutdown) {
            return null;
        }
        WispTask task = engine.groupTaskCache.poll();
        if (task == null) {
            return null;
        }
        if (task.carrier != this) {
            if (steal(task) != Coroutine.StealResult.SUCCESS) {
                engine.groupTaskCache.add(task);
                return null;
            }
        }
        assert task.carrier == this;
        return task;
    }

    /**
     * cache task back to global or local cache and return true, if beyond the capacity of
     * cache will return false.
     */
    private boolean returnTaskToCache(WispTask task) {
        // reuse exited wispTasks from shutdown wispEngine is very tricky, so we'd better not return
        // these tasks to global cache
        if (taskCache.size() > WispConfiguration.WISP_ENGINE_TASK_CACHE_SIZE && !engine.hasBeenShutdown) {
            if (engine.groupTaskCache.size() > WispConfiguration.WISP_ENGINE_TASK_GLOBAL_CACHE_SIZE) {
                return false;
            } else {
                engine.groupTaskCache.add(task);
            }
        } else {
            taskCache.add(task);
        }
        return true;
    }

    /**
     * hook for yield wispTask
     */
    private void runWispTaskEpilog() {
        processPendingTimer();
        processYield();
    }

    void destroy() {
        WispTask.cleanExitedTasks(taskCache);
        WispTask.cleanExitedTask(threadTask);
        terminated = true;
    }

    // ------------------------------------------  scheduling

    /**
     * Block current coroutine and do scheduling.
     * Typically called when resource is not ready.
     * @param terminal indicate terminal current coroutine.
     */
    final void schedule(boolean terminal) {
        assert WispCarrier.current() == this;
        WispTask current = this.current;
        current.countExecutionTime(switchTimestamp);
        assert current.resumeEntry != null && current != threadTask
                : "call `schedule()` in scheduler";
        if (current.controlGroup != null) {
            current.totalTs +=  current.controlGroup.calcCpuTicks(current);
        } else {
            current.totalTs += System.nanoTime() -  current.enterTs;
            current.enterTs = 0;
        }
        current.resumeEntry.setStealEnable(true);
        yieldTo(threadTask, terminal); // letting the scheduler choose runnable task
        current.carrier.checkAndDispatchShutdown();
    }

    private void checkAndDispatchShutdown() {
        assert WispCarrier.current() == this;
        WispTask current = WispCarrier.current().getCurrentTask();
        if (shutdownPending(current) &&
                CoroutineSupport.checkAndThrowException(current.ctx)) {
            current.shutdownPending = true;
            throw new ThreadDeath();
        }
    }

    boolean shutdownPending(WispTask current) {
        return (engine.hasBeenShutdown
                || (current.inDestoryedGroup() && current.inheritedFromNonRootContainer()))
                && !WispTask.SHUTDOWN_TASK_NAME.equals(current.getName())
                && current.isAlive();
    }

    /**
     * Wake up a {@link WispTask} that belongs to this carrier
     *
     * @param task target task
     */
    void wakeupTask(WispTask task) {
        assert !task.isThreadTask();
        assert task.resumeEntry != null;
        assert task.carrier == this;
        task.updateEnqueueTime();
        engine.scheduler.executeWithWorkerThread(task.resumeEntry, thread);
    }

    /**
     * create a Entry runnable for wisp task,
     * used for bridge coroutine and Executor interface.
     */
    StealAwareRunnable createResumeEntry(WispTask task) {
        assert !task.isThreadTask();
        return new StealAwareRunnable() {
            boolean stealEnable = true;

            @Override
            public void run() {
                WispCarrier current = WispCarrier.current();
                /*
                 * Please be extremely cautious:
                 * task.carrier can not be changed here by other thread
                 * is based on our implementation of using park instead of
                 * direct schedule, so only one thread could receive
                 * this closure.
                 */
                WispCarrier source = task.carrier;
                if (source != current) {
                    Coroutine.StealResult res = current.steal(task);
                    if (res != Coroutine.StealResult.SUCCESS) {
                        if (res != Coroutine.StealResult.FAIL_BY_CONTENTION) {
                            stealEnable = false;
                        }
                        source.wakeupTask(task);
                        return;
                    }
                    // notify terminated empty worker to exit
                    if (source.worker.hasBeenHandoff && TASK_COUNT_UPDATER.get(source.engine) == 0) {
                        source.worker.signal();
                    }
                }
                current.countEnqueueTime(task.getEnqueueTime());
                task.resetEnqueueTime();
                if (task.controlGroup != null) {
                    long res = task.controlGroup.checkCpuLimit(task, false);
                    if (res != 0) {
                        current.resumeLater(System.nanoTime() + res, task);
                        return;
                    }
                } else {
                    task.enterTs = System.nanoTime();
                }
                if (current.yieldTo(task, false)) {
                    current.runWispTaskEpilog();
                } else { // switch failure
                    // this is unexpected, record in counter to help troubleshooting.
                    // The actual behavior of switch failure is similar to unpark lost,
                    // so we re-enqueue the entry for compensation.
                    resumeFailure++;
                    current.wakeupTask(task);
                }
            }

            @Override
            public void setStealEnable(boolean b) {
                stealEnable = b;
            }

            @Override
            public boolean isStealEnable() {
                return stealEnable;
            }
        };
    }

    private static int resumeFailure = 0;

    /**
     * Steal task from it's current bond carrier to this carrier
     *
     * @return steal result
     */
    private Coroutine.StealResult steal(WispTask task) {
        /* shutdown is an async operation in wisp2, SHUTDOWN task relies on runningTaskCount to
        determine whether it's okay to exit the worker, hence we need to make sure no more new
        wispTasks are created or stolen for hasBeenShutdown engines
        for example:
        1. SHUTDOWN task found runningTaskCount equals 0 and exit
        2. worker's task queue may still has some remaining tasks, when tried to steal these tasks
        we may encounter jvm crash.
        */
        if (engine.hasBeenShutdown) {
            return Coroutine.StealResult.FAIL_BY_STATUS;
        }

        assert WispCarrier.current() == this;
        assert !task.isThreadTask();
        if (task.carrier != this) {
            while (task.stealLock != 0) {/* wait until steal enabled */}
            Coroutine.StealResult res = task.ctx.steal(true);
            if (res != Coroutine.StealResult.SUCCESS) {
                task.stealFailureCount++;
                return res;
            }
            task.stealCount++;
            task.setCarrier(this);
        }
        return Coroutine.StealResult.SUCCESS;
    }

    /**
     * The ONLY entry point to a task,
     * {@link #current} will be set correctly
     *
     * @param terminal indicates terminal current coroutine
     */
    private boolean yieldTo(WispTask task, boolean terminal) {
        assert task != null;
        assert WispCarrier.current() == this;
        assert task.carrier == this;
        assert task != current;

        schedTick++;

        if (!task.isAlive()) {
            unregisterEvent(task);
            return false;
        }

        WispTask from = current;
        current = task;
        counter.incrementSwitchCount();
        switchTimestamp = WispEngine.getNanoTime();
        assert !isInCritical;
        boolean res = WispTask.switchTo(from, task, terminal);
        assert res : "coroutine switch failure";
        // Since carrier is changed with stealing,
        // we shouldn't directly access carrier's member any more.
        assert WispCarrier.current().current == from;
        assert !from.carrier.isInCritical;
        return res;
    }

    /**
     * Telling to the scheduler that the current carrier is willing to yield
     * its current use of a processor.
     * <p>
     * Called by {@link Thread#yield()}
     */
    void yield() {
        if (!WispConfiguration.WISP_HIGH_PRECISION_TIMER && worker != null) {
            worker.processTimer();
        }
        if (WispEngine.runningAsCoroutine(current.getThreadWrapper())) {
            boolean withControlGroup = current.controlGroup != null;
            boolean quotaExhausted = false;
            int len = getTaskQueueLength();
            if (len == 0 && withControlGroup) {
                current.controlGroup.calcCpuTicks(current);
                quotaExhausted = current.controlGroup.checkCpuLimit(current, true) != 0;
            }
            if (len > 0 || (withControlGroup && quotaExhausted)) {
                assert yieldingTask == null;
                yieldingTask = current;
                // delay it, make sure wakeupTask is called after yield out
                schedule(false);
            }
            current.carrier.checkAndDispatchShutdown();
        } else {
            WispEngine.JLA.yield0();
        }
    }

    private void processYield() {
        assert current.isThreadTask();
        if (yieldingTask != null) {
            wakeupTask(yieldingTask);
            yieldingTask = null;
        }
    }

    // ------------------------------------------------ IO

    /**
     * Modify current {@link WispTask}'s interest channel and event.
     * {@see registerEvent(...)}
     * <p>
     * Used for implementing socket io
     * <pre>
     *     while (!ch.read(buf) == 0) { // 0 indicate IO not ready, not EOF..
     *         registerEvent(ch, OP_READ);
     *         schedule(false);
     *     }
     *     // read is done here
     * <pre/>
     */
    void registerEvent(SelectableChannel ch, int events) throws IOException {
        registerEvent(current, ch, events);
    }


    /**
     * register target {@link WispTask}'s interest channel and event.
     *
     * @param ch     the channel that is related to the current WispTask
     * @param events interest event
     */
    private void registerEvent(WispTask target, SelectableChannel ch, int events) throws IOException {
        if (ch != null && ch.isOpen() && events != 0) {
            WispEventPump.Pool.INSTANCE.registerEvent(target, ch, events);
        }
    }

    /**
     * Clean current task's interest event before an non-IO blocking operation
     * or task exit to prevent unexpected wake up.
     */
    void unregisterEvent() {
        unregisterEvent(current);
    }

    private void unregisterEvent(WispTask target) {
        if (target.ch != null) {
            target.resetRegisterEventTime();
            target.ch = null;
        }
    }

    // ------------------------------------------------ timer support

    /**
     * Add a timer for current {@link WispTask},
     * used for implementing timed IO operation / sleep etc...
     *
     * @param deadlineNano        deadline of the timer
     * @param action      JVM_UNPARK/JDK_UNPARK/RESUME
     */
    void addTimer(long deadlineNano, TimeOut.Action action) {
        WispTask task = current;
        addTimerInternal(deadlineNano, task, action, true);
    }

    private void resumeLater(long deadlineNano, WispTask task) {
        assert task != null;
        addTimerInternal(deadlineNano, task, TimeOut.Action.RESUME, false);
    }

    private void addTimerInternal(long deadlineNano, WispTask task, TimeOut.Action action, boolean refTimeOut) {
        TimeOut timeOut = new TimeOut(task, deadlineNano, action);
        if (refTimeOut) {
            task.timeOut = timeOut;
        }

        if (WispConfiguration.WISP_HIGH_PRECISION_TIMER) {
            if (task.isThreadTask()) {
                scheduleInTimer(timeOut);
            } else {
                /*
                 * timer.schedule may enter park() again
                 * we delegate this operation to thread coroutine
                 * (which always use native park)
                 */
                pendingTimer = timeOut;
            }
        } else {
            engine.scheduler.addTimer(timeOut, thread);
        }
    }

    /**
     * Cancel the timer added by {@link #addTimer(long, TimeOut.Action action)}.
     */
    void cancelTimer() {
        if (current.timeOut != null) {
            current.timeOut.canceled = true;
            if (!WispConfiguration.WISP_HIGH_PRECISION_TIMER) {
                engine.scheduler.cancelTimer(current.timeOut, thread);
            }
            current.timeOut = null;
        }
        pendingTimer = null;
    }

    private void processPendingTimer() {
        assert current.isThreadTask();
        if (WispConfiguration.WISP_HIGH_PRECISION_TIMER && pendingTimer != null) {
            scheduleInTimer(pendingTimer);
            pendingTimer = null;
        }
    }

    private void scheduleInTimer(TimeOut timeOut) {
        boolean isInCritical0 = isInCritical;
        final long timeout = timeOut.deadlineNano - System.nanoTime();
        isInCritical = true;
        if (timeout > 0) {
            WispEngine.timer.schedule(new Runnable() {
                @Override
                public void run() {
                    if (!timeOut.canceled) {
                        timeOut.doAction();
                    }
                }
            }, timeout, TimeUnit.NANOSECONDS);
        } else if (!timeOut.canceled) {
            timeOut.task.jdkUnpark();
        }
        isInCritical = isInCritical0;
    }

    // ----------------------------------------------- status fetch

    /**
     * @return if current carrier is busy
     */
    boolean isRunning() {
        return current != threadTask;
    }

    /**
     * @return queue length. used for mxBean report
     */
    int getTaskQueueLength() {
        if (worker == null) {
            return 0;
        }
        int ql = worker.queueLength;
        // use a local copy to avoid queueLength change to negative.
        return Math.max(ql, 0);
    }

    /**
     * @return running task number, used for mxBean report
     */
    int getRunningTaskCount() {
        return engine.runningTaskCount;
    }

    // -----------------------------------------------  retake

    /**
     * hand off wispEngine for blocking system calls.
     */
    void handOff() {
        engine.scheduler.handOffWorkerThread(thread);
    }

    // ----------------------------------------------- Monitoring

    WispCounter getCounter() {
        return counter;
    }

    void countEnqueueTime(long enqueueTime) {
        if (enqueueTime != 0) {
            counter.incrementTotalEnqueueTime(System.nanoTime() - enqueueTime);
        }
    }

    @Override
    public String toString() {
        return "WispCarrier on " + thread.getName();
    }

    @Override
    public int compareTo(WispCarrier o) {
        return Long.compare(getId(), o.getId());
    }
}
