/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

package gc.stress.gclocker;

// Based on Kim Barrett;s test for JDK-8048556

/*
 * @test TestExcessGCLockerCollections
 * @key gc
 * @bug 8048556
 * @summary Check for GC Locker initiated GCs that immediately follow another
 * GC and so have very little needing to be collected.
 * @library /testlibrary
 * @run driver/timeout=1000 gc.stress.gclocker.TestExcessGCLockerCollections 300 4 2
 */

import java.util.HashMap;
import java.util.Map;

import java.util.zip.Deflater;

import java.util.ArrayList;
import java.util.Arrays;

import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.openmbean.CompositeData;
import java.lang.management.ManagementFactory;
import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.MemoryUsage;
import java.util.List;
import com.sun.management.GarbageCollectionNotificationInfo;
import com.sun.management.GcInfo;

import com.oracle.java.testlibrary.Asserts;
import com.oracle.java.testlibrary.ProcessTools;
import com.oracle.java.testlibrary.OutputAnalyzer;

class TestExcessGCLockerCollectionsStringConstants {
    // Some constant strings used in both GC logging and error detection
    static public final String GCLOCKER_CAUSE = "GCLocker Initiated GC";
    static public final String USED_TOO_LOW = "TOO LOW";
    static public final String USED_OK = "OK";
}

class TestExcessGCLockerCollectionsAux {
    static private final int LARGE_MAP_SIZE = 64 * 1024;

    static private final int MAP_ARRAY_LENGTH = 4;
    static private final int MAP_SIZE = 1024;

    static private final int BYTE_ARRAY_LENGTH = 128 * 1024;

    static private void println(String str) { System.out.println(str); }
    static private void println()           { System.out.println();    }

    static private volatile boolean keepRunning = true;

    static Map<Integer,String> populateMap(int size) {
        Map<Integer,String> map = new HashMap<Integer,String>();
        for (int i = 0; i < size; i += 1) {
            Integer keyInt = Integer.valueOf(i);
            String valStr = "value is [" + i + "]";
            map.put(keyInt,valStr);
        }
        return map;
    }

    static private class AllocatingWorker implements Runnable {
        private final Object[] array = new Object[MAP_ARRAY_LENGTH];
        private int arrayIndex = 0;

        private void doStep() {
            Map<Integer,String> map = populateMap(MAP_SIZE);
            array[arrayIndex] = map;
            arrayIndex = (arrayIndex + 1) % MAP_ARRAY_LENGTH;
        }

        public void run() {
            while (keepRunning) {
                doStep();
            }
        }
    }

    static private class JNICriticalWorker implements Runnable {
        private int count;

        private void doStep() {
            byte[] inputArray = new byte[BYTE_ARRAY_LENGTH];
            for (int i = 0; i < inputArray.length; i += 1) {
                inputArray[i] = (byte) (count + i);
            }

            Deflater deflater = new Deflater();
            deflater.setInput(inputArray);
            deflater.finish();

            byte[] outputArray = new byte[2 * inputArray.length];
            deflater.deflate(outputArray);

            count += 1;
        }

        public void run() {
            while (keepRunning) {
                doStep();
            }
        }
    }

    static class GCNotificationListener implements NotificationListener {
        static private final double MIN_USED_PERCENT = 40.0;

        static private final List<String> newGenPoolNames = Arrays.asList(
                "G1 Eden Space",           // OpenJDK G1GC: -XX:+UseG1GC
                "PS Eden Space",           // OpenJDK ParallelGC: -XX:+ParallelGC
                "Par Eden Space",          // OpenJDK ConcMarkSweepGC: -XX:+ConcMarkSweepGC
                "Eden Space"               // OpenJDK SerialGC: -XX:+UseSerialGC
                                           // OpenJDK ConcMarkSweepGC: -XX:+ConcMarkSweepGC -XX:-UseParNewGC
        );

