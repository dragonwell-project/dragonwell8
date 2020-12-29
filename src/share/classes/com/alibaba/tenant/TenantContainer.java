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
 */

package com.alibaba.tenant;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.LinkedList;
import java.util.Map;
import java.util.Properties;
import java.util.WeakHashMap;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Supplier;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.internal.AbstractResourceContainer;
import sun.misc.SharedSecrets;
import sun.misc.TenantAccess;
import static com.alibaba.tenant.TenantState.*;


/**
 * TenantContainer is a "virtual container" for a tenant of application, the
 * resource consumption of tenant such as CPU, heap is constrained by the policy
 * of this "virtual container". The thread can run in virtual container by
 * calling <code>TenantContainer.run</code>
 *
 */
public class TenantContainer {

    TenantResourceContainer resourceContainer;

    private static NativeDispatcher nd = new NativeDispatcher();

    /*
     * Used to generate the tenant id.
     */
    private static AtomicLong nextTenantID = new AtomicLong(0);

    /*
     * Used to hold the mapping from tenant id to TenantContainer object for all
     * tenants
     */
    private static Map<Long, TenantContainer> tenantContainerMap = null;

    /*
     * Holds the threads attached with this tenant container
     */
    private List<Thread> attachedThreads = new LinkedList<>();

    /*
     * Newly created threads which attach to this tenant container
     */
    private List<WeakReference<Thread>> spawnedThreads = Collections.synchronizedList(new ArrayList<>());

    /*
     * Used to contain service threads, including finalizer threads, shutdown hook threads.
     */
    private Map<Thread, Void> serviceThreads = Collections.synchronizedMap(new WeakHashMap<>());

    /*
     * the configuration of this tenant container
     */
    private TenantConfiguration configuration = null;

    /*
     * tenant state
     */
    private volatile TenantState state;

    /*
     * tenant id
     */
    private long tenantId;

    /*
     * address of native tenant allocation context
     */
    private long allocationContext = 0L;

    /*
     * tenant name
     */
    private String name;

    /*
     * Used to store the system properties per tenant
     */
    private Properties props;

    /*
     * allocated memory of attached threads, which is accumulated for current tenant
     */
    private long accumulatedMemory = 0L;

    /**
     * Get total allocated memory of this tenant.
     * @return the total allocated memory of this tenant.
     */
    public synchronized long getAllocatedMemory() {
        Thread[] threads = getAttachedThreads();
        int size         = threads.length;
        long[] ids       = new long[size];
        long[] memSizes  = new long[size];

        for (int i = 0; i < size; i++) {
            ids[i] = threads[i].getId();
        }

        nd.getThreadsAllocatedMemory(ids, memSizes);

        long totalThreadsAllocatedMemory = 0;
        for (long s : memSizes) {
            totalThreadsAllocatedMemory += s;
        }
        return totalThreadsAllocatedMemory + accumulatedMemory;
    }

    /**
     * Data repository used to store the data isolated per tenant.
     */
    private TenantData tenantData = new TenantData();

    /**
     * Used to track and run tenant shutdown hooks
     */
    private TenantShutdownHooks tenantShutdownHooks = new TenantShutdownHooks();

    /**
     * Retrieves the data repository used by this tenant.
     * @return the data repository associated with this tenant.
     */
    public TenantData getTenantData() {
        return tenantData;
    }

    /**
     * Sets the tenant properties to the one specified by argument.
     * @param props the properties to be set, CoW the system properties if it is null.
     */
    public void setProperties(Properties props) {
        if (props == null) {
            props = new Properties();
            Properties sysProps = System.getProperties();
            for(Object key: sysProps.keySet()) {
                props.put(key, sysProps.get(key));
            }
        }
        this.props = props;
    }

    /**
     * Gets the properties of tenant
     * @return the tenant properties
     */
    public Properties getProperties() {
        return props;
    }

