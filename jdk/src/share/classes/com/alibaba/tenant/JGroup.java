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

import com.alibaba.rcm.Constraint;
import sun.security.action.GetPropertyAction;
import java.io.File;
import java.io.IOException;
import java.nio.file.FileVisitOption;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.AccessController;
import java.util.ArrayList;
import java.util.List;
import static com.alibaba.tenant.NativeDispatcher.*;

/**
 * JGroup is a java mirror of Linux control group
 */
class JGroup {

    // to dispatch calls to libcgroup and file system
    private static NativeDispatcher nd;

    /*
     * The cgroup hierarchy is:
     *
     * OS/Container ROOT group
     * |
     * |__ JDK Group
     *     |
     *     |__ JVM group of process 1
     *     |    |___ TenantContainer1's group
     *     |    |___ TenantContainer2's group
     *     |    |___ ...
     *     |
     *     |__ JVM group of process 2
     *     |
     *     |__ ...
     *
     * The cgroup path identifying TenantContainer 't0' is in following format
     * /<cgroup mountpoint>/<controller name>/<optional user specified ROOT path/<JDK path>/<JVM path>/<TenantContainer group path>
     *
     * e.g.
     * /sys/fs/cgroup/cpuset,cpu,cpuacct/ajdk_multi_tenant/12345/t0
     * /sys/fs/cgroup/cpuset/system.slice/docker-0fdsnjk23njkfnkfnwe/ajdk_multi_tenant/12345/t0
     *
     */

    // OS/Container ROOT group, will be appended to each cgroup controller's root path
    static final String ROOT_GROUP_PATH;

    // JDK group must be initialized before running any JVM process with CPU throttling/accounting enabled
    static final String JDK_GROUP_PATH;

    // Current JVM process's cgroup path, parent of all non-ROOT TenantContainers' groups
    static final String JVM_GROUP_PATH;

    // deprecated constants
    static final int DEFAULT_WEIGHT = 1024;

    static final int DEFAULT_MAXCPU = 100;

    // unique top level groups
    // rootGroup and jdkGroup are static, just to serve 'parent' API
    private static JGroup rootGroup;
    private static JGroup jdkGroup;
    // jvm group is the top level active cgroup
    private static JGroup jvmGroup;

    static JGroup jvmGroup() {
        return jvmGroup;
    }

    static JGroup jdkGroup() {
        return jdkGroup;
    }

    static JGroup rootGroup() {
        return rootGroup;
    }

    /*
     * CGroup path of this cgroup
     */
    private String groupPath;

    /*
     * parent group
     */
    private JGroup parentGroup;

    // corresponding TenantResourceContainer object
    private TenantResourceContainer resourceContainer;

    // Create a JGroup and initialize necessary underlying cgroup configurations
    JGroup(TenantResourceContainer container) {
        if (container == null) {
            throw new IllegalArgumentException("Must provide a non-null TenantContainer to start with");
        }
        this.resourceContainer = container;
        assert resourceContainer.getTenant() != null;
        init(resourceContainer, getTenantGroupPath(resourceContainer));
    }

    //
    // only directly called by initializeJGroupClass to initialize jvmGroup and jdkGroup
    //
    private JGroup(TenantResourceContainer container, String path) {
        init(container, path);
    }

    private void init(TenantResourceContainer container, String path) {
        this.groupPath = path;
        if (container == null) {
            this.parentGroup = null;
            if (JVM_GROUP_PATH.equals(path)) {
                this.parentGroup = jdkGroup();
                initSystemGroup(this);
            } else if (JDK_GROUP_PATH.equals(path)) {
                this.parentGroup = rootGroup();
                // JDK group has already been initialized by JGroupMain
            } else if (!ROOT_GROUP_PATH.equals(path)) {
                System.err.println("Should not call this with path=" + path);
            }
        } else {
            this.parentGroup = container.getParent() == null ? jvmGroup() : container.getParent().getJGroup();
            initUserGroup(this, container.getConstraints());
        }
    }

    //
    // To generate group path for non-ROOT tenant containers
    // current pattern:
    //      "JVM_GROUP_PATH/t<tenant id>"
    //
    private String getTenantGroupPath(TenantResourceContainer container) {
        TenantResourceContainer parent = container.getParent();
        return (parent == null ? JVM_GROUP_PATH : parent.getJGroup().groupPath)
                + File.separator + "t" + container.getTenant().getTenantId();
    }

    // returns the relative cgroup path of this JGroup
    private String groupPath() {
        return groupPath;
    }

    // returns parent JGroup of this one, null if current is the top most JGroup
    JGroup parent() {
        return parentGroup;
    }

