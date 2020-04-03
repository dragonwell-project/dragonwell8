package sun.misc;

import com.alibaba.tenant.TenantContainer;

public interface TenantAccess {

    /**
     * Register a thread to {@code tenant}'s service thread list.
     * At present, a service thread is either a registered shutdown hook thread or Finalizer thread.
     *
     * @param tenant  The teant container to register thread with
     * @param thread  Thread to be registered
     */
    void registerServiceThread(TenantContainer tenant, Thread thread);
}
