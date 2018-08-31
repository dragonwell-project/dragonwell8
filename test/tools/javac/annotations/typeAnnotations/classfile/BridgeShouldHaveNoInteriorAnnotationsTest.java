/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8160928
 * @summary javac incorrectly copies over interior type annotations to bridge method
 * @library /tools/javac/lib
 * @build ToolBox
 * @run compile -g BridgeShouldHaveNoInteriorAnnotationsTest.java
 * @run main BridgeShouldHaveNoInteriorAnnotationsTest
 */

import java.nio.file.Path;
import java.nio.file.Paths;
import java.lang.annotation.ElementType;
import java.lang.annotation.Target;
import java.io.*;

class Pair_8160928<T1, T2> {
}

public class BridgeShouldHaveNoInteriorAnnotationsTest implements java.util.Iterator<Pair_8160928<Object, Object>> {

    @Override
    public boolean hasNext() {
        throw new RuntimeException();
    }

    @Override
    public Pair_8160928<@NonNull Object, Object> next() {
        Comparable<@NonNull Object> cble1 = (Comparable<@NonNull Object>) null;
        return null;
    }

    @Override
    public void remove() {
        throw new RuntimeException();
    }

    @Target(ElementType.TYPE_USE)
    public @interface NonNull {
    }

    static class OutputExpectedOnceHolder {
        public String[] outputs = {
            "0: #55(): CAST, offset=0, type_index=0, location=[TYPE_ARGUMENT(0)]",
            "1: #55(): LOCAL_VARIABLE, {start_pc=5, length=2, index=1}, location=[TYPE_ARGUMENT(0)]",
        };
    }

    static class OutputExpectedTwiceHolder {
        public String[] outputs = {
            "0: #55(): METHOD_RETURN, location=[TYPE_ARGUMENT(0)]",
        };
    }

    public static void main(String[] args) throws Exception {
        Path classPath = Paths.get(System.getProperty("test.classes"), "BridgeShouldHaveNoInteriorAnnotationsTest.class");
        String javapOut = ToolBox.javap(new ToolBox.JavaToolArgs().setAllArgs("-v", "-p", classPath.toString()));

        OutputExpectedOnceHolder holder = new OutputExpectedOnceHolder();
        for (String s : holder.outputs) {
            String newOutput = javapOut.replace(s, "");
            if (((javapOut.length() - newOutput.length()) / s.length()) != 1)
                throw new AssertionError("Interior annotations carried over to bridge ?");
        }

        OutputExpectedTwiceHolder holder2 = new OutputExpectedTwiceHolder();
        for (String s : holder2.outputs) {
            String newOutput = javapOut.replace(s, "");
            if (((javapOut.length() - newOutput.length()) / s.length()) != 2)
                throw new AssertionError("Exterior annotations not properly carried over to bridge");
        }
    }

}