    /*
     * Destory this jgroup
     */
    void destory() {
        Path parentTasks = configPathOf(parent(), CG_TASKS);
        try {
            // move all tasks of children group to parent!
            Files.walk(Paths.get(rootPathOf(CG_CPU), groupPath()), FileVisitOption.FOLLOW_LINKS)
                    .filter(path -> path.endsWith(CG_TASKS))
                    .forEach(path -> {
                        try {
                            for (String pidLine : Files.readAllLines(path)) {
                                Files.write(parentTasks, pidLine.getBytes());
                                debug("destroyCgroup: move " + pidLine + " from " + path
                                        + " to " + parentTasks);
                            }
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    });

            // delete all directories
            Files.walk(Paths.get(rootPathOf(CG_CPU), groupPath()), FileVisitOption.FOLLOW_LINKS)
                    .map(Path::toFile)
                    .filter(File::isDirectory)
                    .forEach(File::delete);

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    // Initialize a cgroup with user specified limits after JVM bootstrapping,
    // for non-ROOT tenant containers
    private void initUserGroup(JGroup jgroup, Iterable<Constraint> constraints) {
        try {
            initCgroupCommon(jgroup);

            // set value with user specified values
            // ignoring non-cgroup resources, like ResourceType.MEMORY
            if (constraints != null) {
                for (Constraint c : constraints) {
                    if (c.getResourceType() instanceof TenantResourceType) {
                        TenantResourceType type = (TenantResourceType)c.getResourceType();
                        if (type.isJGroupResource()) {
                            ((JGroupConstraint)c).sync(jgroup);
                        }
                    }
                }
            }
        } catch (RuntimeException e) {
            throw e;
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    // Initialize system cgroups, used by JGroup#initializeJGroupClass
    // during JVM initialization to create the JDK and JVM group.
    // NOTE: never throw exception here, will cause VM code to crash, very rude!
    private void initSystemGroup(JGroup jgroup) {
        try {
            initCgroupCommon(jgroup);
        } catch (Throwable t) {
            t.printStackTrace();
            System.exit(127);
        }
    }

    // create and initialize underlying cgroups
    private void initCgroupCommon(JGroup jgroup) {
        if (jgroup == null || jgroup.groupPath() == null
                || jgroup.groupPath().isEmpty()) {
            throw new IllegalArgumentException("Bad argument to createCGroup()");
        }

        if (nd.createCgroup(jgroup.groupPath()) == 0) {
            // init two mandatory fields
            if (IS_CPUSET_ENABLED) {
                copyValueFromParentGroup(jgroup, CG_CPUSET_CPUS);
                copyValueFromParentGroup(jgroup, CG_CPUSET_MEMS);
            }
            debug("Created group with standard controllers");
        } else {
            throw new RuntimeException("Failed to create cgroup at " + jgroup.groupPath());
        }
    }

    // Copy config value from parent group
    private void copyValueFromParentGroup(JGroup jgroup, String configName) {
        String parentValue = getValue(jgroup.parent(), configName);
        setValue(jgroup, configName, parentValue);
        debug("Set group " + jgroup.groupPath()+ "'s config " + configName
                + " with parent group's value: " + parentValue);
    }


    // Not a frequent operation, using Java implementation
    private String getValue(JGroup group, String key) {
        if (group == null || group.groupPath() == null || group.groupPath().isEmpty()
                || key == null || key.isEmpty() || !key.contains(".")) {
            return null;
        }
        String ctrlName = key.split("\\.")[0];
        Path configPath = Paths.get(rootPathOf(ctrlName), group.groupPath(), key);
        if (Files.exists(configPath)) {
            try {
                String value = new String(Files.readAllBytes(configPath)).trim();
                debug(key + "=" + value);
                return value;
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        } else {
            debug("getValue: config path " + configPath + " does not exist");
        }
        return null;
    }

    // Not a frequent operation, using Java implementation
    private void setValue(JGroup group, String key, String value) {
        if (group == null || group.groupPath() == null || group.groupPath().isEmpty()
                || key == null || key.isEmpty()
                || value == null || value.isEmpty()
                || !key.contains(".")) {
            throw new IllegalArgumentException("Cannot set " + group.groupPath()
                    + File.separator + key + "=" + value);
        }
        String controllerName = key.split("\\.")[0];
        Path configPath = Paths.get(rootPathOf(controllerName), group.groupPath(), key);

        if (Files.exists(configPath)) {
            try {
                debug("Set value: " + configPath + "=" + value);
                Files.write(configPath, value.getBytes());
            } catch (IOException e) {
                throw new RuntimeException("Failed to set " + group.groupPath()
                        + File.separator + key + " = " + value, e);
            }
        } else {
            debug("setValue: config path " + configPath + " does not exist");
        }
    }

    // used by injected test case
    static String rootPathOf(String controllerName) {
        switch (controllerName) {
            case CG_CPU     : return CG_MP_CPU;
            case CG_CPUSET  : return CG_MP_CPUSET;
            case CG_CPUACCT : return CG_MP_CPUACCT;
            default:
                throw new IllegalArgumentException("Unsupported controller : " + controllerName);
        }
    }

    private static void debug(String... messages) {
        if (debugJGroup) {
            System.out.print("[JGroupDispatcher] ");
            for (String msg : messages) {
                System.out.print(msg);
                System.out.print(" ");
            }
            System.out.println();
        }
    }

    /*
     * Attach the current thread into this jgroup.
     * @return 0 if successful
     */
    void attach() {
        if (nd.moveToCgroup(groupPath()) != 0) {
            throw new IllegalStateException("Cannot attach to group " + this);
        } else {
            debug("Attached to cgroup: " + groupPath());
        }
    }

    /**
     * Detach the current thread from this jgroup
     * @return 0 if successful
     */
    void detach() {
        jvmGroup().attach();
    }

    /**
     * Get cpuacct usage of this group
     * @return cpu usage in nano seconds
     */
    long getCpuTime() {
        try {
            return Long.parseLong(getValue(this, "cpuacct.usage"));
        } catch (Exception e) {
            System.err.println("Exception from JGroup.getCpuTime()");
            e.printStackTrace();
        }
        return -1;
    }

    /*
     * Set the value of a cgroup attribute
     */
    synchronized void setValue(String key, String name) {
        setValue(this, key, name);
    }

    /*
     * Get the value of a cgroup attribute
     */
    synchronized String getValue(String key) {
        return getValue(this, key);
    }

    /*
     * Initialize the JGroup class, called in create_vm.
     */
    private static void initializeJGroupClass() {
        try {
            // NOTE: below sequence should not be changed!
            rootGroup = new JGroup(null, ROOT_GROUP_PATH);
            jdkGroup = new JGroup(null, JDK_GROUP_PATH);
            jvmGroup = new JGroup(null, JVM_GROUP_PATH);

            // check before attach
            checkRootGroupPath();

            jvmGroup().attach();
        } catch (Throwable t) {
            System.err.println("Failed to initialize JGroup");
            t.printStackTrace();
            // do not throw any exception during VM initialization, but just exit
            System.exit(128);
        }
    }

    // Check if mandatory root group paths exist
    private static void checkRootGroupPath() {
        List<String> controllers = new ArrayList<>(4);
        if (TenantGlobals.isCpuThrottlingEnabled()) {
            controllers.add("cpu");
            controllers.add("cpuset");
        }
        if (TenantGlobals.isCpuAccountingEnabled()) {
            controllers.add("cpuacct");
        }
        for (String ctrl : controllers) {
            String r = rootPathOf(ctrl);
            if (r == null || !Files.exists(Paths.get(r, ROOT_GROUP_PATH))) {
                throw new IllegalArgumentException("Bad ROOT group path: " + r + File.separator + ROOT_GROUP_PATH);
            }
        }
    }

    private static Path configPathOf(JGroup group, String config) {
        String rootPath = null;
        try {
            rootPath = rootPathOf(config.split("\\.")[0]);
        } catch (IllegalArgumentException e) {
            // using 'cpu' group root as default
            rootPath = rootPathOf("cpu");
        }
        return Paths.get(rootPath, group.groupPath(), config);
    }

    /*
     * Destory the JGroup class, called in destroy_vm.
     */
    private static void destroyJGroupClass() {
        jvmGroup.destory();
        jvmGroup = null;
    }

    // --- debugging support ---
    private static boolean debugJGroup;
    private static final String DEBUG_JGROUP_PROP = "com.alibaba.tenant.debugJGroup";

    // property name of user specified ROOT cgroup path, relative to each controller's ROOT path
    private static final String PROP_ROOT_GROUP = "com.alibaba.tenant.jgroup.rootGroup";

    // property name of user specified JDK group path, relative to ROOT_GROUP_PATH
    private static final String PROP_JDK_GROUP = "com.alibaba.tenant.jgroup.jdkGroup";

    // default value of property PROP_JDK_GROUP
    private static final String DEFAULT_JDK_GROUP_NAME = "ajdk_multi_tenant";

    // default value of ROOT_GROUP_PATH
    private static final String DEFAULT_ROOT_GROUP_PATH = "/";

    static {
        nd = new NativeDispatcher();

        String prop = System.getProperty(PROP_ROOT_GROUP);
        if (prop == null) {
            ROOT_GROUP_PATH = DEFAULT_ROOT_GROUP_PATH;
        } else {
            ROOT_GROUP_PATH = prop;
        }

        prop = System.getProperty(PROP_JDK_GROUP);
        if (prop == null) {
            JDK_GROUP_PATH = ROOT_GROUP_PATH + File.separator + DEFAULT_JDK_GROUP_NAME;
        } else {
            JDK_GROUP_PATH = ROOT_GROUP_PATH + File.separator + prop;
        }

        JVM_GROUP_PATH = JDK_GROUP_PATH + File.separator + nd.getProcessId();

        debugJGroup = Boolean.parseBoolean(
                AccessController.doPrivileged(new GetPropertyAction(DEBUG_JGROUP_PROP)));
    }
}