    /**
     * Sets the property indicated by the specified key.
     * @param  key the name of the property.
     * @param  value the value of the property.
     * @return the previous value of the property,
     *         or null if it did not have one.
     */
    public String setProperty(String key, String value) {
        checkKey(key);
        return (String) props.setProperty(key, value);
    }

    /**
     * Gets the property indicated by the specified key.
     * @param  key  the name of the property.
     * @return the  string value of the property,
     *         or null if there is no property with that key.
     */
    public String getProperty(String key) {
        checkKey(key);
        return props.getProperty(key);
    }

    /**
     * Removes the property indicated by the specified key.
     * @param  key  the name of the property to be removed.
     * @return the  previous string value of the property,
     *         or null if there was no property with that key.
     */
    public String clearProperty(String key) {
        checkKey(key);
        return (String) props.remove(key);
    }

    private void checkKey(String key) {
        if (null == key) {
            throw new NullPointerException("key can't be null");
        }
        if ("".equals(key)) {
            throw new IllegalArgumentException("key can't be empty");
        }
    }

    //
    // Used to synchronize between destroy() and runThread()
    private ReentrantReadWriteLock destroyLock = new ReentrantReadWriteLock();


    /**
     * Destroy this tenant container and release occupied resources including memory, cpu, FD, etc.
     *
     */
    public void destroy() {
        if (TenantContainer.current() != null) {
            throw new RuntimeException("Should only call destroy() in ROOT tenant");
        }

        destroyLock.writeLock().lock();
        try {
            if (state != TenantState.STOPPING && state != TenantState.DEAD) {
                setState(TenantState.STOPPING);

                tenantContainerMap.remove(getTenantId());

                // finish all finalizers
                resourceContainer.attach();
                nd.attach(this);
                try {
                    Runtime.getRuntime().runFinalization();
                } finally {
                    nd.attach(null);
                    resourceContainer.detach();
                }

                // execute all shutdown hooks
                tenantShutdownHooks.runHooks();
            }
        } catch (Throwable t) {
            System.err.println("Exception from TenantContainer.destroy()");
            t.printStackTrace();
        } finally {
            setState(TenantState.DEAD);
            cleanUp();
            destroyLock.writeLock().unlock();
        }
    }

    /*
     * Release all native resources and Java references
     * should be the very last step of {@link #destroy()} operation.
     * If cannot kill all threads in {@link #killAllThreads()}, should do this in {@link WatchDogThread}
     *
     */
    private void cleanUp() {
        if (TenantGlobals.isHeapIsolationEnabled()) {
            nd.destroyTenantAllocationContext(allocationContext);
        }

        resourceContainer.destroyImpl();
        // clear references
        spawnedThreads.clear();
        attachedThreads.clear();
        tenantData.clear();
        tenantShutdownHooks = null;
    }

    private TenantContainer(TenantContainer parent, String name, TenantConfiguration configuration) {
        this.tenantId = nextTenantID.getAndIncrement();
        this.resourceContainer = new TenantResourceContainer(
                (parent == null ? null : parent.resourceContainer),
                this,
                configuration.getAllConstraints());
        this.name = (name == null ? "Tenant-" + getTenantId() : name);
        this.configuration = configuration;

        setState(STARTING);

        //Initialize the tenant properties.
        props = new Properties();
        props.putAll(System.getProperties());
        tenantContainerMap.put(this.tenantId, this);

        // Create allocation context if heap isolation enabled
        if (TenantGlobals.isHeapIsolationEnabled()) {
            nd.createTenantAllocationContext(this, configuration.getMaxHeap());
        }
    }

    TenantConfiguration getConfiguration() {
        return configuration;
    }

    /**
     * @return the tenant state
     */
    public TenantState getState() {
        return state;
    }

    /*
     * Set the tenant state
     * @param state used to set
     */
    void setState(TenantState state) {
        this.state = state;
    }

    /**
     * Returns the tenant' id
     * @return tenant id
     */
    public long getTenantId() {
        return tenantId;
    }

    /**
     * Returns this tenant's name.
     * @return this tenant's name.
     */
    public String getName() {
        return name;
    }

