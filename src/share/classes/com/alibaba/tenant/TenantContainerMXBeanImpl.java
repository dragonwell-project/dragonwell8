package com.alibaba.tenant;

import com.alibaba.management.TenantContainerMXBean;
import sun.management.Util;
import javax.management.ObjectName;
import java.util.List;

/**
 * Implementation class for TenantContainerMXBean.
 */
public class TenantContainerMXBeanImpl implements TenantContainerMXBean {

    private final static String TENANT_CONTAINER_MXBEAN_NAME = "com.alibaba.management:type=TenantContainer";

    @Override
    public List<Long> getAllTenantIds() {
        return TenantContainer.getAllTenantIds();
    }

    public long getTenantAllocatedMemoryById(long id) {
        TenantContainer container = TenantContainer.getTenantContainerById(id);
        if (null == container) {
            throw new IllegalArgumentException("The id of tenant is invalid !");
        }
        return container.getAllocatedMemory();
    }

    @Override
    public ObjectName getObjectName() {
        return Util.newObjectName(TENANT_CONTAINER_MXBEAN_NAME);
    }

    @Override
    public String getTenantNameById(long id) {
        TenantContainer container = TenantContainer.getTenantContainerById(id);
        if (null == container) {
            throw new IllegalArgumentException("The id of tenant is invalid !");
        }
        return container.getName();
    }
}
