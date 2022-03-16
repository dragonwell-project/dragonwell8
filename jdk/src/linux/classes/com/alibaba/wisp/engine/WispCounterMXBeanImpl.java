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

import com.alibaba.management.WispCounterMXBean;

import sun.management.Util;

import javax.management.ObjectName;
import java.util.*;
import java.util.Map.Entry;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;

/**
 * Implementation class for WispCounterMXBean.
 */
public class WispCounterMXBeanImpl implements WispCounterMXBean {

    private final static String WISP_COUNTER_MXBEAN_NAME = "com.alibaba.management:type=WispCounter";

    private final static Map<Long, WispCounter> managedEngineCounters;

    static {
        if (!WispEngine.transparentWispSwitch()) {
            managedEngineCounters = new HashMap<>();
        } else {
            managedEngineCounters = new ConcurrentHashMap<>(100);
        }
    }

    @Override
    public List<Boolean> getRunningStates() {
        return aggregate(WispCounter::getRunningState);
    }

    @Override
    public List<Long> getSwitchCount() {
        return aggregate(WispCounter::getSwitchCount);
    }

    @Override
    public List<Long> getWaitTimeTotal() {
        return aggregate(WispCounter::getWaitTimeTotal);
    }

    @Override
    public List<Long> getRunningTimeTotal() {
        return aggregate(WispCounter::getRunningTimeTotal);
    }

    @Override
    public List<Long> getCompleteTaskCount() {
        return aggregate(WispCounter::getCompletedTaskCount);
    }

    @Override
    public List<Long> getCreateTaskCount() {
        return aggregate(WispCounter::getCreateTaskCount);
    }

    @Override
    public List<Long> getParkCount() {
        return aggregate(WispCounter::getParkCount);
    }

    @Override
    public List<Long> getUnparkCount() {
        return aggregate(WispCounter::getUnparkCount);
    }

    @Override
    public List<Long> getLazyUnparkCount() {
        return aggregate(w -> 0L);
    }

    @Override
    public List<Long> getUnparkInterruptSelectorCount() {
        return aggregate(WispCounter::getUnparkInterruptSelectorCount);
    }

    @Override
    public List<Long> getSelectableIOCount() {
        return aggregate(WispCounter::getSelectableIOCount);
    }

    @Override
    public List<Long> getTimeOutCount() {
        return aggregate(WispCounter::getTimeOutCount);
    }

    @Override
    public List<Long> getEventLoopCount() {
        return aggregate(WispCounter::getEventLoopCount);
    }

    @Override
    public List<Long> getQueueLength() {
        return aggregate(WispCounter::getCurrentTaskQueueLength);
    }

    @Override
    public List<Long> getNumberOfRunningTasks() {
        return aggregate(WispCounter::getCurrentRunningTaskCount);
    }

    @Override
    public List<Long> getTotalEnqueueTime() {
        return aggregate(WispCounter::getTotalEnqueueTime);
    }

    @Override
    public List<Long> getEnqueueCount() {
        return aggregate(WispCounter::getEnqueueCount);
    }

    @Override
    public List<Long> getTotalExecutionTime() {
        return aggregate(WispCounter::getTotalExecutionTime);
    }

    @Override
    public List<Long> getExecutionCount() {
        return aggregate(WispCounter::getExecutionCount);
    }

    @Override
    public List<Long> getTotalWaitSocketIOTime() {
        return aggregate(WispCounter::getTotalWaitSocketIOTime);
    }

    @Override
    public List<Long> getWaitSocketIOCount() {
        return aggregate(WispCounter::getWaitSocketIOCount);
    }

    @Override
    public List<Long> getTotalBlockingTime() {
        return aggregate(WispCounter::getTotalBlockingTime);
    }

    /**
     * @param id WispCarrier id
     * @return WispCounter
     */
    @Override
    public WispCounter getWispCounter(long id) {
        return WispEngine.getWispCounter(id);
    }

    private <T> List<T> aggregate(Function<WispCounter, T> getter) {
        List<T> result = new ArrayList<>(managedEngineCounters.size());
        for (Entry<Long, WispCounter> entry : managedEngineCounters.entrySet()) {
            result.add(getter.apply(entry.getValue()));
        }
        return result;
    }

    @Override
    public ObjectName getObjectName() {
        return Util.newObjectName(WISP_COUNTER_MXBEAN_NAME);
    }

    static void register(WispCounter counter) {
        managedEngineCounters.put(counter.carrier.getId(), counter);
    }

    static void deRegister(WispCounter counter) {
        managedEngineCounters.remove(counter.carrier.getId());
        counter.cleanup();
    }
}
