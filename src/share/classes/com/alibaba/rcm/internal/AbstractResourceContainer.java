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

package com.alibaba.rcm.internal;

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceContainerMonitor;
import com.alibaba.rcm.ResourceType;
import sun.misc.SharedSecrets;
import sun.misc.VM;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A skeletal implementation of {@link ResourceContainer} that practices
 * the attach/detach paradigm described in {@link ResourceContainer#run(Runnable)}.
 * <p>
 * Each {@code ResourceContainer} implementation must inherit from this class.
 *
 * @see ResourceContainer#run(Runnable)
 */

public abstract class AbstractResourceContainer implements ResourceContainer {

    protected final static AbstractResourceContainer ROOT = new RootContainer();
    final long id;

    protected AbstractResourceContainer() {
        id = ResourceContainerMonitor.register(this);
    }

    public static AbstractResourceContainer root() {
        return ROOT;
    }

    public static AbstractResourceContainer current() {
        if (!VM.isBooted()) {
            // JLA will be available only after full VM bootstrap.
            // before that stage, we assume VM is running in ROOT container.
            return ROOT;
        }
        return SharedSecrets.getJavaLangAccess().getResourceContainer(Thread.currentThread());
    }

    public abstract List<Long> getActiveContainerThreadIds();

    public abstract Long getConsumedAmount(ResourceType resourceType);

    public abstract Long getResourceLimitReachedCount(ResourceType resourceType);

    @Override
    public void run(Runnable command) {
        if (getState() != State.RUNNING) {
            throw new IllegalStateException("container not running");
        }
        ResourceContainer container = current();
        if (container == this) {
            command.run();
        } else {
            if (container != ROOT) {
                throw new IllegalStateException("must be in root container " +
                        "before running into non-root container.");
            }
            attach();
            try {
                command.run();
            } finally {
                detach();
            }
        }
    }

    /**
     * Attach to this resource container.
     * Ensure {@link ResourceContainer#current()} as a root container
     * before calling this method.
     * <p>
     * The implementation class must call {@code super.attach()} to coordinate
     * with {@link ResourceContainer#current()}
     */
    protected void attach() {
        SharedSecrets.getJavaLangAccess().setResourceContainer(Thread.currentThread(), this);
    }

    @Override
    public Long getId() {
        return id;
    }

    /**
     * Detach from this resource container and return to root container.
     * <p>
     * The implementation class must call {@code super.detach()} to coordinate
     * with {@link ResourceContainer#current()}
     */
    protected void detach() {
        SharedSecrets.getJavaLangAccess().setResourceContainer(Thread.currentThread(), root());
    }

    private static class RootContainer extends AbstractResourceContainer {
        @Override
        public void run(Runnable command) {
            AbstractResourceContainer container = current();
            if (container == ROOT) {
                command.run();
                return;
            }
            container.detach();
            try {
                command.run();
            } finally {
                container.attach();
            }
        }

        @Override
        public ResourceContainer.State getState() {
            return ResourceContainer.State.RUNNING;
        }

        @Override
        protected void attach() {
            throw new UnsupportedOperationException("should not reach here");
        }

        @Override
        protected void detach() {
            throw new UnsupportedOperationException("should not reach here");
        }

        @Override
        public void updateConstraint(Constraint constraint) {
            throw new UnsupportedOperationException("updateConstraint() is not supported by root container");
        }

        @Override
        public Iterable<Constraint> getConstraints() {
            return Collections.emptyList();
        }

        @Override
        public void destroy() {
            throw new UnsupportedOperationException("destroy() is not supported by root container");
        }

        @Override
        public Long getId() {
            return id;
        }

        @Override
        public Long getConsumedAmount(ResourceType resourceType) {
            return 0L;
        }

        @Override
        public Long getResourceLimitReachedCount(ResourceType resourceType) {
            return 0L;
        }

        @Override
        public List<Long> getActiveContainerThreadIds() {
            ThreadGroup group = Thread.currentThread().getThreadGroup();
            while (group.getParent() != null) {
                group = group.getParent();
            }
            int count = group.activeCount();
            Thread[] threads;
            do {
                threads = new Thread[count + (count / 2) + 1]; //slightly grow the array size
                count = group.enumerate(threads, true);
                //return value of enumerate() must be strictly less than the array size according to javadoc
            } while (count == threads.length);

            final List<Long> result = new ArrayList<>(count);
            for (int i = 0; i < count; ++i) {
                if (SharedSecrets.getJavaLangAccess().getResourceContainer(threads[i]) == ROOT) {
                    result.add(threads[i].getId());
                }
            }
            return result;
        }
    }
}
