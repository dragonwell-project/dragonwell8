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
import com.alibaba.rcm.internal.AbstractResourceContainer;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import static com.alibaba.tenant.TenantState.*;

class TenantResourceContainer extends AbstractResourceContainer {

    TenantResourceContainer(TenantResourceContainer parent,
                            TenantContainer tenant,
                            Iterable<Constraint> constraints) {
        super();
        this.parent = parent;
        this.constraints = new HashMap<>();
        this.tenant = tenant;
        if (constraints != null) {
            constraints.forEach(c -> this.constraints.put(c.getResourceType(), c));
        }
        init();
    }

    private void init() {
        if (TenantGlobals.isCpuThrottlingEnabled() || TenantGlobals.isCpuAccountingEnabled()) {
            if (constraints.containsKey(ResourceType.CPU_PERCENT)) {
                Constraint c = translate(constraints.remove(ResourceType.CPU_PERCENT));
                constraints.put(c.getResourceType(), c);
            }
            jgroup = new JGroup(this);
        }
    }

    private static Constraint translate(Constraint constraint) {
        if (constraint.getResourceType() == ResourceType.CPU_PERCENT) {
            // translate CPU_PERCENT to cfs_quota_us/cfs_period_us
            long period = Long.parseLong(JGroup.jvmGroup().getValue(NativeDispatcher.CG_CPU_CFS_PERIOD));
            long quota = (period * constraint.getValues()[0] / 100L);
            return TenantResourceType.CPU_CFS.newConstraint(period, quota);
        }
        return constraint;
    }

    // cached constraints
    private Map<ResourceType, Constraint> constraints;

    /*
     * The parent container.
     */
    private TenantResourceContainer parent;

    /*
     * Associated TenantContainer object
     */
    private TenantContainer tenant;

    /*
     * tenant jgroup
     */
    private volatile JGroup jgroup;

    TenantResourceContainer getParent() {
        return parent;
    }

    JGroup getJGroup() {
        return jgroup;
    }

    TenantContainer getTenant() {
        return this.tenant;
    }

    @Override
    protected void attach() {
        if (jgroup != null) {
            jgroup.attach();
        }
        super.attach();
    }

    @Override
    protected void detach() {
        super.detach();
        if (jgroup != null) {
            jgroup.detach();
        }
    }


    @Override
    public List<Long> getActiveContainerThreadIds() {
        throw new UnsupportedOperationException("Should not call getActiveTenantThreadIds");
    }

    @Override
    public void run(Runnable command) {
        // This is the first thread which runs in this tenant container
        if (tenant.getState() == STARTING) {
            // move the tenant state to RUNNING
            tenant.setState(RUNNING);
        }
        super.run(command);
    }

    @Override
    public State getState() {
        return TenantState.translate(tenant.getState());
    }

    @Override
    public void updateConstraint(Constraint constraint) {
        Constraint c = translate(constraint);
        if (c.getResourceType() instanceof TenantResourceType) {
            TenantResourceType type = (TenantResourceType)c.getResourceType();
            if (type.isJGroupResource()
                    && jgroup != null) {
                ((JGroupConstraint)c).sync(jgroup);
            }
        }
        constraints.put(c.getResourceType(), c);
    }

    @Override
    public Iterable<Constraint> getConstraints() {
        return constraints.values();
    }

    @Override
    public void destroy() {
        throw new UnsupportedOperationException("Should not call TenantResourceContainer::destroy() directly");
    }

    @Override
    public Long getConsumedAmount(ResourceType resourceType) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Long getResourceLimitReachedCount(ResourceType resourceType) {
        throw new UnsupportedOperationException();
    }

    void destroyImpl() {
        if (jgroup != null) {
            jgroup.destory();
            jgroup = null;
        }
        parent = null;
        tenant = null;
    }

    // exposed to TenantContainer implementation
    long getProcessCpuTime() {
        if ( jgroup != null) {
            return jgroup.getCpuTime();
        }
        return 0;
    }
}
