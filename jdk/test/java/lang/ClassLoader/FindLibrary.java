/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

/* @test
   @bug 8181205
   @summary JRE fails to load/register security providers when started from UNC pathname
*/

import java.io.File;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;

    public class FindLibrary {
        public static void main(String[] args) throws Exception {
            // this test is most relevant to windows. should pass silently on
            // other OSes

            Class<?> innerClazz = Class.forName("sun.misc.Launcher$ExtClassLoader");
            Class<?>[] ctorArgTypes = {File[].class};

            File tmpFile = new File(System.getProperty("java.home"));
            File[] f = new File[1];
            String uncFileName = "\\\\127.0.0.1\\" + tmpFile.getAbsolutePath().replace(':', '$');
            System.out.println("Testing UNC path of :" + uncFileName);

            f[0] = new File(uncFileName); // a UNC path that must exist
            Object[] ctorArgs = {f};
            Constructor constructor = innerClazz.getDeclaredConstructor(File[].class);
            constructor.setAccessible(true);
            Object o = constructor.newInstance(ctorArgs);
            Method m = innerClazz.getMethod("findLibrary", String.class);
            m.setAccessible(true);
            //buggy JDK will throw IllegalArgumentException with next call
            m.invoke(o, "test");
        }
    }
