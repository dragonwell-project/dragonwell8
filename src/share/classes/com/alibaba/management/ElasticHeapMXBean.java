/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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

import java.lang.management.PlatformManagedObject;
import java.util.List;

/**
 * The ElasticHeapMXBean interface provides APIs for manipulating memory commitment of heap
 */
public interface ElasticHeapMXBean extends PlatformManagedObject {

    /**
     * @return ElasticHeapEvaluationMode of elastic heap
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     */
    public ElasticHeapEvaluationMode getEvaluationMode();

    /**
     * Set memory commit percent of young generation in heap
     *
     * @param percent commit percent of young generation to set
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if commit percent cannot be set. Error can be gotten by Exception.getMessage()
     * @throws IllegalArgumentException if percent is not 0 or between
     *          ElasticHeapMinYoungCommitPercent(VM option) and 100
     */
    public void setYoungGenCommitPercent(int percent);

    /**
     * @return memory commit percent of young generation percentage in heap
     * (toInt(YoungGenCommitPercent * 100))
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if not in command control mode
     */
    public int getYoungGenCommitPercent();

    /**
     * Set memory commit percent of young generation in heap
     *
     * @param percent percent of heap to set to trigger concurrent mark and do memory uncommitment
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if IHOP percent cannot be set. Error can be gotten by Exception.getMessage()
     * @throws IllegalArgumentException if percent is not between 0 and 100
     */
    public void setUncommitIHOP(int percent);

    /**
     * @return percent of heap to set to trigger concurrent mark and do memory uncommitment
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if not in command control mode
     */
    public int getUncommitIHOP();


    /**
     * @return number of uncommited bytes of young generation in heap
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if not in command control mode
     */
    public long getTotalYoungUncommittedBytes();

    /**
     * Set memory commit percent of heap
     *
     * @param percent the percentage of Softmx in Xmx to set
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if percent cannot be set. Error can be gotten by Exception.getMessage()
     * @throws IllegalArgumentException if percent is between 0 and 100
     */
    public void setSoftmxPercent(int percent);

    /**
     * @return softmx percent
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if not in softmx mode
     */
    public int getSoftmxPercent();

    /**
     * @return number of uncommited bytes of heap
     * @throws UnsupportedOperationException if -XX:+G1ElasticHeap is not enabled
     * @throws IllegalStateException if not in softmx mode
     */
    public long getTotalUncommittedBytes();
}
