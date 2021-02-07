package com.alibaba.rcm;

import com.alibaba.management.ResourceContainerMXBean;
import sun.management.Util;

import javax.management.ObjectName;
import java.util.List;
import java.util.stream.Collectors;

public class ResourceContainerMXBeanImpl implements ResourceContainerMXBean {
    private final static String TENANT_CONTAINER_MXBEAN_NAME = "com.alibaba.management:type=ResourceContainer";

    @Override
    public List<Long> getAllContainerIds() {
        return ResourceContainerMonitor.getAllContainerIds();
    }

    @Override
    public List<Long> getConstraintsById(long id) {
        return ResourceContainerMonitor.getConstraintsById(id).stream().map(c -> c.getValues()[0]).collect(Collectors.toList());
    }


    @Override
    public long getCPUResourceConsumedAmount(long id) {
        ResourceContainer container = ResourceContainerMonitor.getContainerById(id);
        return container.getConsumedAmount(ResourceType.CPU_PERCENT);
    }

    @Override
    public ObjectName getObjectName() {
        return Util.newObjectName(TENANT_CONTAINER_MXBEAN_NAME);
    }
}
