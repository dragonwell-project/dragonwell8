/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

import java.lang.*;
import java.lang.reflect.*;
import sun.hotspot.WhiteBox;

public class DummyClassHelper {
    public static void main(String[] args) throws Exception {
        String[] classNames = {args[0], args[1]};
        Class cls = null;
        if (args.length == 2) {
            for (int i = 0; i < classNames.length; i++) {
                Method m = null;
                cls = Class.forName(classNames[i]);
                try {
                    m = cls.getMethod("thisClassIsDummy");
                    throw new java.lang.RuntimeException(classNames[i] +
                        " should be loaded from the jimage and should not have the thisClassIsDummy() method.");
                } catch(NoSuchMethodException ex) {
                    System.out.println(ex.toString());
                }
            }
        } else {
            WhiteBox wb = WhiteBox.getWhiteBox();
            for (int i = 0; i < classNames.length; i++) {
                cls = Class.forName(classNames[i]);
                if (!wb.isSharedClass(cls)) {
                    System.out.println(classNames[i] + ".class" + " is not in shared space as expected.");
                } else {
                    throw new java.lang.RuntimeException(classNames[i] +
                        ".class shouldn't be in shared space.");
                }
            }
        }
    }
}
