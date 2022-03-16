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

package com.alibaba.management;

import com.alibaba.wisp.engine.WispCounter;

import java.lang.management.PlatformManagedObject;
import java.util.List;

public interface WispCounterMXBean extends PlatformManagedObject {

    /**
     * @return list of managed wisp worker running state
     */
    List<Boolean> getRunningStates();

    /**
     * @return list of managed wisp worker switch count
     */
    List<Long> getSwitchCount();

    /**
     * @return list of managed wisp worker wait time total, unit ns
     */
    List<Long> getWaitTimeTotal();

    /**
     * @return list of managed wisp worker running time total, unit ns
     */
    List<Long> getRunningTimeTotal();

    /**
     * @return list of managed wisp worker complete task count
     */
    List<Long> getCompleteTaskCount();

    /**
     * @return list of managed wisp worker create task count
     */
    List<Long> getCreateTaskCount();

    /**
     * @return list of managed wisp worker park count
     */
    List<Long> getParkCount();

    /**
     * @return list of managed wisp worker unpark count
     */
    List<Long> getUnparkCount();

    /**
     * @return list of managed wisp worker lazy unpark count
     * @deprecated the lazy unpark mechanism has been removed since ajdk 8.6.11
     */
    @Deprecated
    List<Long> getLazyUnparkCount();

    /**
     * @return list of managed wisp worker unpark interrupt selector count
     */
    List<Long> getUnparkInterruptSelectorCount();

    /**
     * @return list of managed wisp worker do IO count
     */
    List<Long> getSelectableIOCount();

    /**
     * @return list of managed wisp worker timeout count
     */
    List<Long> getTimeOutCount();

    /**
     * @return list of managed wisp worker do event loop count
     */
    List<Long> getEventLoopCount();

    /**
     * @return list of managed wisp worker task queue length
     */
    List<Long> getQueueLength();

    /**
     * @return list of number of running tasks from managed wisp workers
     */
    List<Long> getNumberOfRunningTasks();

    /**
     * @return list of total blocking time in nanos from managed wisp workers
     */
    List<Long> getTotalBlockingTime();

    /**
     * @return list of total execution time in nanos from managed wisp workers
     */
    List<Long> getTotalExecutionTime();

    /**
     * @return list of execution count from managed wisp workers
     */
    List<Long> getExecutionCount();

    /**
     * @return list of total enqueue time in nanos from managed wisp workers
     */
    List<Long> getTotalEnqueueTime();

    /**
     * @return list of enqueue count from managed wisp workers
     */
    List<Long> getEnqueueCount();

    /**
     * @return list of total wait socket io time in nanos from managed wisp workers
     */
    List<Long> getTotalWaitSocketIOTime();

    /**
     * @return list of wait socket io event count from managed wisp workers
     */
    List<Long> getWaitSocketIOCount();

    /**
     * @param id WispEngine id
     * @return WispCounter data
     */
    WispCounter getWispCounter(long id);
}
