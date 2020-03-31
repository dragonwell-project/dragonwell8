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

import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.Constraint;
import java.util.stream.Collectors;
import java.util.Arrays;
import static com.alibaba.tenant.NativeDispatcher.*;

/*
 * Type of resource that can be throttled when MultiTenant feature enabled
 */
class TenantResourceType extends ResourceType {

    /**
     * Throttle CPU resource with cgroup controller: cpu.shares.
     * Expecting one constraint value which represents cpu.shares value.
     */
    static final ResourceType CPU_SHARES = new TenantResourceType("CPU_SHARE",true) {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            if (values == null || values.length != 1
                    || values[0] <= 0) {
                throw new IllegalArgumentException("Bad CPU_SHARE constraint:" + values[0]);
            }
        }

        @Override
        public Constraint newConstraint(long ... values) {
            validate(values);
            return new JGroupConstraint(this, values) {
                @Override
                void sync(JGroup jgroup) {
                    if (IS_CPU_SHARES_ENABLED) {
                        jgroup.setValue(CG_CPU_SHARES, Integer.toString((int) getValues()[0]));
                    }
                }
            };
        }
    };

    /**
     * Throttle CPU resource with cgroup controller: cpuset.cpus.
     * Expecting N constraint value which represent cores described in cpuset.cpus.
     * e.g. {@code TenantResourceType.CPUSET_CPUS.newConstraint(LongStream.range(0, 16).toArray()) }
     */
    static final ResourceType CPUSET_CPUS = new TenantResourceType("CPUSET_CPUS", true) {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            if (values == null || values.length > Runtime.getRuntime().availableProcessors()) {
                throw new IllegalArgumentException("Invalid number of cpuset.cpus:"
                        + (values == null ? 0 : values.length));
            }
            StringBuilder invalidCores = new StringBuilder();
            for (long v : values) {
                if (v < 0 || v >= Runtime.getRuntime().availableProcessors()) {
                    invalidCores.append(v).append(",");
                }
            }
            String cores = invalidCores.toString();
            if (!cores.isEmpty()) {
                throw new IllegalArgumentException("Invalid cpuset cores: " + cores);
            }
        }

        @Override
        public Constraint newConstraint(long... values) {
            validate(values);
            return new JGroupConstraint(this, values) {
                @Override
                void sync(JGroup jgroup) {
                    if (IS_CPUSET_ENABLED) {
                        String cores = Arrays.stream(getValues())
                                .mapToObj(l -> Long.toString(l))
                                .collect(Collectors.joining(","));
                        jgroup.setValue(CG_CPUSET_CPUS, cores);
                    }
                }
            };
        }
    };

    /**
     * Throttle CPU resource with cgroup controller: cpu.cfs_quota_us/cpu.cfs_period_us.
     * Expecting two constraints:
     * [0] corresponding to cpu.cfs_period_us
     * [1] corresponding to cpu.cfs_quota_us
     */
    static final ResourceType CPU_CFS = new TenantResourceType("CPU_CFS", true) {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            if (values == null || values.length != 2) {
                throw new IllegalArgumentException("CPU_CFS requires constraints period and quota at the same time");
            }
            long period = values[0];
            long quota = values[1];
            // according to https://www.kernel.org/doc/Documentation/scheduler/sched-bwc.txt
            // cpu.cfs_period_us should be in range [1 ms, 1 sec]
            if (period < 1_000 || period > 1_000_000) {
                throw new IllegalArgumentException("Invalid cpu_cfs.period value:" + period
                        + ", expected range [1_000, 1_000_000");
            }
            if (quota < 1_000 && quota != -1) {
                throw new IllegalArgumentException("Invalid cpu_cfs.quota value:" + quota
                        + ", should >= 1_000 or be unlimited (-1)");
            }
        }

        @Override
        public Constraint newConstraint(long... values) {
            validate(values);
            return new JGroupConstraint(this, values) {
                @Override
                void sync(JGroup jgroup) {
                    if (IS_CPU_CFS_ENABLED) {
                        jgroup.setValue(CG_CPU_CFS_PERIOD, Long.toString(values[0]));
                        jgroup.setValue(CG_CPU_CFS_QUOTA, Long.toString(values[1]));
                    }
                }
            };
        }
    };

    /**
     * Throttle total number of opened socket file descriptors.
     * Expecting one constraint value which is the maximum number of socket FDs.
     */
    static final ResourceType SOCKET = new TenantResourceType("SOCKET",false) {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            if (values == null || values.length != 1
                    || values[0] <= 0) {
                throw new IllegalArgumentException("Bad CPU_SHARE constraint:" + values[0]);
            }
        }
    };

    private TenantResourceType(String name, boolean isJGroup) {
        super(name);
        this.isJGroupResource = isJGroup;
    }

    // if this type of resource is controlled by JGroup
    private boolean isJGroupResource;

    /**
     * Check if this type of resource is controlled by cgroup
     * @return
     */
    boolean isJGroupResource() {
        return this.isJGroupResource;
    }
}
