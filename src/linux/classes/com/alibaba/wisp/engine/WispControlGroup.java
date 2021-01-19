package com.alibaba.wisp.engine;

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.AbstractExecutorService;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;

/**
 * WispControlGroup is used to limit a group of wisp threads'{@link WispTask}
 * cpu resource consumption. WispControlGroup is similar to cgroup cpu_cfs
 * function.
 */
class WispControlGroup extends AbstractExecutorService {

    // the accuracy of CPU control group based on WispTask depends very much on a
    // WispTask's
    // schedule times in one cfs period, so make sure there are at least 5 times
    // schedule
    // chance in one cfs period.
    private static final int SCHEDULE_TIMES = 5;
    // limit max period duration 100ms.
    private static final int MAX_PERIOD = 100_000;
    // limit min period duration 10ms.
    private static final int MIN_PERIOD = 10_000;
    /**
     * ESTIMATED_PERIOD is an estimated cpu_cfs period according to wisp preemptive
     * schedule period, make sure there are at least SCHEDULE_TIMES wisp schedule
     * happen during cpu_cfs period, which is crucial to cpu_cfs accuracy. And also
     * restrict ESTIMATED_PERIOD in scope [MIN_PERIOD, MAX_PERIOD];
     */
    private static final int ESTIMATED_PERIOD = Math.max(MIN_PERIOD,
            Math.min(MAX_PERIOD, WispConfiguration.SYSMON_TICK_US * SCHEDULE_TIMES));
    private static final AtomicReferenceFieldUpdater<WispControlGroup, Boolean> SHUTDOWN_UPDATER
            = AtomicReferenceFieldUpdater.newUpdater(WispControlGroup.class, Boolean.class, "destroyed");

    private static int defaultCfsPeriod() {
        // prior to adopt configured cfs period.
        int cfsPeriodUs = WispConfiguration.WISP_CONTROL_GROUP_CFS_PERIOD;
        // estimate cpu_cfs quota according to cpu_cfs and giving maxCPUPercent.
        return cfsPeriodUs == 0 ? ESTIMATED_PERIOD : cfsPeriodUs;
    }

    /**
     * @param quota  max cpu time slice in one period{@param period} the
     *               WispControlGroup could consume.
     * @param period cpu time consumption accounting unit.
     * @return {@link WispControlGroup}.
     */
    static WispControlGroup newInstance(int quota, int period) {
        return new WispControlGroup(quota, period);
    }

    static WispControlGroup newInstance(int maxCPUPercent) {
        int cfsPeriodUs = defaultCfsPeriod();
        int cfsQuotaUs = (int) ((double) cfsPeriodUs * maxCPUPercent / 100);
        return new WispControlGroup(cfsQuotaUs, cfsPeriodUs);
    }

    // newInstance() should only be used for creating an ResourceContainer.
    static WispControlGroup newInstance() {
        int cfsPeriodUs = defaultCfsPeriod();
        int cfsQuotaUs = cfsPeriodUs * Runtime.getRuntime().availableProcessors();
        return new WispControlGroup(cfsPeriodUs, cfsQuotaUs);
    }

    private WispControlGroup(int cfsQuotaUs, int cfsPeriodUs) {
        if (cfsQuotaUs <= 0 || cfsPeriodUs <= 0) {
            throw new IllegalArgumentException(
                    "Invalid parameter: cfsQuotaUs or cfsPeriodUs should be positive number");
        }
        this.cpuLimit = new CpuLimit(cfsQuotaUs, cfsPeriodUs);
        this.currentPeriodStart = new AtomicLong();
        this.remainQuota = new AtomicLong();
    }

    private CpuLimit cpuLimit;
    private final AtomicLong currentPeriodStart;
    private final AtomicLong remainQuota;
    volatile Boolean destroyed = false;
    CountDownLatch destroyLatch = new CountDownLatch(1);

    private static class CpuLimit {
        long cfsPeriod;
        long cfsQuota;

        CpuLimit(int cfsQuotaUs, int cfsPeriodUs) {
            cfsQuota = TimeUnit.MICROSECONDS.toNanos(cfsQuotaUs);
            cfsPeriod = TimeUnit.MICROSECONDS.toNanos(cfsPeriodUs);
        }
    }

