/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package jdk.jfr.api.consumer.streaming;

import static jdk.test.lib.Asserts.assertTrue;

import java.io.IOException;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Properties;

import sun.misc.Unsafe;
import jdk.jfr.Event;
import jdk.test.lib.process.ProcessTools;

import com.sun.tools.attach.VirtualMachine;

/**
 * Class that emits a NUMBER_OF_EVENTS and then awaits crash or exit
 *
 * Requires jdk.attach module.
 *
 */
public final class TestProcess {

    private static class TestEvent extends Event {
    }

    public final static int NUMBER_OF_EVENTS = 10;

    private final Process process;
    private final long pid;
    private final Path path;

    public TestProcess(String name) throws IOException {
        this.path = Paths.get("action-" + System.currentTimeMillis()).toAbsolutePath();
        Path pidPath = Paths.get("pid-" + System.currentTimeMillis()).toAbsolutePath();
        String[] args = {
                "-XX:StartFlightRecording:settings=none",
                TestProcess.class.getName(), path.toString(),
                pidPath.toString()
            };
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(false, args);
        process = ProcessTools.startProcess(name, pb);
        do {
            takeNap();
        } while (!pidPath.toFile().exists());

        String pidStr;
        do {
            pidStr = readString(pidPath);
        } while (pidStr == null || !pidStr.endsWith("@"));

        pid = Long.valueOf(pidStr.substring(0, pidStr.length()-1));
    }

    private static String readString(Path path) throws IOException {
        try (InputStream in = new FileInputStream(path.toFile())) {
            byte[] bytes = new byte[32];
            int length = in.read(bytes);
            assertTrue(length < bytes.length, "bytes array to small");
            if (length == -1) {
                return null;
            }
            return new String(bytes, 0, length);
        }
    }

    private static void writeString(Path path, String content) throws IOException {
        try (OutputStream out = new FileOutputStream(path.toFile())) {
            out.write(content.getBytes());
        }
    }

    public static void main(String... args) throws Exception {
        Path pidPath = Paths.get(args[1]);
        writeString(pidPath, ProcessTools.getProcessId() + "@");

        for (int i = 0; i < NUMBER_OF_EVENTS; i++) {
            TestEvent e = new TestEvent();
            e.commit();
        }

        Path path = Paths.get(args[0]);
        while (true) {
            try {
                String action = readString(path);
                if ("crash".equals(action)) {
                    System.out.println("About to crash...");
                    Unsafe.getUnsafe().putInt(0L, 0);
                }
                if ("exit".equals(action)) {
                    System.out.println("About to exit...");
                    System.exit(0);
                }
            } catch (Exception ioe) {
                // Ignore
            }
            takeNap();
        }
    }

    public Path getRepository() {
        while (true) {
            try {
                VirtualMachine vm = VirtualMachine.attach(String.valueOf(pid));
                Properties p = vm.getSystemProperties();
                vm.detach();
                String repo = (String) p.get("jdk.jfr.repository");
                if (repo != null) {
                    return Paths.get(repo);
                }
            } catch (Exception e) {
                System.out.println("Attach failed: " + e.getMessage());
                System.out.println("Retrying...");
            }
            takeNap();
        }
    }

    private static void takeNap() {
        try {
            Thread.sleep(10);
        } catch (InterruptedException ie) {
            // ignore
        }
    }

    public void crash() {
        try {
            writeString(path, "crash");
        } catch (IOException ioe) {
            ioe.printStackTrace();
        }
    }

    public void exit() {
        try {
            writeString(path, "exit");
        } catch (IOException ioe) {
            ioe.printStackTrace();
        }
    }

    public long pid() {
        return pid;
    }
}
