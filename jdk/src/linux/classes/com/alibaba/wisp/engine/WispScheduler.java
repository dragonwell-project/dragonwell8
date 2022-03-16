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
import sun.misc.UnsafeAccess;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicIntegerFieldUpdater;

/**
 * Wisp work-stealing scheduler.
 * Every worker thread has a {@link ConcurrentLinkedQueue} to
 * receive tasks.
 * Once local queue is empty, worker will scan
 * others' queue, execute stolen task, then check local queue.
 * Again and again, until all the queue is empty.
 */
class WispScheduler {

    private static final SchedulingPolicy SCHEDULING_POLICY = WispConfiguration.SCHEDULING_POLICY;

    private final static int IDX_MASK = 0xffff; // ensure positive
    private final static int STEAL_HIGH_WATER_LEVEL = 4;

    // instance const
    private int PARALLEL;
    private int STEAL_RETRY;
    private int PUSH_RETRY;
    private int HELP_STEAL_RETRY;
    private final boolean IS_ROOT_CARRIER;

    // workers could be changed by handOff(),
    // add volatile to avoiding workers's elements
    // to be allocated in register.
    // we could not add volatile volatile modifier
    // to array elements, so make the array volatile.
    private volatile Worker[] workers;
    private final ThreadFactory threadFactory;
    private final WispEngine engine;
    private int sharedSeed = randomSeed();

    WispScheduler(int parallelism, ThreadFactory threadFactory, WispEngine group) {
        this(parallelism, Math.max(1, parallelism / 2), parallelism,
                Math.max(1, parallelism / 4), threadFactory, group, false);
    }

    WispScheduler(int parallelism, int stealRetry, int pushRetry, int helpStealRetry,
                  ThreadFactory threadFactory, WispEngine engine, boolean isRootCarrier) {
        assert parallelism > 0;
        PARALLEL = parallelism;
        STEAL_RETRY = stealRetry;
        PUSH_RETRY = pushRetry;
        HELP_STEAL_RETRY = helpStealRetry;
        IS_ROOT_CARRIER = isRootCarrier;
        this.engine = engine;
        this.threadFactory = threadFactory;
        workers = new Worker[PARALLEL];
        for (int i = parallelism - 1; i >= 0; i--) {
            workers[i] = new Worker(i);
            workers[i].next = i == PARALLEL - 1 ? null : workers[i + 1];
        }
        workers[PARALLEL - 1].next = workers[0];
        if (!isRootCarrier) {
            // root worker threads are started in startWispDaemons()
            startWorkerThreads();
        }
    }

    void startWorkerThreads() {
        for (Worker worker : workers) {
            worker.thread.start();
        }
    }

    private int generateRandom() {
        return sharedSeed = nextRandom(sharedSeed);
    }

    class Worker implements Runnable {
        ConcurrentLinkedQueue<StealAwareRunnable> taskQueue;
        private final TimeOut.TimerManager timerManager;
        private final Thread thread;
        private final WispEventPump pump; // ONE pump verse N worker relationship
        volatile boolean hasBeenHandoff = false;
        private Worker next;

        private final static int QL_PROCESSING_TIMER = -1; // idle than ql=0, but not really idle
        private final static int QL_POLLING = -1000002;
        private final static int QL_IDLE = -2000002;

        volatile int queueLength;

        Worker(int index) {
            thread = threadFactory.newThread(this);
            WispEngine.carrierThreads.add(thread);
            taskQueue = new ConcurrentLinkedQueue<>();
            timerManager = new TimeOut.TimerManager();
            queueLength = 0;
            pump = WispConfiguration.CARRIER_AS_POLLER && IS_ROOT_CARRIER ?
                    WispEventPump.Pool.INSTANCE.getPump(index) : null;
        }

        void processTimer() {
            timerManager.processTimeoutEventsAndGetWaitDeadline(System.nanoTime());
        }

        @Override
        public void run() {
            try {
                WispCarrier carrier = WispCarrier.current();
                carrier.engine = WispScheduler.this.engine;
                carrier.worker = this;
                WispScheduler.this.engine.carrierEngines.add(carrier);
                WispEngine.registerPerfCounter(carrier);
                WispSysmon.INSTANCE.register(carrier);
                runCarrier(carrier);
            } finally {
                WispEngine.carrierThreads.remove(thread);
                try {
                    engine.shutdownBarrier.await();
                } catch (Exception e) {
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    System.out.println("[Wisp-ERROR] unexpected error "
                            + "on current" + Thread.currentThread() + "has exception"
                            + sw.toString());
                }
            }
        }