    /**
     * @return A collection of all threads attached to the container.
     */
    public synchronized Thread[] getAttachedThreads() {
        return attachedThreads.toArray(new Thread[attachedThreads.size()]);
    }

    /**
     * Get the tenant container by id
     * @param id tenant id.
     * @return the tenant specified by id, null if the id doesn't exist.
     */
    public static TenantContainer getTenantContainerById(long id) {
        checkIfTenantIsEnabled();
        return tenantContainerMap.get(id);
    }

    /**
     * Create tenant container by the configuration
     * @param configuration used to create tenant
     * @return the tenant container
     */
    public static TenantContainer create(TenantConfiguration configuration) {
        return create(TenantContainer.current(), configuration);
    }

    /**
     * Create tenant container by the configuration
     * @param parent parent tenant container
     * @param configuration used to create tenant
     * @return the tenant container
     */
    public static TenantContainer create(TenantContainer parent,
                                         TenantConfiguration configuration) {
        checkIfTenantIsEnabled();
        //parameter checking
        if (null == configuration) {
            throw new IllegalArgumentException("Failed to create tenant, illegal arguments: configuration is null");
        }

        return create(parent, null, configuration);
    }
    /**
     * Create tenant container by the name and configuration
     * @param name the tenant name
     * @param configuration used to create tenant
     * @return the tenant container
     */
    public static TenantContainer create(String name, TenantConfiguration configuration) {
        return create(TenantContainer.current(), name, configuration);
    }

    /**
     * Create tenant container by the name and configuration
     * @param parent parent tenant container
     * @param name the tenant name
     * @param configuration used to create tenant
     * @return the tenant container
     */
    public static TenantContainer create(TenantContainer parent, String name, TenantConfiguration configuration) {
        checkIfTenantIsEnabled();
        //parameter checking
        if (null == configuration) {
            throw new IllegalArgumentException("Failed to create tenant, illegal arguments: configuration is null");
        }

        TenantContainer tc = new TenantContainer(parent, name, configuration);
        tenantContainerMap.put(tc.getTenantId(), tc);
        return tc;
    }

    /**
     * Gets the tenant id list
     * @return the tenant id list, Collections.emptyList if no tenant exists.
     */
    public static List<Long> getAllTenantIds() {
        checkIfTenantIsEnabled();
        if (null == tenantContainerMap) {
            throw new IllegalStateException("TenantContainer class is not initialized !");
        }
        if (tenantContainerMap.size() == 0) {
            return Collections.EMPTY_LIST;
        }

        return new ArrayList<>(tenantContainerMap.keySet());
    }

    /**
     * Gets the TenantContainer attached to the current thread.
     * @return The TenantContainer attached to the current thread, null if no
     *         TenantContainer is attached to the current thread.
     */
    public static TenantContainer current() {
        checkIfTenantIsEnabled();
        AbstractResourceContainer curResContainer = TenantResourceContainer.current();
        if (TenantResourceContainer.root() == curResContainer) {
            return null;
        }
        assert curResContainer instanceof TenantResourceContainer;
        return ((TenantResourceContainer)curResContainer).getTenant();
    }

    /**
     * Gets the cpu time consumed by this tenant
     * @return the cpu time used by this tenant, 0 if tenant cpu throttling or accounting feature is disabled.
     */
    public long getProcessCpuTime() {
        if (!TenantGlobals.isCpuAccountingEnabled()) {
            throw new IllegalStateException("-XX:+TenantCpuAccounting is not enabled");
        }
        long cpuTime = 0;
        return cpuTime;
    }

    /**
     * Gets the heap space occupied by this tenant
     * @return heap space occupied by this tenant, 0 if tenant heap isolation is disabled.
     * @throws IllegalStateException if -XX:+TenantHeapIsolation is not enabled.
     */
    public long getOccupiedMemory() {
        if (!TenantGlobals.isHeapIsolationEnabled()) {
            throw new IllegalStateException("-XX:+TenantHeapIsolation is not enabled");
        }
        return nd.getTenantOccupiedMemory(allocationContext);
    }

