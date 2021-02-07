package com.alibaba.rcm;

import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

public class ResourceContainerMonitor {
    private static Map<Long, ResourceContainer> resourceContainerMap = new HashMap<>();
    private static long idGen = 0;

    public synchronized static long register(ResourceContainer resourceContainer) {
        long id = idGen++;
        resourceContainerMap.put(id, resourceContainer);
        return id;
    }

    public synchronized static ResourceContainer getContainerById(long id) {
        return resourceContainerMap.get(id);
    }

    public synchronized static List<Long> getAllContainerIds() {
        return new ArrayList<>(resourceContainerMap.keySet());
    }

    public synchronized static List<Constraint> getConstraintsById(long id) {
        AbstractResourceContainer resourceContainer = (AbstractResourceContainer) resourceContainerMap.get(id);
        if (resourceContainer == null)
            throw new IllegalArgumentException("Invalid ResourceContainer id : " + id);
        return StreamSupport
                .stream(resourceContainer.getConstraints().spliterator(), false)
                .collect(Collectors.toList());
    }
}