        private void runCarrier(final WispCarrier carrier) {
            int r = randomSeed();
            Runnable task;
            while (true) {
                while ((task = pollTask(false)) != null) {
                    doExec(task);
                }

                if (carrier.engine.terminated) {
                    return;
                } else if ((task = SCHEDULING_POLICY.steal(this, r = nextRandom(r))) != null) {
                    doExec(task);
                    continue; // process local queue
                }
                doParkOrPolling();
            }
        }

        private void doParkOrPolling() {
            int st = QL_PROCESSING_TIMER;
            if (queueLength != 0 || !LENGTH_UPDATER.compareAndSet(this, 0, st)) {
                return;
            }
            final long now = System.nanoTime();
            final long deadline = timerManager.processTimeoutEventsAndGetWaitDeadline(now);
            assert deadline != 0;
            if (queueLength == st) {
                int update = pump != null && pump.tryAcquire(this) ? QL_POLLING : QL_IDLE;
                if (LENGTH_UPDATER.compareAndSet(this, st, update)) {
                    st = update;
                    if (taskQueue.peek() == null) {
                        if (st == QL_IDLE) {
                            UA.park0(false, deadline < 0 ? 0 : deadline - now);
                        } else { // st == QL_POLLING
                            doPolling(deadline, now);
                        }
                    }
                }
                if (update == QL_POLLING) {
                    pump.release(this);
                }
            }
            LENGTH_UPDATER.addAndGet(this, -st);
        }

        private void doPolling(final long deadline, long now) {
            while ((deadline < 0 || deadline > now) &&
                    !pump.pollAndDispatchEvents(
                            deadline < 0 ? -1 : TimeOut.nanos2Millis(deadline - now)) &&
                    queueLength == QL_POLLING) { // if wakened by event, we can still waiting..
                now = deadline < 0 ? now : System.nanoTime();
            }
        }

        /**
         * @return if it is idle
         */
        private boolean doSignalIfNecessary(int len) {
            Thread current = JLA.currentThread0();
            if (thread != current) {
                if (len == QL_IDLE) {
                    UA.unpark0(this.thread);
                } else if (len == QL_POLLING) {
                    pump.wakeup();
                }
            }
            return len < 0;
        }

        boolean idleOrPolling() {
            return queueLength == QL_IDLE || queueLength == QL_POLLING;
        }

        boolean isProcessingTimer() {
            return queueLength == QL_PROCESSING_TIMER;
        }

        Runnable pollTask(boolean isSteal) {
            StealAwareRunnable task = taskQueue.poll();
            if (task != null) {
                LENGTH_UPDATER.decrementAndGet(this);
                if (isSteal && !task.isStealEnable()) {
                    // disable steal is a very uncommon case,
                    // The overhead of re-enqueue is acceptable
                    // use pushAndSignal rather than offer,
                    // let the worker execute the task as soon as possible.
                    pushAndSignal(task);
                    return null;
                }
            }
            return task;
        }

        /**
         * @return if it is idle
         */
        boolean pushAndSignal(StealAwareRunnable task) {
            taskQueue.offer(task);
            return doSignalIfNecessary(LENGTH_UPDATER.getAndIncrement(this));
        }

        void signal() {
            doSignalIfNecessary(queueLength);
        }

        void copyContextFromDetachedCarrier(Worker detachedWorker) {
            // copy timers
            timerManager.copyTimer(detachedWorker.timerManager.queue);
            // drain wispTasks
            StealAwareRunnable task;
            while ((task = detachedWorker.taskQueue.poll()) != null) {
                pushAndSignal(task);
            }
        }

        WispScheduler theScheduler() {
            return WispScheduler.this;
        }
    }

    /**
     * try steal one task from the most busy worker's queue
     *
     * @param r random seed
     */
    private Runnable trySteal(int r) {
        Worker busyWorker = null;
        for (int i = 0; i < STEAL_RETRY; i++) {
            final Worker w = getWorker(r + i);
            int ql = w.queueLength;
            if (ql >= STEAL_HIGH_WATER_LEVEL) {
                Runnable task = w.pollTask(true);
                if (task != null) {
                    return task;
                }
            }
            if (busyWorker == null || ql > busyWorker.queueLength) {
                busyWorker = w;
            }
        }
        // If busyCarrier's head is disable steal, we also miss the
        // chance of steal from second busy worker..
        // For disable steal is uncommon path, current implementation is good enough..
        return busyWorker == null ? null : busyWorker.pollTask(true);
    }

