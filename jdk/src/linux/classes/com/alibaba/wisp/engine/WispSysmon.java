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

import java.util.*;
import java.util.concurrent.ConcurrentSkipListSet;
import java.util.concurrent.TimeUnit;

enum WispSysmon {
    INSTANCE;

    static {
        registerNatives();
    }

    private final Set<WispCarrier> carriers = new ConcurrentSkipListSet<>();
    final static String WISP_SYSMON_NAME = "Wisp-Sysmon";

    void startDaemon() {
        if (WispConfiguration.ENABLE_HANDOFF) {
            assert WispConfiguration.HANDOFF_POLICY != null;
            Thread thread = new Thread(WispEngine.DAEMON_THREAD_GROUP,
                    WispSysmon.INSTANCE::sysmonLoop, WISP_SYSMON_NAME);
            thread.setDaemon(true);
            thread.start();
        }
    }

    void register(WispCarrier carrier) {
        if (WispConfiguration.ENABLE_HANDOFF) {
            carriers.add(carrier);
        }
    }

    private void sysmonLoop() {
        final long interval = TimeUnit.MICROSECONDS.toNanos(WispConfiguration.SYSMON_TICK_US);
        final long carrierCheckRate = TimeUnit.MICROSECONDS.toNanos(WispConfiguration.SYSMON_CARRIER_GROW_TICK_US);
        final long checkCarrierOnNthTick = carrierCheckRate / interval;
        final boolean checkCarrier = WispConfiguration.CARRIER_GROW && checkCarrierOnNthTick > 0
                // Detach carrier's worker cnt is not specified by configuration
                && WispConfiguration.WORKER_COUNT == Runtime.getRuntime().availableProcessors();
        long nextTick = System.nanoTime() + interval;
        int tick = 0;

        while (true) {
            long timeout = nextTick - System.nanoTime();
            if (timeout > 0) {
                do {
                    UA.park0(false, timeout);
                } while ((timeout = nextTick - System.nanoTime()) > 0);
                handleLongOccupation();
                if (checkCarrier && tick++ == checkCarrierOnNthTick) {
                    WispEngine.WISP_ROOT_ENGINE.scheduler.checkAndGrowWorkers(Runtime.getRuntime().availableProcessors());
                    tick = 0;
                }
            } // else: we're too slow, skip a tick
            nextTick += interval;
        }
    }

    private final List<WispCarrier> longOccupationEngines = new ArrayList<>();

    /**
     * Handle a WispTask occupied a worker thread for long time.
     */
    private void handleLongOccupation() {
        for (WispCarrier carrier : carriers) {
            if (carrier.terminated) {
                // remove in iteration is OK for ConcurrentSkipListSet
                carriers.remove(carrier);
                continue;
            }
            if (carrier.isRunning() && carrier.schedTick == carrier.lastSchedTick) {
                longOccupationEngines.add(carrier);
            }
            carrier.lastSchedTick = carrier.schedTick;
        }

        if (!longOccupationEngines.isEmpty()) {
            Iterator<WispCarrier> itr = longOccupationEngines.iterator();
            while (itr.hasNext()) {
                WispCarrier carrier = itr.next();
                WispConfiguration.HANDOFF_POLICY.handle(carrier, !itr.hasNext());
                itr.remove();
            }
        }
        assert longOccupationEngines.isEmpty();
    }

    enum Policy {
        HAND_OFF { // handOff the worker
            @Override
            void handle(WispCarrier carrier, boolean isLast) {
                if (JLA.isInSameNative(carrier.thread)) {
                    carrier.handOff();
                    INSTANCE.carriers.remove(carrier);
                }
            }
        },
        PREEMPT { // insert a yield() after next safepoint
            @Override
            void handle(WispCarrier carrier, boolean isLast) {
                markPreempted(carrier.thread, isLast);
            }
        },
        ADAPTIVE { // depends on thread status
            @Override
            void handle(WispCarrier carrier, boolean isLast) {
                if (JLA.isInSameNative(carrier.thread)) {
                    carrier.handOff();
                    INSTANCE.carriers.remove(carrier);
                } else {
                    markPreempted(carrier.thread, isLast);
                }
            }
        };

        abstract void handle(WispCarrier carrier, boolean isLast);

    }

    private static native void registerNatives();


    /**
     * Mark the thread as running single wispTask in java too much time.
     * And the Thread.yield() invocation will be emitted after next safepoint.
     *
     * @param thread the thread to mark
     * @param force  fire a force_safepoint immediately
     */
    private static native void markPreempted(Thread thread, boolean force);

    private static final UnsafeAccess UA = SharedSecrets.getUnsafeAccess();
    private static final JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();
}
