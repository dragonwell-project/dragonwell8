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

import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;
import sun.nio.ch.Net;

import java.dyn.Coroutine;
import java.dyn.CoroutineExitException;
import java.dyn.CoroutineSupport;
import java.io.IOException;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;
import java.util.function.Supplier;

/**
 * Coroutine Runtime Engine. It's a "wisp" thing, as we want our asynchronization transformation to be transparent
 * without any modification to user code.
 * <p>
 * WispEngine represents a group of {@link WispCarrier}, which can steal
 * tasks from each other to achieve work-stealing.
 * <p>
 * {@code WispEngine#WISP_ROOT_ENGINE} is created by system.
 * {@link WispEngine#current().execute(Runnable)} in non-worker thread and WISP_ROOT_ENGINE's
 * worker thread will dispatch task in this carrier.
 * <p>
 * User code could also create {@link WispEngine} by calling
 * {@link WispEngine#createEngine(int, ThreadFactory)},
 * Calling {@link WispEngine#execute(Runnable)} will dispatch
 * WispTask inner created carrier.
 * {@link WispEngine#current().execute(Runnable)} in a user created carrier will also
 * dispatch task in current carrier.
 */
public class WispEngine extends AbstractExecutorService {

    static {
        registerNatives();
        setWispEngineAccess();
        timer = createTimerScheduler();
    }

    public static boolean transparentWispSwitch() {
        return WispConfiguration.TRANSPARENT_WISP_SWITCH;
    }

    public static boolean enableThreadAsWisp() {
        return shiftThreadModel;
    }

    @Deprecated
    public static boolean isTransparentAsync() {
        return transparentWispSwitch();
    }

    private static final String WISP_ROOT_ENGINE_NAME = "Root";
    private static final AtomicReferenceFieldUpdater<WispEngine, Boolean> SHUTDOWN_UPDATER
            = AtomicReferenceFieldUpdater.newUpdater(WispEngine.class, Boolean.class, "hasBeenShutdown");
    /*
         some of our users change this field by reflection
         in the runtime to disable wisp temporarily.
         We should move shiftThreadModel to WispConfiguration
         after we provide api to control this behavior and
         notify the users to modify their code.
         TODO refactor to com.alibaba.wisp.enableThreadAsWisp later
         */
    static boolean shiftThreadModel = WispConfiguration.ENABLE_THREAD_AS_WISP;
    static final JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();
    /*
     * Wisp specified Thread Group
     * all the daemon threads in wisp should be created with the Thread Group.
     * In Thread.start(), if the thread should not convert to WispTask,
     * check whether the thread's carrier is daemonThreadGroup
     */
    final static ThreadGroup DAEMON_THREAD_GROUP =
            new ThreadGroup(JLA.currentThread0().getThreadGroup(), "Daemon Thread Group");
    static ScheduledExecutorService timer;
    static Set<Thread> carrierThreads;
    static Thread unparkDispatcher;

    static WispEngine WISP_ROOT_ENGINE;

    private static ScheduledExecutorService createTimerScheduler() {
        return !WispConfiguration.WISP_HIGH_PRECISION_TIMER ? null :
                Executors.newScheduledThreadPool(1, new ThreadFactory() {
                    @Override
                    public Thread newThread(Runnable r) {
                        Thread thread = new Thread(WispEngine.DAEMON_THREAD_GROUP, r);
                        thread.setDaemon(true);
                        thread.setName("Wisp-Timer");
                        return thread;
                    }
                });
    }

    private static void initializeWispClass() {
        assert JLA != null : "WispCarrier should be initialized after System";
        assert JLA.currentThread0().getName().equals("main") : "Wisp need to be loaded by main thread";
        shiftThreadModel = WispConfiguration.ENABLE_THREAD_AS_WISP;
        carrierThreads = new ConcurrentSkipListSet<>(new Comparator<Thread>() {
            @Override
            public int compare(Thread o1, Thread o2) {
                return Long.compare(o1.getId(), o2.getId());
            }
        });
        WISP_ROOT_ENGINE = new WispEngine(WISP_ROOT_ENGINE_NAME);
        if (transparentWispSwitch()) {
            WispEngine.initializeClasses();
            JLA.wispBooted();
        }
    }

