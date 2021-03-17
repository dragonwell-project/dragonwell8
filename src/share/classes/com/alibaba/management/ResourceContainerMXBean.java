package com.alibaba.management;

import java.lang.management.PlatformManagedObject;
import java.util.List;


/**
 * Platform-specific management interface for the resource container
 * of the Java virtual machine.
 */
public interface ResourceContainerMXBean extends PlatformManagedObject {
    /**
     * Get all running containers uniq id as List
     * @return all active containers' id
     */
    List<Long> getAllContainerIds();

    /**
     * Get a specific container's constraints by id
     * @param id container id
     * @return constraints as list
     */
    List<Long> getConstraintsById(long id);

    /**
     * Get the total cpu time consumed by id specified container
     * @param id container id
     * @return consumed cpu time by nanosecond
     */
    long getCPUResourceConsumedAmount(long id);

    /**
     * Get how many times the resource limitation has been reached
     * @param id
     * @return
     */
    long getCPUResourceLimitReachedCount(long id);

    /**
     * Get how many active threads are running in container
     * @param id container id
     * @return thread id as list
     */
    List<Long> getActiveContainerThreadIds(long id);
}