    /**
     * Find an idle worker, and push task to it's work queue
     *
     * @param n       retry times
     * @param command the task
     * @param force   push even all workers are busy
     * @return success push to a idle worker
     */
    private boolean tryPush(int n, StealAwareRunnable command, boolean force) {
        assert n > 0;
        int r = generateRandom();
        Worker idleWorker = null;
        int idleQl = Integer.MAX_VALUE;

        Worker w = getWorker(r);
        for (int i = 0; i < n; i++, w = w.next) {
            if (w.idleOrPolling()) {
                if (command != null) {
                    w.pushAndSignal(command);
                } else {
                    w.signal();
                }
                return true;
            }
            int ql = w.queueLength;
            if (ql < idleQl) {
                idleWorker = w;
                idleQl = ql;
            }
        }
        if (force) {
            assert idleWorker != null && command != null;
            idleWorker.pushAndSignal(command);
            return true;
        }
        return false;
    }

    private Worker getWorker(int r) {
        return workers[(r & IDX_MASK) % PARALLEL];
    }

    /**
     * cast thread to worker
     *
     * @param detachedAsNull treat detached worker as not worker thread if detachedAsNull is true.
     * @return null means thread is not considered as a worker
     */
    private Worker castToWorker(Thread thread, boolean detachedAsNull) {
        if (thread == null) {
            return null;
        }
        Worker worker = JLA.getWispTask(thread).carrier.worker;
        if (worker == null || worker.theScheduler() != this) {
            return null;
        } else {
            return detachedAsNull && worker.hasBeenHandoff ? null : worker;
        }
    }

    void addTimer(TimeOut timeOut, Thread current) {
        Worker worker = castToWorker(current, true);
        if (worker != null) {
            worker.timerManager.addTimer(timeOut);
        } else {
            tryPush(1, new StealAwareRunnable() {
                @Override
                public void run() {
                    //Adding timer to detached worker is ok since we rely on
                    //interrupt to wakeup all wispTasks in shutdown
                    Worker worker = castToWorker(JLA.currentThread0(), false);
                    assert worker != null;
                    worker.timerManager.addTimer(timeOut);
                }
            }, true);
        }
    }

    void cancelTimer(TimeOut timeOut, Thread current) {
        Worker worker = castToWorker(current, true);
        if (worker != null) {
            worker.timerManager.cancelTimer(timeOut);
        }
    }

    /**
     * Run the command on the specified thread.
     * Used to implement Thread affinity for scheduler.
     * When execute with detached worker thread, we try to execute this task by
     * other workers, if this step failed command would be marked as can't be stolen,
     * then we push this command to detached worker.
     *
     * @param command the code
     * @param thread  target thread
     */
    void executeWithWorkerThread(StealAwareRunnable command, Thread thread) {
        final Worker worker = castToWorker(thread, false);
        boolean stealEnable = command.isStealEnable();
        if (worker == null || worker.hasBeenHandoff && stealEnable) {
            // detached worker try to execute from global scheduler at first
            execute(command);
        } else {
            SCHEDULING_POLICY.enqueue(worker, stealEnable, command);
        }
    }

    enum SchedulingPolicy {
        PULL {
            // always enqueue to the bounded worker, but workers will steal tasks from each other.
            @Override
            void enqueue(Worker worker, boolean stealEnable, StealAwareRunnable command) {
                WispScheduler scheduler = worker.theScheduler();
                if (!worker.pushAndSignal(command) && stealEnable && scheduler.HELP_STEAL_RETRY > 0) {
                    scheduler.signalIdleWorkerToHelpSteal();
                }
            }

            @Override
            Runnable steal(Worker worker, int r) {
                return worker.theScheduler().trySteal(r);
            }
        },
        PUSH {
            // never try to pull other worker's queues, but choose an idle worker when we're enqueueing
            @Override
            void enqueue(Worker worker, boolean stealEnable, StealAwareRunnable command) {
                WispScheduler scheduler = worker.theScheduler();
                if (stealEnable
                        && !(worker.idleOrPolling() || worker.isProcessingTimer())
                        && scheduler.STEAL_RETRY > 0
                        && scheduler.tryPush(scheduler.STEAL_RETRY, command, false)) {
                    return;
                }
                worker.pushAndSignal(command);
            }

            @Override
            Runnable steal(Worker worker, int r) {
                return null;
            }
        };

