package com.alibaba.util;

import com.alibaba.cds.CDSDumperHelper;

import java.io.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.security.PrivilegedAction;
import java.util.*;

public class QuickStart {

    private static final String CONFIG_NAME               = "quickstart.properties";
    private static final String SERVERLESS_ADAPTER_NAME   = "ServerlessAdapter";
    private static final String CDSDUMPER_NAME            = "CDSDumper";

    // serverless adapter stuff
    private static String serverlessAdapter;
    public static void setServerlessAdapter(String serverlessAdapter) {
        QuickStart.serverlessAdapter = serverlessAdapter;
    }
    public static String getServerlessAdapter() {
        return serverlessAdapter;
    }

    private static native void registerNatives();

    static {
        registerNatives();

        loadQuickStartConfig();
    }

    private static void loadQuickStartConfig() {
        // read the quickstart.properties config file
        Properties p = java.security.AccessController.doPrivileged(
                (PrivilegedAction<Properties>) System::getProperties
        );
        Path path = Paths.get(Utils.getJDKHome(), "lib", CONFIG_NAME);
        try (InputStream is = new BufferedInputStream(new FileInputStream(path.toFile()))) {
            p.load(is);
        } catch (Exception e) {
            throw new Error(e);
        }
        CDSDumperHelper.setCdsDumper(p.getProperty(CDSDUMPER_NAME));
        QuickStart.setServerlessAdapter(p.getProperty(SERVERLESS_ADAPTER_NAME));
    }

     /**
      * The enumeration is the same as VM level `enum QuickStart::QuickStartRole`
      */
    public enum QuickStartRole {
        NORMAL(0),
        TRACER(1),
        REPLAYER(2),
        PROFILER(3),
        DUMPER(4);
        int code;

        QuickStartRole(int code) {
            this.code = code;
        }

        public static QuickStartRole getRoleByCode(int code) {
            for (QuickStartRole role : QuickStartRole.values()) {
                if (role.code == code) {
                    return role;
                }
            }
            return null;
        }
    }

    private static QuickStartRole role = QuickStartRole.NORMAL;

    private final static List<Runnable> dumpHooks = new ArrayList<>();

    private static boolean verbose = false;

    public static boolean isVerbose() { return verbose; }

    // JVM will set these fields
    protected static String cachePath;

    protected static String[] vmOptionsInProfileStage;

    protected static String classPathInProfileStage;

    // called by JVM
    private static void initialize(int roleCode, String cachePath, boolean verbose, String[] vmOptionsInProfileStage, String classPathInProfileStage) {
        role = QuickStartRole.getRoleByCode(roleCode);
        QuickStart.cachePath = cachePath;
        QuickStart.verbose = verbose;
        QuickStart.vmOptionsInProfileStage = vmOptionsInProfileStage;
        QuickStart.classPathInProfileStage = classPathInProfileStage;

        if (role == QuickStartRole.TRACER || role == QuickStartRole.PROFILER) {
            Runtime.getRuntime().addShutdownHook(new Thread(QuickStart::notifyDump));
        }
    }

    /**
     * Detect whether this Java process is a normal one.
     * Has the same semantics as VM level `!QuickStart::is_enabled()`
     * @return true if this Java process is a normal process.
     */
    public static boolean isNormal() {
        return role == QuickStartRole.NORMAL;
    }

    /**
     * Detect whether this Java process is a tracer.
     * Has the same semantics as VM level `QuickStart::is_tracer()`
     * @return true if this Java process is a tracer.
     */
    public static boolean isTracer() {
        return role == QuickStartRole.TRACER;
    }

    /**
     * Detect whether this Java process is a replayer.
     * Has the same semantics as VM level `QuickStart::is_replayer()`
     * @return true if this Java process is replayer.
     */
    public static boolean isReplayer() {
        return role == QuickStartRole.REPLAYER;
    }

    public static boolean isDumper() {
        return role == QuickStartRole.DUMPER;
    }

    public static String[] getVmOptionsInProfileStage() {
        return vmOptionsInProfileStage;
    }

    public static String getClassPathInProfileStage() {
        return classPathInProfileStage;
    }

    public static String cachePath() {
        return cachePath;
    }

    public static synchronized void addDumpHook(Runnable runnable) {
        if (notifyCompleted) {
            return;
        }
        dumpHooks.add(runnable);
    }

    public static synchronized void notifyDump(String destCacheDir) {
        notifyDump();
        if (role == QuickStartRole.PROFILER) {
            if (destCacheDir != null && !"".equals(destCacheDir)) {
                doCopy(new File(cachePath()), checkDestDir(destCacheDir));
            }
        }
    }

    private static void doCopy(File srcDir, File destDir) {
        try {
            //the same directory,so no need to do anything.
            if (srcDir.getCanonicalPath().equals(destDir.getCanonicalPath())) {
                return;
            }
            File[] srcFiles = srcDir.listFiles((f) -> f.isFile());
            if (srcFiles != null) {
                for (File file : srcFiles) {
                    Files.copy(file.toPath(), destDir.toPath().resolve(file.getName()),
                            StandardCopyOption.COPY_ATTRIBUTES, StandardCopyOption.REPLACE_EXISTING);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static File checkDestDir(String destCacheDir) {
        File destDir = new File(destCacheDir);
        if (destDir.exists()) {
            if (!destDir.isDirectory()) {
                throw new RuntimeException(destCacheDir + " is not a directory.");
            }
        } else {
            if (!destDir.mkdirs()) {
                throw new RuntimeException("Create directory " + destCacheDir + " failed.");
            }
        }
        return destDir;
    }

    public static synchronized void notifyDump() {
        if (notifyCompleted) {
            return;
        }
        for (Runnable dumpHook : dumpHooks) {
            dumpHook.run();
        }
        notifyDump0();
        notifyCompleted = true;
    }

    private static boolean notifyCompleted = false;

    private static native void notifyDump0();
}
