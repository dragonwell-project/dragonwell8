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

import java.beans.ConstructorProperties;

final public class WispCounter {

    private long switchCount = 0;

    private long waitTimeTotal = 0;

    private long runningTimeTotal = 0;

    private long completedTaskCount = 0;

    private long createTaskCount = 0;

    private long parkCount = 0;

    private long unparkCount = 0;

    private long unparkInterruptSelectorCount = 0;

    private long selectableIOCount = 0;

    private long timeOutCount = 0;

    private long eventLoopCount = 0;

    private long totalEnqueueTime = 0;

    private long maxEnqueueTime = 0;

    private long enqueueCount = 0;

    private long totalExecutionTime = 0;

    private long maxExecutionTime = 0;

    private long executionCount = 0;

    private long totalWaitSocketIOTime = 0;

    private long maxWaitSocketIOTime = 0;

    private long waitSocketIOCount = 0;

    private long totalBlockingTime = 0;

    private long maxBlockingTime = 0;

    private long unparkFromJvmCount = 0;

    private long runningTaskCount = 0;

    private long taskQueueLength = 0;

    WispCarrier carrier;

    private WispCounter(WispCarrier carrier) {
        this.carrier = carrier;
    }

    boolean getRunningState() {
        WispCarrier e = carrier;
        return e != null && e.isRunning();
    }

    void incrementSwitchCount() {
        switchCount++;
    }

    long getSwitchCount() {
        return switchCount;
    }

    void incrementCompleteTaskCount() {
        completedTaskCount++;
    }

    void incrementRunningTimeTotal(long value) {
        runningTimeTotal += value;
    }

    long getRunningTimeTotal() {
        return runningTimeTotal;
    }

    void incrementWaitTime(long value) {
        waitTimeTotal += value;
    }

    long getWaitTimeTotal() {
        return waitTimeTotal;
    }

    public long getCompletedTaskCount() {
        return completedTaskCount;
    }

    void incrementCreateTaskCount() {
        createTaskCount++;
    }

    long getCreateTaskCount() {
        return createTaskCount;
    }

    void incrementParkCount() {
        parkCount++;
    }

    long getParkCount() {
        return parkCount;
    }


    void incrementUnparkInterruptSelectorCount() {
        unparkInterruptSelectorCount++;
    }

    long getUnparkInterruptSelectorCount() {
        return unparkInterruptSelectorCount;
    }

    void incrementSelectableIOCount() {
        selectableIOCount++;
    }

    long getSelectableIOCount() {
        return selectableIOCount;
    }

    void incrementTimeOutCount() {
        timeOutCount++;
    }

    long getTimeOutCount() {
        return timeOutCount;
    }

    void incrementEventLoopCount() {
        eventLoopCount++;
    }

    long getEventLoopCount() {
        return eventLoopCount;
    }

    void incrementTotalEnqueueTime(long value) {
        totalEnqueueTime += value;
        enqueueCount++;
        if (value > maxEnqueueTime) {
            maxEnqueueTime = value;
        }
    }

    public long getTotalEnqueueTime() {
        return totalEnqueueTime;
    }

    public long getEnqueueCount() {
        return enqueueCount;
    }

    void incrementTotalExecutionTime(long value) {
        totalExecutionTime += value;
        executionCount++;
        if (value > maxExecutionTime) {
            maxExecutionTime = value;
        }
    }

    public long getTotalExecutionTime() {
        return totalExecutionTime;
    }

    public long getExecutionCount() {
        return executionCount;
    }

    void incrementTotalWaitSocketIOTime(long value) {
        totalWaitSocketIOTime += value;
        waitSocketIOCount++;
        if (value > maxWaitSocketIOTime) {
            maxWaitSocketIOTime = value;
        }
    }

    public long getTotalWaitSocketIOTime() {
        return totalWaitSocketIOTime;
    }

    public long getWaitSocketIOCount() {
        return waitSocketIOCount;
    }

    void incrementTotalBlockingTime(long value) {
        totalBlockingTime += value;
        unparkCount++;
        if (value > maxBlockingTime) {
            maxBlockingTime = value;
        }
    }

    public long getTotalBlockingTime() {
        return totalBlockingTime;
    }