        abstract void enqueue(Worker worker, boolean stealEnable, StealAwareRunnable command);

        abstract Runnable steal(Worker worker, int r);
    }

    private void signalIdleWorkerToHelpSteal() {
        tryPush(HELP_STEAL_RETRY, null, false);
    }

    /**
     * Executes the given command at some time in the future.
     *
     * @param command the runnable task
     * @throws NullPointerException if command is null
     */
    public void execute(StealAwareRunnable command) {
        tryPush(PUSH_RETRY, command, true);
    }

    /**
     * Detach worker and create a new worker to replace it.
     * This function should only be called by Wisp-Sysmon
     */
    void handOffWorkerThread(Thread thread) {
        assert WispSysmon.WISP_SYSMON_NAME.equals(Thread.currentThread().getName());
        Worker worker = castToWorker(thread, true);
        if (worker != null && !worker.hasBeenHandoff) {
            worker.hasBeenHandoff = true;
            worker.pushAndSignal(new StealAwareRunnable() {
                @Override
                public void run() {
                }
            }); // ensure `detached` visibility
            worker.thread.setName(worker.thread.getName() + " (HandOff)");
            Worker[] cs = Arrays.copyOf(this.workers, workers.length);
            Worker last = cs[PARALLEL - 1];
            for (int i = 0; i < PARALLEL; i++) {
                if (cs[i] == worker) {
                    cs[i] = new Worker(i);
                    // tasks blocked on detached worker may not be scheduled in time
                    // because it's in long-time syscall, so we try our best to delegate
                    // all context to the new worker
                    cs[i].copyContextFromDetachedCarrier(worker);
                    cs[i].next = worker.next;
                    last.next = cs[i];
                    cs[i].thread.start();
                    break;
                }
                last = cs[i];
            }
            workers = cs;
            WispEngine.deRegisterPerfCounter(JLA.getWispTask(thread).carrier);
        }
    }

    /**
     * Check if current processor number exceeds workers.length, if so we add new workers
     * to this scheduler.
     * This function should only be called by Wisp-Sysmon
     */
    void checkAndGrowWorkers(int availableProcessors) {
        assert WispSysmon.WISP_SYSMON_NAME.equals(Thread.currentThread().getName());
        if (availableProcessors <= workers.length) {
            return;
        }
        double growFactor = (double) availableProcessors / (double) workers.length;
        Worker[] cs = Arrays.copyOf(this.workers, availableProcessors);
        for (int i = availableProcessors - 1; i >= workers.length; i--) {
            if (cs[i] == null) {
                cs[i] = new Worker(i);
                cs[i].next = i == availableProcessors - 1 ? cs[0] : cs[i + 1];
            }
        }
        cs[workers.length - 1].next = cs[workers.length];
        for (int i = workers.length; i < availableProcessors; i++) {
            cs[i].thread.start();
        }
        int originLength = workers.length;
        workers = cs;
        adjustParameters(originLength, growFactor);
    }

    private void adjustParameters(int originLength, double growFactor) {
        PARALLEL = Integer.min((int) Math.round((double) originLength * growFactor),
                workers.length);
        PUSH_RETRY = ((int) (PARALLEL * growFactor));
        STEAL_RETRY = ((int) (STEAL_RETRY * growFactor));
        HELP_STEAL_RETRY = ((int) (HELP_STEAL_RETRY * growFactor));
    }

    private static void doExec(Runnable task) {
        try {
            task.run();
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    private static int nextRandom(int r) {
        r ^= r << 13;
        r ^= r >>> 17;
        return r ^ (r << 5);
    }

    private static int randomSeed() {
        int r = 0;
        while (r == 0) {
            r = (int) System.nanoTime();
        }
        return r;
    }

    private static final AtomicIntegerFieldUpdater<Worker> LENGTH_UPDATER =
            AtomicIntegerFieldUpdater.newUpdater(Worker.class, "queueLength");
    private static final UnsafeAccess UA = SharedSecrets.getUnsafeAccess();
    private static final JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();
}