    /**
     * Runs the code in the target tenant container
     * @param task the code to run
     */
    public void run(Runnable task) throws TenantException {
        if (getState() == DEAD || getState() == STOPPING) {
            throw new TenantException("Tenant is dead");
        }
        TenantContainer container = current();
        if (container == this) {
            task.run();
        } else {
            if (container != null) {
                throw new TenantException("must be in root container " +
                        "before running into non-root container.");
            }
            attach();
            try {
                task.run();
            } finally {
                detach();
            }
        }
    }

    /*
     * Get accumulatedMemory value of current thread
     */
    private long getThreadAllocatedMemory() {
        long[] memSizes = new long[1];
        nd.getThreadsAllocatedMemory(null, memSizes);
        return memSizes[0];
    }

    private void attach() {
        // This is the first thread which runs in this tenant container
        if (getState() == TenantState.STARTING) {
            // move the tenant state to RUNNING
            this.setState(TenantState.RUNNING);
        }

        Thread curThread = Thread.currentThread();

        long curAllocBytes = getThreadAllocatedMemory();

        synchronized (this) {
            attachedThreads.add(curThread);

            accumulatedMemory -= curAllocBytes;

            resourceContainer.attach();

            nd.attach(this);
        }
    }

    private void detach() {
        Thread curThread = Thread.currentThread();

        long curAllocBytes = getThreadAllocatedMemory();

        synchronized (this) {
            nd.attach(null);

            resourceContainer.detach();

            attachedThreads.remove(curThread);

            accumulatedMemory += curAllocBytes;
        }
    }

    /*
     * Check if the tenant feature is enabled.
     */
    private static void checkIfTenantIsEnabled() {
        if (!TenantGlobals.isTenantEnabled()) {
            throw new UnsupportedOperationException("The multi-tenant feature is not enabled!");
        }
    }

    /*
     * Invoked by the VM to run a thread in multi-tenant mode.
     *
     * NOTE: please ensure relevant logic has been fully understood before changing any code
     *
     * @throws TenantException
     */
    private void runThread(final Thread thread) throws TenantException {
        if (destroyLock.readLock().tryLock()) {
            if (getState() != STOPPING && getState() != DEAD) {
                spawnedThreads.add(new WeakReference<>(thread));
                this.run(() -> {
                    destroyLock.readLock().unlock();
                    thread.run();
                });
            } else {
                destroyLock.readLock().unlock();
            }

            // try to clean up once
            if (destroyLock.readLock().tryLock()) {
                if (getState() != STOPPING && getState() != DEAD) {
                    spawnedThreads.removeIf(ref -> ref.get() == null || ref.get() == thread);
                }
                destroyLock.readLock().unlock();
            }
        } else {
            // shutdown in progress
            if (serviceThreads.containsKey(thread)) {
                // attach to current thread to run without registering
                resourceContainer.attach();
                nd.attach(this);
                try {
                    thread.run();
                } finally {
                    nd.attach(null);
                    resourceContainer.detach();
                    removeServiceThread(thread);
                }
            }
        }
    }

