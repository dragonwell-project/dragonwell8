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

/**
 * Factory class for {@link ResourceContainer}.
 * <p>
 * Each ResourceContainer implementation needs to provide a public
 * ResourceContainerFactory instance to allow users to choose a specific
 * ResourceContainer implementation:
 *
 * <pre>
 * ResourceContainerFactory FACTORY_INSTANCE = new ResourceContainerFactory() {
 *     protected ResourceContainer createContainer(Iterable<Constraint> constraints) {
 *         return new AbstractResourceContainer() {
 *             // implement abstract methods
 *         }
 *     }
 * }
 * </pre>
 *
 * Then API users can create ResourceContainer by
 * {@code FACTORY_INSTANCE.createContainer(...)}
 */
public interface ResourceContainerFactory {
    /**
     * Builds ResourceContainer with constraints.
     *
     * @param constraints the target {@code Constraint}s
     * @return a newly-created ResourceContainer
     */
    ResourceContainer createContainer(Iterable<Constraint> constraints);
}