        @Override
        public void handleNotification(Notification notification, Object handback) {
            try {
                if (notification.getType().equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION)) {
                    GarbageCollectionNotificationInfo info =
                            GarbageCollectionNotificationInfo.from((CompositeData) notification.getUserData());

                    String gc_cause = info.getGcCause();

                    if (gc_cause.equals(TestExcessGCLockerCollectionsStringConstants.GCLOCKER_CAUSE)) {
                        Map<String, MemoryUsage> memory_before_gc = info.getGcInfo().getMemoryUsageBeforeGc();

                        for (String newGenPoolName : newGenPoolNames) {
                            MemoryUsage usage = memory_before_gc.get(newGenPoolName);
                            if (usage == null) continue;

                            double startTime = ((double) info.getGcInfo().getStartTime()) / 1000.0;
                            long used = usage.getUsed();
                            long committed = usage.getCommitted();
                            long max = usage.getMax();
                            double used_percent = (((double) used) / Math.max(committed, max)) * 100.0;

                            System.out.printf("%6.3f: (%s) %d/%d/%d, %8.4f%% (%s)\n",
                                              startTime, gc_cause, used, committed, max, used_percent,
                                              ((used_percent < MIN_USED_PERCENT) ? TestExcessGCLockerCollectionsStringConstants.USED_TOO_LOW
                                                                                 : TestExcessGCLockerCollectionsStringConstants.USED_OK));
                        }
                    }
                }
            } catch (RuntimeException ex) {
                System.err.println("Exception during notification processing:" + ex);
                ex.printStackTrace();
            }
        }

        public static boolean register() {
            try {
                MBeanServer mbeanServer = ManagementFactory.getPlatformMBeanServer();

                // Get the list of MX
                List<GarbageCollectorMXBean> gc_mxbeans = ManagementFactory.getGarbageCollectorMXBeans();

                // Create the notification listener
                GCNotificationListener gcNotificationListener = new GCNotificationListener();

                for (GarbageCollectorMXBean gcbean : gc_mxbeans) {
                  // Add notification listener for the MXBean
                  mbeanServer.addNotificationListener(gcbean.getObjectName(), gcNotificationListener, null, null);
                }
            } catch (Exception ex) {
                System.err.println("Exception during mbean registration:" + ex);
                ex.printStackTrace();
                // We've failed to set up, terminate
                return false;
            }

            return true;
        }
    }

    static public Map<Integer,String> largeMap;

    static public void main(String args[]) {
        long durationSec = Long.parseLong(args[0]);
        int allocThreadNum = Integer.parseInt(args[1]);
        int jniCriticalThreadNum = Integer.parseInt(args[2]);

        println("Running for " + durationSec + " secs");

        if (!GCNotificationListener.register()) {
          println("failed to register GC notification listener");
          System.exit(-1);
        }

        largeMap = populateMap(LARGE_MAP_SIZE);

        println("Starting " + allocThreadNum + " allocating threads");
        for (int i = 0; i < allocThreadNum; i += 1) {
            new Thread(new AllocatingWorker()).start();
        }

        println("Starting " + jniCriticalThreadNum + " jni critical threads");
        for (int i = 0; i < jniCriticalThreadNum; i += 1) {
            new Thread(new JNICriticalWorker()).start();
        }

        long durationMS = (long) (1000 * durationSec);
        long start = System.currentTimeMillis();
        long now = start;
        long soFar = now - start;
        while (soFar < durationMS) {
            try {
                Thread.sleep(durationMS - soFar);
            } catch (Exception e) {
            }
            now = System.currentTimeMillis();
            soFar = now - start;
        }
        println("Done.");
        keepRunning = false;
    }
}

public class TestExcessGCLockerCollections {
    private static final String USED_OK_LINE =
        "\\(" + TestExcessGCLockerCollectionsStringConstants.GCLOCKER_CAUSE + "\\)"
              + " .* " +
        "\\(" + TestExcessGCLockerCollectionsStringConstants.USED_OK + "\\)";
    private static final String USED_TOO_LOW_LINE =
        "\\(" + TestExcessGCLockerCollectionsStringConstants.GCLOCKER_CAUSE + "\\)"
              + " .* " +
        "\\(" + TestExcessGCLockerCollectionsStringConstants.USED_TOO_LOW + "\\)";

    private static final String[] COMMON_OPTIONS = new String[] {
        "-Xmx1G", "-Xms1G", "-Xmn256M" };

    public static void main(String args[]) throws Exception {
        if (args.length < 3) {
            System.out.println("usage: TestExcessGCLockerCollections" +
                               " <duration sec> <alloc threads>" +
                               " <jni critical threads>");
            throw new RuntimeException("Invalid arguments");
        }

        ArrayList<String> finalArgs = new ArrayList<String>();
        finalArgs.addAll(Arrays.asList(COMMON_OPTIONS));
        finalArgs.add(TestExcessGCLockerCollectionsAux.class.getName());
        finalArgs.addAll(Arrays.asList(args));

        // GC and other options obtained from test framework.
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
            true, finalArgs.toArray(new String[0]));
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldHaveExitValue(0);
        //System.out.println("------------- begin stdout ----------------");
        //System.out.println(output.getStdout());
        //System.out.println("------------- end stdout ----------------");
        output.stdoutShouldMatch(USED_OK_LINE);
        output.stdoutShouldNotMatch(USED_TOO_LOW_LINE);
    }
}
