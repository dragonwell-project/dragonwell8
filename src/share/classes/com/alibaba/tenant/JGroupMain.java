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

import sun.security.action.GetPropertyAction;
import javax.tools.ToolProvider;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.AccessController;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Launcher of the jgroup configuration tools
 * usually invoked during JDK installation.
 * <p>
 * NOTE: Privileged user permission (e.g. sudo, su) is needed!
 */
public class JGroupMain {

    /*
     * name of JGroup's initializer resource stored in jdk/tools.jar.
     * The content of the resource file will be loaded from jdk/tools.jar and writen out to a temp shell script file,
     * finally the temp shell script will be executed.
     */
    private static final String INITIALIZER_NAME = "com/alibaba/tenant/JGroupInitializer.sh";

    /*
     * Wait {@code TIMEOUT_MILLIS} for child process to end
     */
    private static final long TIMEOUT_MILLIS = 60_000;

    private static final String SCRIPT_PREFIX = "jgroupInit_";

    public static void main(String[] args) {
        try {
            new JGroupMain().doInitilization(args);
        } catch (Exception e) {
            throw (e instanceof RuntimeException ? (RuntimeException)e : new RuntimeException(e));
        }
    }

    // Check if current process has permission to init jgroup
    private void checkPermission() {
        // check platform
        String osName = AccessController.doPrivileged(new GetPropertyAction("os.name"));
        if (!"Linux".equalsIgnoreCase(osName)) {
            throw new RuntimeException("Only supports Linux platform");
        }

        // check permission of writing system files
        File file = new File("/etc");
        if (!file.canWrite()) {
            throw new SecurityException("Permission denied! need WRITE permission on system files");
        }
    }

    // do initialization work!
    private void doInitilization(String[] args)
            throws TimeoutException, IOException {
        // pre-check
        checkPermission();

        // create a temporary shell script to do primary initialization work
        String scriptAbsPath = generateInitScript();

        // execute the script file to initialize jgroup in a child process
        List<String> arguments = new ArrayList<>(1 + (args == null ? 0 : args.length));
        arguments.add(scriptAbsPath);
        for (String arg : args) {
            arguments.add(arg);
        }
        ProcessBuilder pb = new ProcessBuilder(arguments);
        pb.redirectOutput(ProcessBuilder.Redirect.INHERIT);
        pb.redirectError(ProcessBuilder.Redirect.INHERIT);
        Process process = pb.start();
        if (process == null) {
            throw new RuntimeException("Failed to launch initializer process!");
        }

        // wait with timeout for child process to terminate
        try {
            process.waitFor();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        if (process.isAlive()) {
            process.destroyForcibly();
            throw new TimeoutException("ERROR: Initializer process does not finish in " + TIMEOUT_MILLIS + "ms!");
        }

        int retValue = process.exitValue();
        if (retValue != 0) {
            throw new IllegalStateException("Bad exit value from initialization process: " + retValue);
        }
    }

    private String generateInitScript() throws IOException {
        // load script content
        ClassLoader loader = ToolProvider.getSystemToolClassLoader(); // JGroupInitializer.sh is in jdk/lib/tools.jar
        InputStream stream = loader.getResourceAsStream(INITIALIZER_NAME);

        if (stream == null) {
            throw new IOException("Cannot load " + INITIALIZER_NAME + " using system classloader");
        }

        File script = File.createTempFile(SCRIPT_PREFIX, ".sh");
        script.setExecutable(true);
        script.setWritable(true);
        script.deleteOnExit();

        // copy content of initializer resource to a temp shell script file
        OutputStream fos = new FileOutputStream(script);
        int buf = 0;
        while ((buf = stream.read()) != -1) {
            fos.write(buf);
        }
        fos.close();

        return script.getAbsolutePath();
    }
}