    private static void initializeClasses() {
        try {
            Class.forName(CoroutineExitException.class.getName());
            Class.forName(WispThreadWrapper.class.getName());
            Class.forName(TaskDispatcher.class.getName());
            Class.forName(StartShutdown.class.getName());
            Class.forName(Coroutine.StealResult.class.getName());
            Class.forName(WispCounterMXBeanImpl.class.getName());
            Class.forName(ThreadAsWisp.class.getName());
            Class.forName(WispEventPump.class.getName());
            Class.forName(ShutdownEngine.class.getName());
            Class.forName(AbstractShutdownTask.class.getName());
            Class.forName(ShutdownControlGroup.class.getName());
            if (WispConfiguration.WISP_PROFILE) {
                Class.forName(WispPerfCounterMonitor.class.getName());
            }
            if (WispConfiguration.WISP_HIGH_PRECISION_TIMER) {
                timer.submit(new Runnable() {
                    @Override
                    public void run() {
                    }
                });
            }
            new ConcurrentLinkedQueue<>().iterator();
            new ConcurrentSkipListMap<>().keySet().iterator();
            WispCarrier carrier = WispCarrier.current();
            carrier.addTimer(System.nanoTime() + Integer.MAX_VALUE, TimeOut.Action.JDK_UNPARK);
            carrier.cancelTimer();
            carrier.createResumeEntry(new WispTask(carrier, null, false, false));
            // preload classes used by by constraintInResourceContainer method.
            WispTask.wrapRunOutsideWisp(null);
            registerPerfCounter(carrier);
            deRegisterPerfCounter(carrier);
        } catch (Exception e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private static void startWispDaemons() {
        if (transparentWispSwitch()) {
            unparkDispatcher = new Thread(DAEMON_THREAD_GROUP, new Runnable() {
                @Override
                public void run() {
                    int[] proxyUnparks = new int[12];
                    CoroutineSupport.setWispBooted();
                    while (true) {
                        int n = WispEngine.getProxyUnpark(proxyUnparks);
                        for (int i = 0; i < n; i++) {
                            WispTask.unparkById(proxyUnparks[i]);
                        }
                    }
                }
            }, "Wisp-Unpark-Dispatcher");
            unparkDispatcher.setDaemon(true);
            unparkDispatcher.start();
            WispSysmon.INSTANCE.startDaemon();
            WISP_ROOT_ENGINE.scheduler.startWorkerThreads();

            if (!WispConfiguration.CARRIER_AS_POLLER) {
                WispEventPump.Pool.INSTANCE.startPollerThreads();
            }
            if (WispConfiguration.WISP_PROFILE_LOG_ENABLED) {
                WispPerfCounterMonitor.INSTANCE.startDaemon();
            }
        }
    }

    private static void setWispEngineAccess() {
        SharedSecrets.setWispEngineAccess(new WispEngineAccess() {

            @Override
            public WispTask getCurrentTask() {
                return WispCarrier.current().getCurrentTask();
            }

            @Override
            public void registerEvent(SelectableChannel ch, int events) throws IOException {
                WispCarrier.current().registerEvent(ch, events);
            }

            @Override
            public void unregisterEvent() {
                WispCarrier.current().unregisterEvent();
            }

            @Override
            public int epollWait(int epfd, long pollArray, int arraySize, long timeout,
                                 AtomicReference<Object> status, Object INTERRUPTED) throws IOException {
                return WispEventPump.Pool.INSTANCE.epollWaitForWisp(epfd, pollArray, arraySize, timeout, status, INTERRUPTED);
            }

            @Override
            public void interruptEpoll(AtomicReference<Object> status, Object INTERRUPTED, int interruptFd) {
                WispEventPump.Pool.INSTANCE.interruptEpoll(status, INTERRUPTED, interruptFd);
            }

            @Override
            public void addTimer(long deadlineNano) {
                WispCarrier.current().addTimer(deadlineNano, TimeOut.Action.JDK_UNPARK);
            }

            @Override
            public void cancelTimer() {
                WispCarrier.current().cancelTimer();
            }

            @Override
            public void sleep(long ms) {
                WispTask.sleep(ms);
            }

            @Override
            public void yield() {
                WispCarrier.current().yield();
            }

            @Override
            public boolean isThreadTask(WispTask task) {
                return task.isThreadTask();
            }

            @Override
            public boolean isTimeout() {
                WispTask task = WispCarrier.current().current;
                return task.timeOut != null && task.timeOut.expired();
            }

            @Override
            public void park(long timeoutNano) {
                WispTask.jdkPark(timeoutNano);
            }

            @Override
            public void unpark(WispTask task) {
                if (task != null) {
                    task.jdkUnpark();
                }
            }

            @Override
            public void destroy() {
                WispCarrier.current().destroy();
            }

            @Override
            public boolean hasMoreTasks() {
                return WispCarrier.current().getTaskQueueLength() > 0;
            }

            @Override
            public boolean runningAsCoroutine(Thread t) {
                return WispEngine.runningAsCoroutine(t);
            }

            @Override
            public boolean usingWispEpoll() {
                return runningAsCoroutine(null);
            }

            public boolean isAlive(WispTask task) {
                return task.isAlive();
            }

            @Override
            public void interrupt(WispTask task) {
                task.interrupt();
            }

            @Override
            public boolean testInterruptedAndClear(WispTask task, boolean clear) {
                return task.testInterruptedAndClear(clear);
            }

            @Override
            public boolean tryStartThreadAsWisp(Thread thread, Runnable target) {
                return ThreadAsWisp.tryStart(thread, target);
            }

            @Override
            public boolean isAllThreadAsWisp() {
                return WispConfiguration.ALL_THREAD_AS_WISP;
            }

            @Override
            public boolean useDirectSelectorWakeup() {
                return WispConfiguration.USE_DIRECT_SELECTOR_WAKEUP;
            }

            @Override
            public boolean enableSocketLock() {
                return WispConfiguration.WISP_ENABLE_SOCKET_LOCK;
            }

            @Override
            public StackTraceElement[] getStackTrace(WispTask task) {
                return task.getStackTrace();
            }

            @Override
            public void getCpuTime(long[] ids, long[] times) {
                WispTask currentTask = WispCarrier.current().getCurrentTask();
                assert currentTask.getThreadWrapper() != null;
                for (int i = 0; i < ids.length; i++) {
                    if (ids[i] == currentTask.getThreadWrapper().getId()) {
                        times[i] = currentTask.getCpuTime();
                        continue;
                    }
                    for (WispTask wispTask : WispTask.id2Task.values()) {
                        Thread wispThreadWrapper = wispTask.getThreadWrapper();
                        if (wispThreadWrapper != null && wispThreadWrapper.getId() == ids[i]) {
                            times[i] = wispTask.getCpuTime();
                        }
                    }
                }
            }

            @Override
            public int poll(SelectableChannel channel, int interestOps, long millsTimeOut) throws IOException {
                assert interestOps == Net.POLLIN || interestOps == Net.POLLCONN || interestOps == Net.POLLOUT;
                WispTask task = WispCarrier.current().getCurrentTask();
                if (millsTimeOut > 0) {
                    task.carrier.addTimer(System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(millsTimeOut),
                            TimeOut.Action.JDK_UNPARK);
                }
                try {
                    task.carrier.registerEvent(channel, translateToSelectionKey(interestOps));
                    park(-1);
                    return millsTimeOut > 0 && task.timeOut.expired() ? 0 : 1;
                } finally {
                    if (millsTimeOut > 0) {
                        task.carrier.cancelTimer();
                    }
                    unregisterEvent();
                }
            }

            private int translateToSelectionKey(int event) {
                if (Net.POLLIN == event) {
                    return SelectionKey.OP_READ;
                } else if (Net.POLLCONN == event) {
                    return SelectionKey.OP_CONNECT;
                } else if (Net.POLLOUT == event) {
                    return SelectionKey.OP_WRITE;
                }
                return 0;
            }
        });
    }

    final WispScheduler scheduler;
    final Set<WispCarrier> carrierEngines;
    final Queue<WispTask> groupTaskCache = new ConcurrentLinkedQueue<>();
    final CyclicBarrier shutdownBarrier;
    volatile int runningTaskCount = 0;
    private CountDownLatch shutdownFuture;
    volatile Boolean hasBeenShutdown = false;
    volatile boolean terminated;

    /**
     * Create a new WispEngine for executing tasks.
     *
     * @param size worker thread counter
     * @param tf   ThreadFactory used to create worker thread
     */
    public static WispEngine createEngine(int size, ThreadFactory tf) {
        return new WispEngine(size, tf);
    }

    /**
     * Create Root Worker.
     */
    private WispEngine(String name) {
        carrierEngines = new ConcurrentSkipListSet<>();
        // detached carrier won't shut down
        shutdownBarrier = null;
        scheduler = new WispScheduler(
                WispConfiguration.WORKER_COUNT,
                WispConfiguration.WISP_SCHEDULE_STEAL_RETRY,
                WispConfiguration.WISP_SCHEDULE_PUSH_RETRY,
                WispConfiguration.WISP_SCHEDULE_HELP_STEAL_RETRY,
                new ThreadFactory() {
                    final AtomicInteger seq = new AtomicInteger();

                    @Override
                    public Thread newThread(Runnable r) {
                        Thread t = new Thread(r, "Wisp-" + name + "-Worker-" + seq.getAndIncrement());
                        t.setDaemon(true);
                        return t;
                    }
                }, this, true);
    }

    private WispEngine(int size, ThreadFactory tf) {
        carrierEngines = new ConcurrentSkipListSet<>();
        shutdownBarrier = new CyclicBarrier(size);
        scheduler = new WispScheduler(size, tf, this);
        shutdownFuture = new CountDownLatch(1);
    }


    public static WispEngine current() {
        return WispCarrier.current().engine;
    }


    /**
     * Create WispTask to run task code
     * <p>
     * The real running thread depends on implementation
     *
     * @param target target code
     */
    public static void dispatch(Runnable target) {
        WispEngine.current().execute(target);
    }

    @Deprecated
    public static boolean isShiftThreadModel() {
        return shiftThreadModel;
    }

    static boolean isEngineThread(Thread t) {
        assert DAEMON_THREAD_GROUP != null;
        return DAEMON_THREAD_GROUP == t.getThreadGroup() || carrierThreads.contains(t);
    }

    static long getNanoTime() {
        return WispConfiguration.WISP_PROFILE ? System.nanoTime() : 0;
    }

    /**
     * DO NOT use this helper inside WispCarrier,
     * because lambda may cause class loading.
     */
    static <T> T runInCritical(Supplier<T> supplier) {
        WispCarrier carrier = WispCarrier.current();
        boolean critical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            return supplier.get();
        } finally {
            carrier.isInCritical = critical0;
        }
    }

    static boolean runningAsCoroutine(Thread t) {
        WispTask task = t == null ? WispCarrier.current().getCurrentTask() : JLA.getWispTask(t);
        assert task != null;
        // Only carrierThread could create WispTask, and
        // the carrierThread will listen on WispTask's wakeup.
        // So we can safely letting the non-worker wispTask block the whore Thread.
        return !task.isThreadTask();
    }

    static WispCounter getWispCounter(long id) {
        return WispConfiguration.WISP_PROFILE ? WispPerfCounterMonitor.INSTANCE.getWispCounter(id) : null;
    }


    // -----------------------------------------------  shutdown support
    @Override
    public void shutdown() {
        if (!SHUTDOWN_UPDATER.compareAndSet(this, false, true)) {
            return;
        }
        for (WispCarrier carrier : carrierEngines) {
            deRegisterPerfCounter(carrier);
        }
        scheduler.execute(new StartShutdown(null));
    }

    void shutdown(WispControlGroup group) {
        scheduler.execute(new StartShutdown(group));
    }

    @Override
    public List<Runnable> shutdownNow() {
        throw new UnsupportedOperationException();
    }
    class StartShutdown implements StealAwareRunnable {
        private final WispControlGroup wispControlGroup;

        /**
         * @param wispControlGroup which WispControlGroup to shutdown,
         *                         null indicates the whole engine.
         */
        StartShutdown(WispControlGroup wispControlGroup) {
            this.wispControlGroup = wispControlGroup;
        }

        @Override
        public void run() {
            WispCarrier.current().runTaskInternal(
                    wispControlGroup == null ? new ShutdownEngine() : new ShutdownControlGroup(wispControlGroup),
                    WispTask.SHUTDOWN_TASK_NAME, null, null);
        }
    }

    abstract class AbstractShutdownTask implements StealAwareRunnable {
        @Override
        public void run() {
            List<WispTask> tasks;
            do {
                tasks = getTasksForShutdown();
                for (WispTask task : tasks) {
                    if (task.isAlive()) {
                        task.jdkUnpark();
                        task.unpark();
                    }
                }
                // wait tasks to exit on fixed frequency instead of polling
                WispTask.jdkPark(TimeUnit.MILLISECONDS.toNanos(1));
            } while (!tasks.isEmpty());
            finishShutdown();
        }

        abstract List<WispTask> getTasksForShutdown();

        abstract void finishShutdown();
    }

    class ShutdownEngine extends AbstractShutdownTask {
        @Override
        List<WispTask> getTasksForShutdown() {
            return getRunningTasks(null);
        }

        @Override
        void finishShutdown() {
            assert WispTask.SHUTDOWN_TASK_NAME.equals(WispCarrier.current().current.getName());
            terminated = true;
            //notify all worker to exit
            for (WispCarrier carrier : carrierEngines) {
                carrier.worker.signal();
            }
            shutdownFuture.countDown();
        }

    }

    class ShutdownControlGroup extends AbstractShutdownTask {
        private final WispControlGroup wispControlGroup;

        ShutdownControlGroup(WispControlGroup wispControlGroup) {
            this.wispControlGroup = wispControlGroup;
        }
        @Override
        List<WispTask> getTasksForShutdown() {
            return getRunningTasks(wispControlGroup);
        }

        @Override
        void finishShutdown() {
            wispControlGroup.destroyLatch.countDown();
        }
    }


    /**
     * 1. In Wisp2, each WispCarrier's runningTask is modified when WispTask is stolen, we can't guarantee
     * the accuracy of the task set.
     * 2. this function is only called in shutdown, so it's not performance sensitive
     * 3. this function should only be called by current WispTask
     */
    private List<WispTask> getRunningTasks(WispControlGroup group) {
        assert WispTask.SHUTDOWN_TASK_NAME.equals(WispCarrier.current().current.getName());
        WispCarrier carrier = WispCarrier.current();
        ArrayList<WispTask> runningTasks = new ArrayList<>();
        boolean isInCritical0 = carrier.isInCritical;
        carrier.isInCritical = true;
        try {
            for (WispTask task : WispTask.id2Task.values()) {
                if (task.isAlive()
                        && task.carrier.engine == WispEngine.this
                        && !task.isThreadTask()
                        && !task.getName().equals(WispTask.SHUTDOWN_TASK_NAME)
                        && (group == null || task.inDestoryedGroup())) {
                    runningTasks.add(task);
                }
            }
            return runningTasks;
        } finally {
            carrier.isInCritical = isInCritical0;
        }
    }

    @Override
    public boolean isShutdown() {
        return hasBeenShutdown;
    }

    @Override
    public boolean isTerminated() {
        return terminated;
    }

    @Override
    public boolean awaitTermination(long timeout, TimeUnit unit) throws InterruptedException {
        return shutdownFuture.await(timeout, unit);
    }

    @Override
    public void execute(Runnable command) {
        startAsThread(command, "execute task", null);
    }

    public List<Long> getWispCarrierIds() {
        List<Long> carriers = new ArrayList<>();
        for (WispCarrier carrier : carrierEngines) {
            carriers.add(carrier.getId());
        }
        return carriers;
    }

    static void registerPerfCounter(WispCarrier carrier) {
        WispEngine.runInCritical(() -> {
            if (WispConfiguration.WISP_PROFILE) {
                WispPerfCounterMonitor.INSTANCE.register(carrier.counter);
            }
            WispCounterMXBeanImpl.register(carrier.counter);
            return null;
        });
    }

    static void deRegisterPerfCounter(WispCarrier carrier) {
        WispEngine.runInCritical(() -> {
            if (WispConfiguration.WISP_PROFILE) {
                WispPerfCounterMonitor.INSTANCE.deRegister(carrier.counter);
            }
            WispCounterMXBeanImpl.deRegister(carrier.counter);
            return null;
        });
    }

    void startAsThread(Runnable target, String name, Thread thread) {
        scheduler.execute(new TaskDispatcher(WispCarrier.current().current.ctxClassLoader,
                target, name, thread));
    }

    private static native void registerNatives();

    private static native int getProxyUnpark(int[] res);
}