    public long getUnparkCount() {
        return unparkCount;
    }

    long getCurrentTaskQueueLength() {
        WispCarrier e = carrier;
        return e != null ? e.getTaskQueueLength() : 0;
    }

    long getCurrentRunningTaskCount() {
        WispCarrier e = carrier;
        return e != null ? e.getRunningTaskCount() : 0;
    }

    void incrementUnparkFromJvmCount() {
        unparkFromJvmCount++;
    }

    long getUnparkFromJvmCount() {
        return unparkFromJvmCount;
    }

    public long getMaxEnqueueTime() {
        return maxEnqueueTime;
    }

    public long getMaxExecutionTime() {
        return maxExecutionTime;
    }

    public long getMaxWaitSocketIOTime() {
        return maxWaitSocketIOTime;
    }

    public long getMaxBlockingTime() {
        return maxBlockingTime;
    }

    public long getTaskQueueLength() {
        return taskQueueLength;
    }

    public long getRunningTaskCount() {
        return runningTaskCount;
    }

    WispCounter() {
    }

    @ConstructorProperties({"completedTaskCount", "totalEnqueueTime", "maxEnqueueTime", "enqueueCount",
            "totalExecutionTime", "maxExecutionTime", "executionCount",
            "totalWaitSocketIOTime", "maxWaitSocketIOTime", "waitSocketIOCount",
            "totalBlockingTime", "maxBlockingTime", "unparkCount",
            "runningTaskCount", "taskQueueLength"})
    public WispCounter(long completedTaskCount, long totalEnqueueTime, long maxEnqueueTime, long enqueueCount,
                       long totalExecutionTime, long maxExecutionTime, long executionCount,
                       long totalWaitSocketIOTime, long maxWaitSocketIOTime, long waitSocketIOCount,
                       long totalBlockingTime, long maxBlockingTime, long unparkCount,
                       long runningTaskCount, long taskQueueLength) {
        this.completedTaskCount = completedTaskCount;
        this.totalEnqueueTime = totalEnqueueTime;
        this.maxEnqueueTime = maxEnqueueTime;
        this.enqueueCount = enqueueCount;
        this.totalExecutionTime = totalExecutionTime;
        this.maxExecutionTime = maxExecutionTime;
        this.executionCount = executionCount;
        this.totalWaitSocketIOTime = totalWaitSocketIOTime;
        this.maxWaitSocketIOTime = maxWaitSocketIOTime;
        this.waitSocketIOCount = waitSocketIOCount;
        this.totalBlockingTime = totalBlockingTime;
        this.maxBlockingTime = maxBlockingTime;
        this.unparkCount = unparkCount;
        this.runningTaskCount = runningTaskCount;
        this.taskQueueLength = taskQueueLength;
    }


    void assign(WispCounter counter) {
        createTaskCount = counter.createTaskCount;
        completedTaskCount = counter.completedTaskCount;
        totalEnqueueTime = counter.totalEnqueueTime;
        enqueueCount = counter.enqueueCount;
        maxEnqueueTime = counter.maxEnqueueTime;
        totalExecutionTime = counter.totalExecutionTime;
        executionCount = counter.executionCount;
        maxExecutionTime = counter.maxExecutionTime;
        totalBlockingTime = counter.totalBlockingTime;
        unparkCount = counter.unparkCount;
        maxBlockingTime = counter.maxBlockingTime;
        totalWaitSocketIOTime = counter.totalWaitSocketIOTime;
        waitSocketIOCount = counter.waitSocketIOCount;
        maxWaitSocketIOTime = counter.maxWaitSocketIOTime;
        switchCount = counter.switchCount;
        unparkFromJvmCount = counter.unparkFromJvmCount;
        runningTaskCount = counter.getCurrentRunningTaskCount();
        taskQueueLength = counter.getCurrentTaskQueueLength();
    }

    void resetMaxValue() {
        maxEnqueueTime = 0;
        maxExecutionTime = 0;
        maxWaitSocketIOTime = 0;
        maxBlockingTime = 0;
    }

    void cleanup() {
        carrier = null;
    }

    static WispCounter create(WispCarrier carrier) {
        return new WispCounter(carrier);
    }
}
