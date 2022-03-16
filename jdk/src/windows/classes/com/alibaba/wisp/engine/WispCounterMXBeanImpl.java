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
import java.util.List;

public class WispCounterMXBeanImpl implements WispCounterMXBean {

    private final static String WISP_COUNTER_MXBEAN_NAME = "com.alibaba.management:type=WispCounter";

    @Override
    public List<Boolean> getRunningStates() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getSwitchCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getWaitTimeTotal() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getRunningTimeTotal() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getCompleteTaskCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getCreateTaskCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getParkCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getUnparkCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getLazyUnparkCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getUnparkInterruptSelectorCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getSelectableIOCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getTimeOutCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getEventLoopCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getQueueLength() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getNumberOfRunningTasks() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getTotalEnqueueTime() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getEnqueueCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getTotalExecutionTime() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getExecutionCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getTotalWaitSocketIOTime() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getWaitSocketIOCount() {
        throw new UnsupportedOperationException();
    }

    @Override
    public List<Long> getTotalBlockingTime() {
        throw new UnsupportedOperationException();
    }

    @Override
    public WispCounter getWispCounter(long id) {
        throw new UnsupportedOperationException();
    }

    @Override
    public ObjectName getObjectName() {
        return Util.newObjectName(WISP_COUNTER_MXBEAN_NAME);
    }
}