    /**
     * Need to be called by wispEngine before task run
     *
     * @param updateTs: indicate whether update wisp task's enterTs, it still need
     *                  to update this field despite there is no time slice left
     *                  under some special scenes.
     * @return x == 0: if it's ok to run the task; x > 0, quota exceed, need to
     *         delay x nanoseconds.
     */
    long checkCpuLimit(WispTask task, boolean updateTs) {
        assert task.controlGroup == this;
        assert task.enterTs == 0; // clean by calcCpuTicks
        CpuLimit limit = this.cpuLimit;
        long now = System.nanoTime();
        long cp = currentPeriodStart.get();
        if (now > cp + limit.cfsPeriod && currentPeriodStart.compareAndSet(cp, now)) {
            final long quotaInc = (long) ((now - cp) / (double) limit.cfsPeriod * limit.cfsQuota);
            cp = now;
            long q;
            do {
                q = remainQuota.get();
            } while (!remainQuota.compareAndSet(q, Math.min(q + quotaInc, limit.cfsQuota)));
        }
        long q = remainQuota.get();
        if (q >= 0) {
            task.enterTs = System.nanoTime();
            task.ttr = 0;
            return 0;
        }
        if (updateTs) {
            task.enterTs = System.nanoTime();
        }
        long timeToResume = (long) (-q / (double) limit.cfsQuota * limit.cfsPeriod);
        timeToResume = Math.max(timeToResume, cp + limit.cfsPeriod - now);
        task.ttr = timeToResume / 1000;
        assert timeToResume > 0;
        return timeToResume;
    }

    long calcCpuTicks(WispTask task) {
        assert task.controlGroup == this;
        assert task.enterTs != 0;
        long usage = System.nanoTime() - task.enterTs;
        remainQuota.addAndGet(-usage);
        task.enterTs = 0;
        return usage;
    }

    private void attach() {
        WispTask task = WispCarrier.current().current;
        assert task.controlGroup == null;
        if (task.enterTs != 0) {
            task.totalTs += System.nanoTime() - task.enterTs;
            task.enterTs = 0;
        }
        task.controlGroup = this;
        long delay = checkCpuLimit(task, true);
        if (delay != 0) {
            try {
                WispTask.jdkPark(delay);
            } catch (ThreadDeath threadDeath) {
                assert task.enterTs != 0;
                detach();
                throw threadDeath;
            }
        }
        assert task.enterTs != 0;
    }

    private void detach() {
        WispTask task = WispCarrier.current().current;
        assert task.controlGroup != null;
        task.controlGroup.calcCpuTicks(task);
        task.controlGroup = null;
    }

    Runnable wrap(Runnable command) {
        return () -> {
            attach();
            try {
                // must run the command in runOutsideWisp wrap, otherwise preempt will be
                // prevented by Coroutine::in_critical during command running.
                WispTask.runOutsideWisp(command);
            } finally {
                detach();
            }
        };
    }

    /**
     * execute a Runnable in asynchronous mode as a wrapped
     * WispTask{@link WispTask}, wrap will attach running WispTask to current
     * WispControlGroup and then run the giving command.
     */
    @Override
    public void execute(Runnable command) {
        WispEngine.dispatch(wrap(command));
    }

    @Override
    public void shutdown() {
        if (SHUTDOWN_UPDATER.compareAndSet(this, false, true)) {
            WispEngine.WISP_ROOT_ENGINE.shutdown(this);
        }
    }

    @Override
    public List<Runnable> shutdownNow() {
        throw new UnsupportedOperationException("not implemented");
    }

    @Override
    public boolean isShutdown() {
        return destroyed;
    }

    @Override
    public boolean isTerminated() {
        return destroyLatch.getCount() == 0;
    }

    @Override
    public boolean awaitTermination(long timeout, TimeUnit unit) throws InterruptedException {
        return destroyLatch.await(timeout, unit);
    }

    @Override
    public String toString() {
        CpuLimit limit = this.cpuLimit;
        return "WispControlGroup{" + limit.cfsQuota / 1000 + "/" + limit.cfsPeriod / 1000 + '}';
    }

    ResourceContainer createResourceContainer() {
        return new AbstractResourceContainer() {
            private Constraint constraint;

            @Override
            public State getState() {
                if (isTerminated()) {
                    return State.DEAD;
                } else if (isShutdown()) {
                    return State.STOPPING;
                } else {
                    return State.RUNNING;
                }
            }

            @Override
            public void updateConstraint(Constraint constraint) {
                /* check resource type contained in constraint must be CPU_PERCENT */
                if (constraint.getResourceType() != ResourceType.CPU_PERCENT) {
                    throw new IllegalArgumentException("Resource type is not CPU_PERCENT");
                }
                this.constraint = constraint;
                long[] para = constraint.getValues();
                long cpuPercent = para[0];
                int cfsPeriod = defaultCfsPeriod();
                int cfsQuota = (int) ((double) cfsPeriod * cpuPercent / 100);
                cpuLimit = new CpuLimit(cfsQuota, cfsPeriod);
            }

            @Override
            public Iterable<Constraint> getConstraints() {
                assert constraint != null;
                return Collections.singletonList(constraint);
            }

            @Override
            public void destroy() {
                shutdown();
                while (!isTerminated()) {
                    try {
                        awaitTermination(1, TimeUnit.SECONDS);
                    } catch (InterruptedException e) {
                        throw new InternalError(e);
                    }
                }
            }

            @Override
            protected void attach() {
                super.attach();
                WispControlGroup.this.attach();
            }

            @Override
            protected void detach() {
                WispControlGroup.this.detach();
                super.detach();
            }
        };
    }
}
