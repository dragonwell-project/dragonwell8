package com.alibaba.rcm.internal;

import com.alibaba.tenant.TenantGlobals;

import java.security.AccessController;
import java.security.PrivilegedAction;

public final class ResourceContainerGlobals {

    private final static String PROPERTY_ISOLATION_KEY = "com.alibaba.resourceContainer.propertyIsolation";

    private final static boolean PROPERTY_ISOLATION_ENABLED =
            AccessController.doPrivileged(new PrivilegedAction<Boolean>() {
                @Override
                public Boolean run() {
                    return Boolean.valueOf(System.getProperty(PROPERTY_ISOLATION_KEY, "false"));
                }
            });

    /**
     * Returns whether system property isolation is enabled
     */
    public static boolean propertyIsolationEnabled() {
        return PROPERTY_ISOLATION_ENABLED || TenantGlobals.isDataIsolationEnabled();
    }
}
