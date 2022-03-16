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
 *
 */

package com.alibaba.rcm;

import java.util.Objects;

/**
 * Enumeration of {@link Constraint}'s type.
 * <p>
 * {@code class} is used instead of {@code enum} to provide extensibility.
 * Implementation of resource management can define its resource type.
 * Below is an example of extension ResourceType:
 *
 * <pre>
 *     public class MyResourceType extends ResourceType {
 *         public final static ResourceType CPU_CFS = new MyResourceType();
 *
 *         public MyResourceType() {}
 *     }
 * </pre>
 * <p>
 * CPU_CFS is an instance of {@code ResourceType}, so it can be used wherever
 * ResourceType is handled.
 * <p>
 * The descriptions and parameters of each public final static value need to
 * be documented in detail.
 */
public class ResourceType {
    /**
     * Throttling the CPU usage by CPU percentage.
     * <p>
     * param #1: CPU usage measured in a percentage granularity.
     * The value ranges from 0 to CPU_COUNT * 100. For example, {@code 150}
     * means that ResourceContainer can use up to 1.5 CPU cores.
     */
    public final static ResourceType CPU_PERCENT = new ResourceType("CPU_PERCENT") {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            Objects.requireNonNull(values);
            if (values.length != 1
                    || values[0] < 1
                    || values[0] > Runtime.getRuntime().availableProcessors() * 100L) {
                throw new IllegalArgumentException("Bad CPU_PERCENT constraint: " + values[0]);
            }
        }
    };

    /**
     * Throttling the max heap usage.
     * <p>
     * param #1: maximum heap size in bytes
     */
    public final static ResourceType HEAP_RETAINED = new ResourceType("HEAP_RETAINED") {
        @Override
        protected void validate(long... values) throws IllegalArgumentException {
            Objects.requireNonNull(values);
            if (values.length != 1
                    || values[0] <= 0
                    || values[0] > Runtime.getRuntime().maxMemory()) {
                throw new IllegalArgumentException("Bad HEAP_RETAINED constraint: " + values[0]);
            }
        }
    };

    // name of this ResourceType
    private final String name;

    protected ResourceType(String name) {
        this.name = name;
    }

    /**
     * Creates a {@link Constraint} with this {@code ResourceType} and
     * the given {@code values}.
     *
     * @param values constraint values
     * @return newly-created Constraint
     * @throws IllegalArgumentException when parameter check fails
     */
    public Constraint newConstraint(long... values) {
        validate(values);
        return new Constraint(this, values);
    }

    /**
     * Checks the validity of parameters. Since a long [] is used to
     * express the configuration, a length and range check is required.
     * <p>
     * Each ResourceType instance can implement its own {@code validate()}
     * method through Override, for example:
     * <pre>
     * public final static ResourceType MY_RESOURCE =
     *     new ResourceType() {
     *         protected void validate(long[] values) throws IllegalArgumentException {
     *              // the check logic
     *         }
     *     };
     * </pre>
     *
     * @param values parameter value
     * @throws IllegalArgumentException if validation failed
     */
    protected void validate(long... values) throws IllegalArgumentException {
        // No check at all!
    }

    @Override
    public String toString() {
        return "ResourceType-" + name;
    }
}
