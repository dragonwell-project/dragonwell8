/*
 * Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.
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

package jdk.jfr.event.compiler;

import java.lang.reflect.Method;
import java.util.List;

import jdk.jfr.Recording;
import jdk.jfr.consumer.RecordedEvent;
import jdk.test.lib.Utils;
import jdk.test.lib.jfr.EventNames;
import jdk.test.lib.jfr.Events;
import sun.hotspot.WhiteBox;

/**
 * @test
 * @key jfr
 *
 *
 * @library /test/lib /
 * @build sun.hotspot.WhiteBox
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 *     sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -Xbootclasspath/a:.
 *     -XX:-NeverActAsServerClassMachine
 *     -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *     -XX:CompileOnly=jdk.jfr.event.compiler.TestCompilerPhase::dummyMethod
 *     -Xbootclasspath/a:.
 *     jdk.jfr.event.compiler.TestCompilerPhase
 */
public class TestCompilerPhase {
    private final static String EVENT_NAME = EventNames.CompilerPhase;
    private final static String METHOD_NAME = "dummyMethod";
    private static final int COMP_LEVEL_SIMPLE = 1;
    private static final int COMP_LEVEL_FULL_OPTIMIZATION = 4;

    public static void main(String[] args) throws Exception {
        Recording recording = new Recording();
        recording.enable(EVENT_NAME);
        recording.start();

        // Provoke compilation
        Method mtd = TestCompilerPhase.class.getDeclaredMethod(METHOD_NAME, new Class[0]);
        WhiteBox WB = WhiteBox.getWhiteBox();
        if (!WB.enqueueMethodForCompilation(mtd, COMP_LEVEL_FULL_OPTIMIZATION)) {
            WB.enqueueMethodForCompilation(mtd, COMP_LEVEL_SIMPLE);
        }
        Utils.waitForCondition(() -> WB.isMethodCompiled(mtd));
        dummyMethod();

        recording.stop();

        List<RecordedEvent> events = Events.fromRecording(recording);
        Events.hasEvents(events);
        for (RecordedEvent event : events) {
            System.out.println("Event:" + event);
            Events.assertField(event, "phase").notEmpty();
            Events.assertField(event, "compileId").atLeast(0);
            Events.assertField(event, "phaseLevel").atLeast((short)0).atMost((short)4);
            Events.assertEventThread(event);
        }
    }

    static void dummyMethod() {
        System.out.println("hello!");
    }
}
