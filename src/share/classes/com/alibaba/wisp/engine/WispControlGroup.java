package com.alibaba.wisp.engine;

import java.util.List;
import java.util.concurrent.AbstractExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

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

    /**
     * @param quota  max cpu time slice in one period{@param period} the
     *               WispControlGroup could consume.
     * @param period cpu time consumption accounting unit.
     * @return {@link WispControlGroup}.
     */
    static WispControlGroup create(int quota, int period) {
        return new WispControlGroup(quota, period);
    }

    static WispControlGroup create(int maxCPUPercent) {
        // prior to adopt configured cfs period.
        int cfsPeriodUs = WispConfiguration.WISP_CONTROL_GROUP_CFS_PERIOD;
        if (cfsPeriodUs == 0) {
            // estimate cpu_cfs quota according to cpu_cfs and giving maxCPUPercent.
            cfsPeriodUs = ESTIMATED_PERIOD;
        }
        int cfsQuotaUs = (int) ((double) cfsPeriodUs * maxCPUPercent / 100);
        return new WispControlGroup(cfsQuotaUs, cfsPeriodUs);
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
    private AtomicLong currentPeriodStart;
    private AtomicLong remainQuota;

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
        task.controlGroup = this;
        if (task.enterTs != 0) {
            task.totalTs += System.nanoTime() - task.enterTs;
            task.enterTs = 0;
        }
        long delay = checkCpuLimit(task, true);
        if (delay != 0) {
            WispTask.jdkPark(delay);
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
        throw new UnsupportedOperationException("NYI");
    }

    @Override
    public List<Runnable> shutdownNow() {
        return null;
    }

    @Override
    public boolean isShutdown() {
        return false;
    }

    @Override
    public boolean isTerminated() {
        return false;
    }

    @Override
    public boolean awaitTermination(long timeout, TimeUnit unit) throws InterruptedException {
        return false;
    }

    @Override
    public String toString() {
        CpuLimit limit = this.cpuLimit;
        return "WispControlGroup{" + limit.cfsQuota / 1000 + "/" + limit.cfsPeriod / 1000 + '}';
    }
}
