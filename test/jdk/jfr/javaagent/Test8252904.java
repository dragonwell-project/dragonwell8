/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020, Azul Systems, Inc.  All Rights Reserved.
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

/**
 * @test
 * @bug 8252904
 * @key jfr
 * @summary Tests that JFR event classes can be transformed via JVMTI
 *
 * @library /lib/testlibrary /lib /
 * @modules java.instrument
 *
 * @build jdk.jfr.javaagent.Test8252904
 *
 * @run driver jdk.test.lib.util.JavaAgentBuilder
 *             jdk.jfr.javaagent.Test8252904 Test8252904Agent.jar
 *
 * @run main/othervm -javaagent:Test8252904Agent.jar jdk.jfr.javaagent.Test8252904
 */

package jdk.jfr.javaagent;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;
import java.lang.instrument.IllegalClassFormatException;
import java.security.ProtectionDomain;
import jdk.jfr.*;

public class Test8252904 {
    private Instrumentation instr;

    private static class Transformer implements ClassFileTransformer {
        public byte[] transform(ClassLoader loader, String fullyQualifiedClassName, Class<?> classBeingRedefined,
                                ProtectionDomain protectionDomain, byte[] classBytes) throws IllegalClassFormatException {
            if ("jdk/jfr/Event".equals(fullyQualifiedClassName))
                System.out.println("transforming");
            return "jdk/jfr/Event".equals(fullyQualifiedClassName) ? classBytes : null;
        }
    }

    // Called when agent is loaded from command line
    public static void agentmain(String agentArgs, Instrumentation inst) throws Exception {
        inst.addTransformer(new Transformer());
    }

    // Called when agent is dynamically loaded
    public static void premain(String agentArgs, Instrumentation inst) throws Exception {
        inst.addTransformer(new Transformer());
    }


    public static void main(String... arg) throws Exception {
        Configuration c = Configuration.getConfiguration("default");
        Recording r = new Recording(c);
        r.setToDisk(false);
        r.start();
        System.gc();
        r.stop();
    }
}
