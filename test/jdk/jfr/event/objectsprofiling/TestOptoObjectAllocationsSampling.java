/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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
 *
 */
package jfr.event.objectsprofiling;

import java.lang.reflect.Method;
import java.util.List;
import java.util.concurrent.TimeUnit;

import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.jfr.consumer.RecordedClass;
import jdk.jfr.consumer.RecordedStackTrace;
import jdk.jfr.consumer.RecordedFrame;

import sun.hotspot.WhiteBox;

import jdk.test.lib.Asserts;
import jdk.test.lib.jfr.EventNames;
import jdk.test.lib.jfr.Events;
import jdk.test.lib.Utils;

/*
 * @test TestOptoObjectAllocationsSampling
 * @library /lib /
 *
 * @build sun.hotspot.WhiteBox
 * @build ClassFileInstaller
 *
 * @compile -g TestOptoObjectAllocationsSampling.java
 *
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox
 *     sun.hotspot.WhiteBox$WhiteBoxPermission
 *
 * @run main/othervm -Xbootclasspath/a:.
 *     -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:-TieredCompilation
 *     -XX:+LogJFR -XX:FlightRecorderOptions=sampleobjectallocations=true,objectallocationssamplinginterval=1
 *     jfr.event.objectsprofiling.TestOptoObjectAllocationsSampling
 */
public class TestOptoObjectAllocationsSampling {
    private static final WhiteBox WB;
    private static final Method OPTO_METHOD;
    private static final int OPTO_METHOD_INVOKE_COUNT = 10;
    private static final String OPTO_METHOD_NAME = "fireOptoAllocations";
    private static final int COMP_LEVEL_FULL_OPTIMIZATION = 4;
    private static final int RECORDED_ARRAY_CLASS_OBJECT_SIZE_MAGIC_CODE = 0x1111baba;

    static {
        try {
            WB = WhiteBox.getWhiteBox();
            Class<?> thisClass = TestOptoObjectAllocationsSampling.class;
            OPTO_METHOD = thisClass.getMethod(OPTO_METHOD_NAME, Integer.TYPE, Byte.TYPE);
        } catch (Exception e) {
            Asserts.fail(e.getMessage());
            throw new ExceptionInInitializerError(e);
        }
    }

    private static class InstanceObject {
        private byte content;

        InstanceObject(byte content) {
            this.content = content;
        }

        public String toString() {
            return "InstanceObject ( " + content + " )";
        }
    }

    public static Object array;
    public static Object instance;

    public static void fireOptoAllocations(int arrayLength, byte b) {
        array = new InstanceObject[arrayLength];
        instance = new InstanceObject(b);
    }

    private static void ensureOptoMethod() throws Exception {
        int initialCompLevel = WB.getMethodCompilationLevel(OPTO_METHOD);
        if (initialCompLevel != COMP_LEVEL_FULL_OPTIMIZATION) {
            WB.enqueueMethodForCompilation(OPTO_METHOD, COMP_LEVEL_FULL_OPTIMIZATION);
        }
        Utils.waitForCondition(() -> COMP_LEVEL_FULL_OPTIMIZATION == WB.getMethodCompilationLevel(OPTO_METHOD), 30_000L);
        System.out.format("%s is already compiled at full optimization compile level\n", OPTO_METHOD_NAME);
    }

    private static void assertOptoMethod() throws Exception {
        int compLevel = WB.getMethodCompilationLevel(OPTO_METHOD);
        Asserts.assertTrue(compLevel == COMP_LEVEL_FULL_OPTIMIZATION);
    }

    private static void runForLong(int arg1, byte arg2) {
        for (int i = 0; i < 1000_000; i++) {
            try {
                ensureOptoMethod();
                fireOptoAllocations(arg1, arg2);
                TimeUnit.SECONDS.sleep(1);
            } catch (Exception e) {
            }
        }
    }

    public static void main(String[] args) throws Exception {
        int arg1 = 3*1024;
        byte arg2 = (byte)0x66;

        // Run warm code to prevent deoptimizaiton.
        // It will deoptimize if it runs without this warmup step.
        fireOptoAllocations(arg1, arg2);
        ensureOptoMethod();
        assertOptoMethod();

        // runForLong(arg1, arg2);

        try (Recording recording = new Recording()) {
            recording.enable(EventNames.OptoArrayObjectAllocation);
            recording.enable(EventNames.OptoInstanceObjectAllocation);

            recording.start();

            // fireOptoAllocationsMethod would be deoptimized after jfr recording is started.
            // The root cause is not clear.
            // Invoke ensureOptoMethod blindly to enforce it in level 4 again.
            ensureOptoMethod();

            for (int i = 0; i < OPTO_METHOD_INVOKE_COUNT; i++) {
                assertOptoMethod();
                fireOptoAllocations(arg1, arg2);
                WB.youngGC();
            }

            recording.stop();

            List<RecordedEvent> events = Events.fromRecording(recording);
            int countOfInstanceObject = 0;
            int countOfArrayObject = 0;
            final String instanceObjectClassName = InstanceObject.class.getName();
            final String arrayObjectClassName = InstanceObject[].class.getName();

            for (RecordedEvent event : events) {
                RecordedStackTrace stackTrace = event.getStackTrace();
                Asserts.assertTrue(stackTrace !=  null);
                List<RecordedFrame> frames = stackTrace.getFrames();
                Asserts.assertTrue(frames != null && frames.size() > 0);
                RecordedFrame topFrame = frames.get(0);
                Asserts.assertTrue(event.hasField("objectClass"));
                RecordedClass clazz = event.getValue("objectClass");
                String className = clazz.getName();
                Asserts.assertTrue(event.getStackTrace().getFrames().size() > 0);
                int objectSize = clazz.getObjectSize();
                System.out.format("Allocation Object Class Name: %s, Object Size: %x, topFrame: %s\n", className, objectSize, topFrame.getMethod());
                if (className.equals(instanceObjectClassName)) {
                    Asserts.assertTrue(!clazz.isArray());
                    Asserts.assertTrue(objectSize > 0);
                    Asserts.assertTrue(topFrame.getLineNumber() > 0);
                    Asserts.assertEquals(7, topFrame.getBytecodeIndex());
                    countOfInstanceObject++;
                } else if (className.equals(arrayObjectClassName)) {
                    Asserts.assertTrue(clazz.isArray());
                    Asserts.assertTrue(objectSize == RECORDED_ARRAY_CLASS_OBJECT_SIZE_MAGIC_CODE);
                    countOfArrayObject++;
                    Asserts.assertTrue(topFrame.getLineNumber() > 0);
                    Asserts.assertEquals(1, topFrame.getBytecodeIndex());
                }
            }
            System.out.format("Total Event Count: %d, EventOptoInstanceObjectAllocaiton Count: %d, EventOptoArrayObjectAllocation Count: %d\n", events.size(), countOfInstanceObject, countOfArrayObject);

            Asserts.assertTrue(countOfArrayObject == countOfInstanceObject);
            Asserts.assertTrue(countOfArrayObject == OPTO_METHOD_INVOKE_COUNT);
            Asserts.assertTrue(events.size() >= (countOfInstanceObject + countOfArrayObject));
        }
    }
}
