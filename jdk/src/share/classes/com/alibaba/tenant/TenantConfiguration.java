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

package com.alibaba.tenant;

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceType;
import sun.misc.SharedSecrets;
import sun.misc.VM;
import sun.security.action.GetPropertyAction;
import java.security.AccessController;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Collection;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.Constraint;
import java.util.stream.Collectors;
import java.util.stream.LongStream;
import java.util.stream.Stream;
import static com.alibaba.rcm.ResourceType.*;
import static com.alibaba.tenant.TenantResourceType.*;

/**
 *
 * The configuration used by tenant
 */
public class TenantConfiguration {

    /*
     * Resource throttling configurations
     */
    private Map<ResourceType, Constraint> constraints = new HashMap<>();

    /**
     * Create an empty TenantConfiguration, no limitations on any resource
     */
    public TenantConfiguration() {
    }

    /**
     * @param cpuShare
     * @param maxHeapBytes
     */
    public TenantConfiguration(int cpuShare, long maxHeapBytes) {
        limitCpuShares(cpuShare);
        limitHeap(maxHeapBytes);
    }

    /**
     * @param maxHeapBytes
     */
    public TenantConfiguration(long maxHeapBytes) {
        limitHeap(maxHeapBytes);
    }

    /**
     * Build TenantConfiguration with given constraints
     * @param constraints
     */
    public TenantConfiguration(Iterable<Constraint> constraints) {
        if (constraints != null) {
            for (Constraint c : constraints) {
                this.constraints.put(c.getResourceType(), c);
            }
        }
    }

    /**
     * Use cgroup's cpu.cfs_* controller to limit cpu time of
     * new {@code TenantContainer} object created from this configuration
     * @param period    corresponding to cpu.cfs_period
     * @param quota     corresponding to cpu.cfs_quota
     * @return current {@code TenantConfiguration}
     */
    public TenantConfiguration limitCpuCfs(int period, int quota) {
        if (!TenantGlobals.isCpuThrottlingEnabled()) {
            throw new UnsupportedOperationException("-XX:+TenantCpuThrottling is not enabled");
        }
        constraints.put(CPU_CFS, CPU_CFS.newConstraint(period, quota));
        return this;
    }

    /**
     * Use cgroup's cpu.shares controller to limit cpu shares of
     * new {@code TenantContainer} object created from this configuration
     * @param share relative weight of cpu shares
     * @return current {@code TenantConfiguration}
     */
    public TenantConfiguration limitCpuShares(int share) {
        if (!TenantGlobals.isCpuThrottlingEnabled()) {
            // use warning messages instead of exception here to keep backward compatibility
            System.err.println("WARNING: -XX:+TenantCpuThrottling is disabled!");
            //throw new UnsupportedOperationException("-XX:+TenantCpuThrottling is not enabled");
        }
        constraints.put(CPU_SHARES, CPU_SHARES.newConstraint(share));
        return this;
    }

    /**
     * Use cgroup's cpu.cpuset controller to limit cpu cores allowed to be used
     * by new {@code TenantContainer} object created from this configuration
     * @param cpuSets string of cpuset description, such as "1,2,3", "0-7,11"
     * @return current {@code TenantConfiguration}
     */
    public TenantConfiguration limitCpuSet(String cpuSets) {
        if (!TenantGlobals.isCpuThrottlingEnabled()) {
            throw new UnsupportedOperationException("-XX:+TenantCpuThrottling is not enabled");
        }
        constraints.put(CPUSET_CPUS, CPUSET_CPUS.newConstraint(parseCpuSetString(cpuSets)));
        return this;
    }

    private long[] parseCpuSetString(String cpuSets) {
        if (cpuSets == null || cpuSets.isEmpty()) {
            return null;
        }
        return Stream.of(cpuSets.split(","))
                .flatMapToLong(s -> {
                    // for core "1-4", expand to 1,2,3,4
                    if (s.contains("-")) {
                        List<Long> ranges = Stream.of(s.split("-"))
                                .map(Long::parseLong)
                                .collect(Collectors.toList());
                        return LongStream.range(ranges.get(0), ranges.get(1) + 1);
                    } else {
                        return LongStream.of(Long.parseLong(s));
                    }
                })
                .toArray();
    }

    /**
     * Limit total heap size of new {@code TenantContainer} created from this configuration
     * @param maxJavaHeapBytes maximum heap size in byte
     * @return current {@code TenantConfiguration}
     */
    public TenantConfiguration limitHeap(long maxJavaHeapBytes) {
        if (!TenantGlobals.isHeapThrottlingEnabled()) {
            // use warning messages instead of exception here to keep backward compatibility
            System.err.println("WARNING: -XX:+TenantHeapThrottling not enabled!");
            //throw new UnsupportedOperationException("-XX:+TenantHeapThrottling is not enabled");
        }
        constraints.put(HEAP_RETAINED, HEAP_RETAINED.newConstraint(maxJavaHeapBytes));
        return this;
    }

    /*
     * @return all resource limits specified by this configuration
     */
    Collection<Constraint> getAllConstraints() {
        return constraints.values();
    }

    void setConstraint(Constraint constraint) {
        constraints.put(constraint.getResourceType(), constraint);
    }

    /**
     * @return the max amount of heap the tenant is allowed to consume.
     */
    public long getMaxHeap() {
        if (constraints.containsKey(HEAP_RETAINED)) {
            return constraints.get(HEAP_RETAINED).getValues()[0];
        }
        return Runtime.getRuntime().maxMemory();
    }

    /*
     * Corresponding to combination of Linux cgroup's cpu.cfs_period_us and cpu.cfs_quota_us
     * @return the max percent of cpu the tenant is allowed to consume, -1 means unlimited
     */
    public int getMaxCpuPercent() {
        if (constraints.containsKey(CPU_CFS)) {
            Constraint c = constraints.get(CPU_CFS);
            int period = (int) c.getValues()[0];
            int quota = (int) c.getValues()[1];
            if (period > 0 && quota > 0) {
                return (int) (((float) quota / (float) period) * 100);
            }
        }
        return -1;
    }

    /**
     * Corresponding to Linux cgroup's cpu.shares
     * @return the weight, impact the ratio of cpu among all tenants.
     */
    public int getCpuShares() {
        if (constraints.containsKey(CPU_SHARES)) {
            return (int) constraints.get(CPU_SHARES).getValues()[0];
        }
        return 0;
    }
}
