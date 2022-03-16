package com.alibaba.wisp.engine;

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceContainerFactory;

/**
 * Singleton factory specialized for wisp control group
 * {@code ResourceContainer} Theoretically with support of new RCM API.
 * (com.alibaba.rcm) {@code WispResourceContainerFactory} will be the only class
 * need to be exported from package {@code com.alibaba.wisp.engine}.
 */
public final class WispResourceContainerFactory implements ResourceContainerFactory {
    /**
     * @param constraints the target {@code Constraint}s
     * @return Singleton instance of WispResourceContainerFactory
     */
    @Override
    public ResourceContainer createContainer(final Iterable<Constraint> constraints) {
        final ResourceContainer container = WispControlGroup.newInstance().createResourceContainer();
        for (final Constraint constraint : constraints) {
            container.updateConstraint(constraint);
        }
        return container;
    }

    private WispResourceContainerFactory() {
    }

    private static final class Holder {
        private static final WispResourceContainerFactory INSTANCE = new WispResourceContainerFactory();
    }

    /**
     * @param
     * @return Singleton instance of WispResourceContainerFactory
     */
    public static WispResourceContainerFactory instance() {
        return Holder.INSTANCE;
    }
}