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

import sun.security.action.GetPropertyAction;

import java.io.*;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Properties;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

class WispConfiguration {
    private static final String DELIMITER = ";";

    static final boolean TRANSPARENT_WISP_SWITCH;
    static final boolean ENABLE_THREAD_AS_WISP;
    static final boolean ALL_THREAD_AS_WISP;

    static final int STACK_SIZE;
    static final boolean PARK_ONE_MS_AT_LEAST;
    static final int WORKER_COUNT;
    static final boolean ENABLE_HANDOFF;
    static final WispSysmon.Policy HANDOFF_POLICY;
    static final int SYSMON_TICK_US;
    static final int MIN_PARK_NANOS;
    static final int POLLER_SHARDING_SIZE;

    static final int SYSMON_CARRIER_GROW_TICK_US;
    // monitor
    static final boolean WISP_PROFILE;
    static final boolean WISP_PROFILE_LOG_ENABLED;
    static final int WISP_PROFILE_LOG_INTERVAL_MS;
    static final String WISP_PROFILE_LOG_PATH;

    static final boolean WISP_HIGH_PRECISION_TIMER;
    static final int WISP_ENGINE_TASK_CACHE_SIZE;
    static final int WISP_SCHEDULE_STEAL_RETRY;
    static final int WISP_SCHEDULE_PUSH_RETRY;
    static final int WISP_SCHEDULE_HELP_STEAL_RETRY;
    static final WispScheduler.SchedulingPolicy SCHEDULING_POLICY;
    static final boolean USE_DIRECT_SELECTOR_WAKEUP;
    static final boolean CARRIER_AS_POLLER;
    static final boolean MONOLITHIC_POLL;
    static final boolean CARRIER_GROW;

    // io
    static final boolean WISP_ENABLE_SOCKET_LOCK;

    // wisp control group
    static final int WISP_CONTROL_GROUP_CFS_PERIOD;

    private static List<String> THREAD_AS_WISP_BLACKLIST;

    static {
        Properties p = java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Properties>() {
                    public Properties run() {
                        return System.getProperties();
                    }
                }
        );

        TRANSPARENT_WISP_SWITCH = p.containsKey("com.alibaba.wisp.transparentWispSwitch") ?
                parseBooleanParameter(p, "com.alibaba.wisp.transparentWispSwitch", false) :
                parseBooleanParameter(p, "com.alibaba.transparentAsync", false);
        ENABLE_THREAD_AS_WISP = p.containsKey("com.alibaba.wisp.enableThreadAsWisp") ?
                parseBooleanParameter(p, "com.alibaba.wisp.enableThreadAsWisp", false) :
                parseBooleanParameter(p, "com.alibaba.shiftThreadModel", false);
        ALL_THREAD_AS_WISP = parseBooleanParameter(p, "com.alibaba.wisp.allThreadAsWisp", false);
        STACK_SIZE = parsePositiveIntegerParameter(p, "com.alibaba.wisp.stacksize", 512 * 1024);
        PARK_ONE_MS_AT_LEAST = parseBooleanParameter(p, "com.alibaba.wisp.parkOneMs", true);
        WORKER_COUNT = parsePositiveIntegerParameter(p, "com.alibaba.wisp.carrierEngines",
                Runtime.getRuntime().availableProcessors());
        POLLER_SHARDING_SIZE = parsePositiveIntegerParameter(p, "com.alibaba.pollerShardingSize", 8);
        ENABLE_HANDOFF = parseBooleanParameter(p, "com.alibaba.wisp.enableHandOff",
                TRANSPARENT_WISP_SWITCH);
        // handoff worker thread implementation is not stable enough,
        // use preempt by default, and we'll move to ADAPTIVE in the future
        HANDOFF_POLICY = WispSysmon.Policy.valueOf(
                p.getProperty("com.alibaba.wisp.handoffPolicy", WispSysmon.Policy.PREEMPT.name()));
        SYSMON_TICK_US = parsePositiveIntegerParameter(p, "com.alibaba.wisp.sysmonTickUs",
                (int) TimeUnit.MILLISECONDS.toMicros(100));
        MIN_PARK_NANOS = parsePositiveIntegerParameter(p, "com.alibaba.wisp.minParkNanos", 100);
        WISP_PROFILE_LOG_ENABLED = parseBooleanParameter(p, "com.alibaba.wisp.enableProfileLog", false);
        WISP_PROFILE_LOG_INTERVAL_MS = parsePositiveIntegerParameter(p, "com.alibaba.wisp.logTimeInternalMillis", 15000);
        if (WISP_PROFILE_LOG_ENABLED) {
            WISP_PROFILE = true;
            WISP_PROFILE_LOG_PATH = p.getProperty("com.alibaba.wisp.logPath");
        } else {
            WISP_PROFILE = parseBooleanParameter(p, "com.alibaba.wisp.profile", false);
            WISP_PROFILE_LOG_PATH = "";
        }

