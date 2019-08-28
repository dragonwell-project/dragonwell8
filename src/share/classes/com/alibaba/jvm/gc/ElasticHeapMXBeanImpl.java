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

package com.alibaba.jvm.gc;
import com.alibaba.management.ElasticHeapMXBean;
import com.alibaba.management.ElasticHeapEvaluationMode;
import sun.management.Util;
import javax.management.ObjectName;
import sun.security.action.GetBooleanAction;
import java.security.AccessController;

/**
 * Implementation class for ElasticHeapMXBean.
 */
public class ElasticHeapMXBeanImpl implements ElasticHeapMXBean {
    private static native void registerNatives();

    static {
        registerNatives();
        ELASTIC_HEAP_ENABLED = AccessController.doPrivileged(
            new GetBooleanAction("com.alibaba.jvm.gc.ElasticHeapEnabled"));
    }

    private final static String ELASTIC_HEAP_MXBEAN_NAME = "com.alibaba.management:type=ElasticHeap";

    // value of property "com.alibaba.jvm.gc.elasticHeapEnabled"
    private static final boolean    ELASTIC_HEAP_ENABLED;

    private static void checkElasticHeapEnabled() {
        if (!ELASTIC_HEAP_ENABLED) {
            throw new UnsupportedOperationException("-XX:+G1ElasticHeap is not enabled");
        }
    }

    @Override
    public ElasticHeapEvaluationMode getEvaluationMode() {
        checkElasticHeapEnabled();
        ElasticHeapEvaluationMode[] modes = ElasticHeapEvaluationMode.values();
        int mode = getEvaluationModeImpl();
        // Keap the number aligned with Hotspot
        assert mode < modes.length;
        return modes[mode];
    }

    @Override
    public void setYoungGenCommitPercent(int percent) {
        checkElasticHeapEnabled();
        setYoungGenCommitPercentImpl(percent);
    }

    @Override
    public int getYoungGenCommitPercent() {
        checkElasticHeapEnabled();
        return getYoungGenCommitPercentImpl();
    }

    @Override
    public void setUncommitIHOP(int percent) {
        checkElasticHeapEnabled();
        if (percent < 0 || percent > 100) {
            throw new IllegalArgumentException("argument should be between 0 and 100");
        }
        setUncommitIHOPImpl(percent);
    }

    @Override
    public int getUncommitIHOP() {
        checkElasticHeapEnabled();
        return getUncommitIHOPImpl();
    }

    @Override
    public long getTotalYoungUncommittedBytes() {
        checkElasticHeapEnabled();
        return getTotalYoungUncommittedBytesImpl();
    }

    @Override
    public void setSoftmxPercent(int percent) {
        checkElasticHeapEnabled();
        if (percent < 0 || percent > 100) {
            throw new IllegalArgumentException("argument should be between 0 and 100");
        }
        setSoftmxPercentImpl(percent);
    }

    @Override
    public int getSoftmxPercent() {
        checkElasticHeapEnabled();
        return getSoftmxPercentImpl();
    }

    @Override
    public long getTotalUncommittedBytes() {
        checkElasticHeapEnabled();
        return getTotalUncommittedBytesImpl();
    }

    @Override
    public ObjectName getObjectName() {
        return Util.newObjectName(ELASTIC_HEAP_MXBEAN_NAME);
    }

    private static native int getEvaluationModeImpl();
    private static native void setYoungGenCommitPercentImpl(int percent);
    private static native int getYoungGenCommitPercentImpl();
    private static native void setUncommitIHOPImpl(int percent);
    private static native int getUncommitIHOPImpl();
    private static native long getTotalYoungUncommittedBytesImpl();
    private static native void setSoftmxPercentImpl(int percent);
    private static native int getSoftmxPercentImpl();
    private static native long getTotalUncommittedBytesImpl();
}
