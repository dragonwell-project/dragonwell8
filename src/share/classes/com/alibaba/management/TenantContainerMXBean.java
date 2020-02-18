package com.alibaba.management;

import java.lang.management.PlatformManagedObject;
import java.util.List;

/**
 * MXBean interface to interact with TenantContainer object
 */
public interface TenantContainerMXBean extends PlatformManagedObject {

    /**
     * Retrieve all the IDs of all created {@code TenantContainer}'s
     * @return List of TenantContainer IDs
     */
    public List<Long> getAllTenantIds();

    /**
     * Retrieve the accumulated allocated memory size in bytes
     * of a {@code TenantContainer} represented by {@code id}
     * @param id  ID of the {@code TenantContainer} object to be queried
     * @return Accumulated allocated memory size in bytes
     * @throws IllegalArgumentException If cannot find valid {@code TenantContainer}
     *              object corresponding to the given ID
     */
    public long getTenantAllocatedMemoryById(long id);

    /**
     * Retrieve name of a {@code TenantContainer} object represent by 'id'
     * @param id ID of the {@code TenantContainer} object to be queried
     * @return  Name of found ID
     * @throws IllegalArgumentException If cannot find valid {@code TenantContainer}
     *              object corresponding to the given ID
     */
    public String getTenantNameById(long id);
}
