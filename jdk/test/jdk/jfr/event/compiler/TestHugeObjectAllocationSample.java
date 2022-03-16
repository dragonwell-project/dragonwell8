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

package jdk.jfr.event.compiler;

import static java.lang.Math.floor;
import static jdk.test.lib.Asserts.assertGreaterThanOrEqual;

import java.time.Duration;

import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.test.lib.jfr.EventNames;
import jdk.test.lib.jfr.Events;

/**
 * @test
 * @summary Test that when a huge object is allocated an event will be triggered.
 * @key jfr
 *
 * @library /lib /
 * @run main/othervm -XX:+UseTLAB -XX:-FastTLABRefill -XX:TLABSize=90k -XX:-ResizeTLAB -XX:TLABRefillWasteFraction=256 -XX:HugeObjectAllocationThreshold=64m jdk.jfr.event.compiler.TestHugeObjectAllocationSample
 * @run main/othervm -XX:+UseTLAB -XX:-FastTLABRefill -XX:TLABSize=90k -XX:-ResizeTLAB -XX:TLABRefillWasteFraction=256 -Xint -XX:HugeObjectAllocationThreshold=64m jdk.jfr.event.compiler.TestHugeObjectAllocationSample
 */
public class TestHugeObjectAllocationSample {
    private static final String EVENT_NAME = EventNames.HugeObjectAllocationSample;
    private static final int OBJECT_SIZE = 64 * 1024 * 1024;
    private static final int OBJECTS_TO_ALLOCATE = 100;

    public static void main(String[] args) throws Exception {
        Recording recording = new Recording();
        recording.enable(EVENT_NAME).withThreshold(Duration.ofMillis(0));
        recording.start();
        for (int i = 0; i < OBJECTS_TO_ALLOCATE ; i++) {
            System.out.println("new huge byte array " + i +": " + new byte[OBJECT_SIZE]);
        }
        recording.stop();
        int count = 0;
        for (RecordedEvent event : Events.fromRecording(recording)) {
            if (!EVENT_NAME.equals(event.getEventType().getName())) {
                continue;
            }
            System.out.println("Event:" + event);
            long allocationSize = Events.assertField(event, "allocationSize").atLeast((long)OBJECT_SIZE).getValue();
            String className = Events.assertField(event, "objectClass.name").notEmpty().getValue();
            if (Thread.currentThread().getId() == event.getThread().getJavaThreadId()
                && className.equals(byte[].class.getName())) {
              count++;
            }
        }
        int minCount = (int) floor(OBJECTS_TO_ALLOCATE * 0.80);
        assertGreaterThanOrEqual(count, minCount, "Too few events");
    }
}
