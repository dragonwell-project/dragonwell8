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

import com.alibaba.rcm.internal.AbstractResourceContainer;

/**
 * A {@code ResourceContainer} defines a set of resource {@link Constraint}s
 * that limit resource usage by threads.
 * Zero or more threads can be attached to one {@code ResourceContainer}.
 * <pre>
 * +-------------------------------+  +-----------------------------+
 * |ResourceContainer              |  |RootContainer                |
 * |                               |  |                             |
 * | +---------------+             |  |                             |
 * | |CPU Constraint |             |  |        +----------+         |
 * | +---------------+      <------------------+  Thread  |         |
 * | +---------------+             |  |        +----------+         |
 * | |Mem Constraint |command.run()|  | container.run(command)      |
 * | +---------------+      |      |  |                             |
 * |    |---------------->  v      |  |                             |
 * |  resources are   command.run()|  |                             |
 * |  controlled by   returns      |  |           back to root      |
 * |  constraints            +----------------->  container         |
 * |                               |  |                             |
 * |                               |  |                             |
 * +-------------------------------+  +-----------------------------+
 * </pre>
 * The figure above describes the structure and usage of {@code ResourceContainer}.
 * <p>
 * The main Thread is bounded to root container which can be fetched by
 * {@link ResourceContainer#root()}. Root Container is the system default
 * {@code ResourceContainer} without any resource restrictions.
 * Newly created threads are implicitly bounded to {@code ResourceContainer}
 * of the parent thread.
 * <p>
 * A Thread can invoke {@link ResourceContainer#run(Runnable command)} to
 * attach to the {@code ResourceContainer} and run the {@code command},
 * the resource usage of the thread is controlled by the {@code ResourceContainer}'s
 * constraints while running the command.
 * When the execution of the command is either finished normally
 * or terminated by Exception, the thread will be detached from the container automatically.
 * <p>
 * Components of a {@code ResourceContainer} implementation:
 * <ul>
 *     <li>{@link ResourceType}: an implementation can customize some ResourceType</li>
 *     <li>{@code ResourceContainer}: Implements a class extends
 *     {@link AbstractResourceContainer}</li>
 *     <li>{@link ResourceContainerFactory}: service provider </li>
 * </ul>
 *
 * <p>
 * {@code ResourceContainer} needs to be created from a set of {@code Constraint}s
 * <p>
 * In most cases, the following idiom should be used:
 *  <pre>
 *    ResourceContainer resourceContainer = containerFactory.createContainer(
 *      Arrays.asList(
 *          CPU_PERCENT.newConstraint(50),
 *          HEAP_RETAINED.newConstraint(100_000_000)
 *    ));
 *
 *    resourceContainer.run(requestHandler);
 *
 *    resourceContainer.destroy();
 *  </pre>
 *
 * @see ResourceContainerFactory
 */
public interface ResourceContainer {
    /**
     * An enumeration of Container state
     */
    enum State {
        /**
         * Created but not ready for attaching (initializing).
         */
        CREATED,
        /**
         * Ready for attaching.
         */
        RUNNING,
        /**
         * {@link ResourceContainer#destroy()} has been called.
         */
        STOPPING,
        /**
         * Container is destroyed. Further usage is not allowed.
         */
        DEAD
    }

    /**
     * Returns the system-wide "root" Resource container.
     * <p>
     * Root ResourceContainer is a <em>virtual<em/> container that indicates
     * the default resource container for any thread, which is not attached
     * to a ResourceContainer created by users. Root ResourceContainer does
     * not have any resource constrains.
     * <p>
     * {@link #run(Runnable)} method of root container is a special
     * implementation that detaches from the current container and returns
     * to the root container.
     * It is very useful in ResourceContainer switch scenario:
     * <pre>
     * // Assume we already attach to a non-root resourceContainer1
     * resourceContainer2.run(command);
     * // throws exception, because it is illegal to switch between non-root
     * // ResourceContainers
     * ResourceContainer.root(() -> resourceContainer2.run(command));
     * </pre>
     *
     * @return root container
     */
    static ResourceContainer root() {
        return AbstractResourceContainer.root();
    }

    /**
     * Returns the ResourceContainer associated with the current thread.
     * For threads that do not attach to any user-created ResourceContainer,
     * {@link #root()} is returned.
     *
     * @return current ResourceContainer
     */
    static ResourceContainer current() {
        return AbstractResourceContainer.current();
    }

    /**
     * Returns the current ResourceContainer state.
     *
     * @return current state.
     */
    ResourceContainer.State getState();

    /**
     * Attach the current thread to the ResourceContainer to run the {@code command},
     * and detach the ResourceContainer when {@code command} is either normally finished
     * or terminated by Exception.
     * <p>
     * At the same time, it is not allowed to switch directly between any two
     * containers. If the switch is indeed required, the
     * {@link #root()} container should be used.
     * <p>
     * This way restricts the container attach/detach mode for the API users,
     * but is less error-prone.
     *
     * <pre>
     *     ResourceContainer resourceContainer = ....
     *     assert ResourceContainer.current() == ResourceContainer.root();
     *     resourceContainer.run(() -> {
     *         assert ResourceContainer.current() == resourceContainer;
     *     });
     *     assert ResourceContainer.current() == ResourceContainer.root();
     * </pre>
     *
     * @param command the target code
     */
    void run(Runnable command);

    /**
     * Updates {@link Constraint} of this resource container.
     * <p>
     * Constraints with an identical type will
     * replace each other according to the calling order.
     *
     * @param constraint constraints list
     * @throws UnsupportedOperationException {@link Constraint#getResourceType()} is not
     *                                       supported by the implementation
     */
    void updateConstraint(Constraint constraint);

    /**
     * Gets container's {@link Constraint}s
     *
     * @return {@code Constraint}s
     */
    Iterable<Constraint> getConstraints();

    /**
     * Destroys this resource container, also kills the attached threads and releases
     * resources described in {@link #getConstraints()}.
     * <p>
     * Once this method is called, the state will become {@link State#STOPPING}.
     * And the caller thread will be blocked until all the resources have been released.
     * Then the container state will become {@link State#DEAD}.
     */
    void destroy();

    Long getId();

    Long getConsumedAmount(ResourceType resourceType);
}