    /*
     * Initialize the TenantContainer class, called after System.initializeSystemClass by VM.
     */
    private static void initializeTenantContainerClass() {
        //Initialize this field after the system is booted.
        tenantContainerMap = Collections.synchronizedMap(new HashMap());

        // initialize TenantAccess
        if (SharedSecrets.getTenantAccess() == null) {
            SharedSecrets.setTenantAccess(new TenantAccess() {
                @Override
                public void registerServiceThread(TenantContainer tenant, Thread thread) {
                    if (tenant != null && thread != null) {
                        tenant.addServiceThread(thread);
                    }
                }
            });
        }

        try {
            // force initialization of TenantConfiguration
            Class.forName("com.alibaba.tenant.TenantConfiguration");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
    }

    /**
     * Retrieve the tenant container where <code>obj</code> is allocated in
     * @param obj    object to be searched
     * @return       TenantContainer object whose memory space contains <code>obj</code>,
     *               or null if ROOT tenant container
     */
    public static TenantContainer containerOf(Object obj) {
        if (!TenantGlobals.isHeapIsolationEnabled()) {
            throw new UnsupportedOperationException("containerOf() only works with -XX:+TenantHeapIsolation");
        }
        return obj != null ? nd.containerOf(obj) : null;
    }

    /**
     * Gets the field value stored in the data repository of this tenant, which is same to call the
     * {@code TenantData.getFieldValue} on the tenant data object retrieved by {@code TenantContainer.getTenantData}.
     *
     * @param obj           Object the field associates with
     * @param fieldName     Field name
     * @param supplier      Responsible for creating the initial field value
     * @return              Value of field.
     */
    public <K, T> T getFieldValue(K obj, String fieldName, Supplier<T> supplier) {
        return tenantData.getFieldValue(obj, fieldName, supplier);
    }

    /**
     * Gets the field value stored in the data repository of this tenant, which is same to call the
     * {@code TenantData.getFieldValue} on the tenant data object retrieved by {@code TenantContainer.getTenantData}.
     *
     * @param obj           Object the field associates with
     * @param fieldName     Field name
     * @return              Value of field, null if not found
     */
    public <K, T> T getFieldValue(K obj, String fieldName) {
        return getFieldValue(obj, fieldName, () -> null);
    }

    /**
     * Runs {@code Supplier.get} in the root tenant.
     * @param supplier target used to call
     * @return the result of {@code Supplier.get}
     */
    public static <T> T primitiveRunInRoot(Supplier<T> supplier) {
        // thread is already in root tenant.
        if(null == TenantContainer.current()) {
            return supplier.get();
        } else{
            TenantContainer tenant = TenantContainer.current();
            //Force to root tenant.
            tenant.resourceContainer.detach();
            nd.attach(null);
            try {
                T t = supplier.get();
                return t;
            } finally {
                nd.attach(tenant);
                tenant.resourceContainer.attach();
            }
        }
    }

    /**
     * Runs a block of code in the root tenant.
     * @param runnable the code to run
     */
    public static void primitiveRunInRoot(Runnable runnable) {
        primitiveRunInRoot(() -> {
            runnable.run();
            return null;
        });
    }

    /**
     * Register a new tenant shutdown hook.
     * When the tenant begins its destroy it will
     * start all registered shutdown hooks in some unspecified order and let
     * them run concurrently.
     * @param   hook
     *          An initialized but unstarted <tt>{@link Thread}</tt> object
     */
    public void addShutdownHook(Thread hook) {
        addServiceThread(hook);
        tenantShutdownHooks.add(hook);
    }

    /**
     * De-registers a previously-registered tenant shutdown hook.
     * @param hook the hook to remove
     * @return true if the specified hook had previously been
     * registered and was successfully de-registered, false
     * otherwise.
     */
    public boolean removeShutdownHook(Thread hook) {
        removeServiceThread(hook);
        return tenantShutdownHooks.remove(hook);
    }

    // add a thread to the service thread list
    private void addServiceThread(Thread thread) {
        if (thread != null) {
            serviceThreads.put(thread, null);
        }
    }

    // remove a thread from the service thread list
    private void removeServiceThread(Thread thread) {
        serviceThreads.remove(thread);
    }

    /**
     * Try to modify resource limit of current tenant,
     * for resource whose limit cannot be changed after creation of {@code TenantContainer}, its limit will be ignored.
     * @param config  new TenantConfiguration to
     */
    public void update(TenantConfiguration config) {
        for (Constraint constraint : config.getAllConstraints()) {
            updateConstraint(constraint);
        }
    }

    void updateConstraint(Constraint constraint) {
        resourceContainer.updateConstraint(constraint);
        getConfiguration().setConstraint(constraint);
    }

    /**
     * @return {@code ResourceContainer} of this tenant
     */
    public ResourceContainer getResourceContainer() {
        return resourceContainer;
    }
}

