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

import sun.misc.SharedSecrets;
import sun.misc.VM;
import sun.security.action.GetPropertyAction;
import java.security.AccessController;
import java.util.HashMap;
import java.util.Map;
import java.util.Collection;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.Constraint;

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

    /*
     * @return all resource limits specified by this configuration
     */
    Collection<Constraint> getAllConstraints() {
        return constraints.values();
    }

    void setConstraint(Constraint constraint) {
        constraints.put(constraint.getResourceType(), constraint);
    }
}
