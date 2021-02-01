/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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
package jdk.jfr.jvm;

import java.io.IOException;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

import sun.misc.Unsafe;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordingFile;
import jdk.test.lib.Asserts;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

/**
 * @test
 * @key jfr
 * @summary Verifies that data associated with a running recording can be evacuated to an hs_err_pidXXX.jfr when the VM crashes
 *
 *
 * @library /lib /
 *

 *
 * @run main/othervm jdk.jfr.jvm.TestDumpOnCrash
 */
public class TestDumpOnCrash {

    private static final CharSequence LOG_FILE_EXTENSION = ".log";
    private static final CharSequence JFR_FILE_EXTENSION = ".jfr";

    private static String readString(Path path) throws IOException {
        try (InputStream in = new FileInputStream(path.toFile())) {
            byte[] bytes = new byte[32];
            int length = in.read(bytes);
            Asserts.assertTrue(length < bytes.length, "bytes array to small");
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

    static class CrasherIllegalAccess {
        public static void main(String[] args) {
          try {
            Path pidPath = Paths.get(args[1]);
            writeString(pidPath, ProcessTools.getProcessId() + "@");

            Field theUnsafeRefLocation = Unsafe.class.getDeclaredField("theUnsafe");
            theUnsafeRefLocation.setAccessible(true);
            ((Unsafe)theUnsafeRefLocation.get(null)).putInt(0L, 0);
          }
          catch(Exception ex) {
            Asserts.fail("cannot execute");
          }
        }
    }

    static class CrasherHalt {
        public static void main(String[] args) throws Exception {
            Path pidPath = Paths.get(args[1]);
            writeString(pidPath, ProcessTools.getProcessId() + "@");

            System.out.println("Running Runtime.getRuntime.halt");
            Runtime.getRuntime().halt(17);
        }
    }

    static class CrasherSig {
        public static void main(String[] args) throws Exception {
            long pid = Long.valueOf(ProcessTools.getProcessId());
            Path pidPath = Paths.get(args[1]);
            writeString(pidPath, pid + "@");

            String signalName = args[0];
            System.out.println("Sending SIG" + signalName + " to process " + pid);
            Runtime.getRuntime().exec("kill -" + signalName + " " + pid).waitFor();
        }
    }

    public static void main(String[] args) throws Exception {
        verify(runProcess(CrasherIllegalAccess.class.getName(), ""));
        verify(runProcess(CrasherHalt.class.getName(), ""));

        // Verification is excluded for the test case below until 8219680 is fixed
        long pid = runProcess(CrasherSig.class.getName(), "FPE");
        // @ignore 8219680
        // verify(pid);
    }

    private static long runProcess(String crasher, String signal) throws Exception {
        System.out.println("Test case for crasher " + crasher);
        Path pidPath = Paths.get("pid-" + System.currentTimeMillis()).toAbsolutePath();
        Process p = ProcessTools.createJavaProcessBuilder(true,
                "-Xmx64m",
                "-XX:-TransmitErrorReport",
                "-XX:-CreateMinidumpOnCrash",
                /*"--add-exports=java.base/jdk.internal.misc=ALL-UNNAMED",*/
                "-XX:StartFlightRecording",
                crasher,
                signal,
                pidPath.toString())
            .start();

        OutputAnalyzer output = new OutputAnalyzer(p);
        System.out.println("========== Crasher process output:");
        System.out.println(output.getOutput());
        System.out.println("==================================");

        String pidStr;
        do {
            pidStr = readString(pidPath);
        } while (pidStr == null || !pidStr.endsWith("@"));

        return Long.valueOf(pidStr.substring(0, pidStr.length() - 1));
    }

    private static void verify(long pid) throws IOException {
        String fileName = "hs_err_pid" + pid + ".jfr";
        Path file = Paths.get(fileName).toAbsolutePath().normalize();

        Asserts.assertTrue(Files.exists(file), "No emergency jfr recording file " + file + " exists");
        Asserts.assertNotEquals(Files.size(file), 0L, "File length 0. Should at least be some bytes");
        System.out.printf("File size=%d%n", Files.size(file));

        List<RecordedEvent> events = RecordingFile.readAllEvents(file);
        Asserts.assertFalse(events.isEmpty(), "No event found");
        System.out.printf("Found event %s%n", events.get(0).getEventType().getName());
    }
}