        CARRIER_AS_POLLER = parseBooleanParameter(p, "com.alibaba.wisp.useCarrierAsPoller", ALL_THREAD_AS_WISP);
        MONOLITHIC_POLL = parseBooleanParameter(p, "com.alibaba.wisp.monolithicPoll", true);
        WISP_HIGH_PRECISION_TIMER = parseBooleanParameter(p, "com.alibaba.wisp.highPrecisionTimer", false);
        WISP_ENGINE_TASK_CACHE_SIZE = parsePositiveIntegerParameter(p, "com.alibaba.wisp.engineTaskCache", 20);
        WISP_SCHEDULE_STEAL_RETRY = parsePositiveIntegerParameter(p, "com.alibaba.wisp.schedule.stealRetry", Math.max(1, WORKER_COUNT / 2));
        WISP_SCHEDULE_PUSH_RETRY = parsePositiveIntegerParameter(p, "com.alibaba.wisp.schedule.pushRetry", WORKER_COUNT);
        WISP_SCHEDULE_HELP_STEAL_RETRY = parsePositiveIntegerParameter(p, "com.alibaba.wisp.schedule.helpStealRetry", Math.max(1, WORKER_COUNT / 4));
        SCHEDULING_POLICY = WispScheduler.SchedulingPolicy.valueOf(p.getProperty("com.alibaba.wisp.schedule.policy",
                WORKER_COUNT > 16 ? WispScheduler.SchedulingPolicy.PUSH.name() : WispScheduler.SchedulingPolicy.PULL.name()));
        USE_DIRECT_SELECTOR_WAKEUP = parseBooleanParameter(p, "com.alibaba.wisp.directSelectorWakeup", true);
        WISP_ENABLE_SOCKET_LOCK = parseBooleanParameter(p, "com.alibaba.wisp.useSocketLock", true);
        CARRIER_GROW = parseBooleanParameter(p, "com.alibaba.wisp.growCarrier", false);
        SYSMON_CARRIER_GROW_TICK_US = parsePositiveIntegerParameter(p, "com.alibaba.wisp.growCarrierTickUs", (int) TimeUnit.SECONDS.toMicros(5));
        // WISP_CONTROL_GROUP_CFS_PERIOD default value is 0(Us), WispControlGroup will estimate a cfs period according to SYSMON_TICK_US.
        // If WISP_CONTROL_GROUP_CFS_PERIOD was configed by user, WispControlGroup will adopt it directly and won't estimate.
        WISP_CONTROL_GROUP_CFS_PERIOD = parsePositiveIntegerParameter(p, "com.alibaba.wisp.controlGroup.cfsPeriod", 0);
        checkCompatibility();
    }

    private static void checkCompatibility() {
        checkDependency(ENABLE_THREAD_AS_WISP, "-Dcom.alibaba.wisp.enableThreadAsWisp=true",
                TRANSPARENT_WISP_SWITCH, "-Dcom.alibaba.wisp.transparentWispSwitch=true");
        checkDependency(ENABLE_HANDOFF, "-Dcom.alibaba.wisp.enableHandOff=true",
                TRANSPARENT_WISP_SWITCH, "-Dcom.alibaba.wisp.enableThreadAsWisp=true");
        checkDependency(ALL_THREAD_AS_WISP, "-Dcom.alibaba.wisp.allThreadAsWisp=true",
                ENABLE_THREAD_AS_WISP, "-Dcom.alibaba.wisp.enableThreadAsWisp=true");
        checkDependency(CARRIER_AS_POLLER, "-Dcom.alibaba.wisp.useCarrierAsPoller=true",
                ALL_THREAD_AS_WISP, "-Dcom.alibaba.wisp.allThreadAsWisp=true");
        if (ENABLE_THREAD_AS_WISP && !ALL_THREAD_AS_WISP) {
            throw new IllegalArgumentException("shift thread model by stack configuration is no longer supported," +
                    " use -XX:+UseWisp2 instead");
        }
    }

    private static void checkDependency(boolean cond, String condStr, boolean preRequire, String preRequireStr) {
        if (cond && !preRequire) {
            throw new IllegalArgumentException("\"" + condStr + "\" depends on \"" + preRequireStr + "\"");
        }
    }

    private static int parsePositiveIntegerParameter(Properties p, String key, int defaultVal) {
        String value;
        if (p == null || (value = p.getProperty(key)) == null) {
            return defaultVal;
        }
        int res = defaultVal;
        try {
            res = Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return defaultVal;
        }
        return res <= 0 ? defaultVal : res;
    }

    private static boolean parseBooleanParameter(Properties p, String key, boolean defaultVal) {
        String value;
        if (p == null || (value = p.getProperty(key)) == null) {
            return defaultVal;
        }
        return Boolean.parseBoolean(value);
    }

    private static List<String> parseListParameter(Properties p, Properties confProp, String key) {
        String value = p.getProperty(key);
        if (value == null) {
            value = confProp.getProperty(key);
        }
        return value == null ? Collections.emptyList() :
                Arrays.asList(value.trim().split(DELIMITER));
    }

    /**
     * Loading config from system property "com.alibaba.wisp.config" specified
     * file or jre/lib/wisp.properties.
     */
    private static void loadBizConfig() {
        Properties p = java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<Properties>() {
                    public Properties run() {
                        return System.getProperties();
                    }
                }
        );
        String path = p.getProperty("com.alibaba.wisp.config");

        Properties confProp = new Properties();
        if (path != null) {
            File f = new File(path);
            if (f.exists()) {
                try (InputStream is = new BufferedInputStream(new FileInputStream(f.getPath()))) {
                    confProp.load(is);
                } catch (IOException e) {
                    // ignore, all STACK_LIST are empty
                }
            }
        }
        THREAD_AS_WISP_BLACKLIST = parseListParameter(p, confProp, "com.alibaba.wisp.threadAsWisp.black");

    }

    private static final int UNLOADED = 0, LOADING = 1, LOADED = 2;
    private static final AtomicInteger bizLoadStatus = new AtomicInteger(UNLOADED);

    private static void ensureBizConfigLoaded() {
        if (bizLoadStatus.get() == LOADED) {
            return;
        }
        if (bizLoadStatus.get() == UNLOADED && bizLoadStatus.compareAndSet(UNLOADED, LOADING)) {
            try {
                loadBizConfig();
            } finally {
                bizLoadStatus.set(LOADED);
            }
        }
        while (bizLoadStatus.get() != LOADED) {/* wait */}
    }

    static List<String> getThreadAsWispBlacklist() {
        ensureBizConfigLoaded();
        assert THREAD_AS_WISP_BLACKLIST != null;
        return THREAD_AS_WISP_BLACKLIST;
    }
}
