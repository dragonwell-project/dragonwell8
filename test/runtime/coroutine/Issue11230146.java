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

/*
 * @test Issue11230146
 * @library /testlibrary /testlibrary/whitebox
 * @build Issue11230146
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm -XX:-Inline -XX:+EnableCoroutine -Xmx10m -Xms10m -XX:ReservedCodeCacheSize=3m -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI Issue11230146
 * @summary Issue11230146, JTreg test for D181275
 */

import sun.hotspot.WhiteBox;
import java.dyn.Coroutine;
import java.io.*;
import java.lang.reflect.Method;
import java.util.*;

public class Issue11230146 {
    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 100; i++) {
            // trigger method state translate:
            //   in_use -> not_entrant -> zombie -> unload -> zombie
            //                                             ^
            //                                             |
            //                                             +-- crash happens here (described in issue11230146)
            doTest();
        }
    }

    public static void doTest() throws Exception {
        WhiteBox whiteBox = WhiteBox.getWhiteBox();
        MyClassloader loader = new MyClassloader();
        try {
            Class c = loader.loadClass("ClassTmp");
            Method fooMethod = c.getMethod("foo");
            Object fooObject = c.newInstance();
            for (int i = 0; i < 10000; i++) {
                fooMethod.invoke(fooObject);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        System.gc();
        // deoptimize methods
        whiteBox.deoptimizeAll();
    }

    /*
     * class file contet:
     *
     * public class ClassTmp {
     *     public String a;
     *
     *     public void foo() {
     *         if (a != null) {
     *             System.out.println("foo...");
     *         }
     *     }
     * }
     *
     */
    public static String classContent =
            "yv66vgAAADQAIQoABwASCQAGABMJABQAFQgAFgoAFwAYBwAZBwAaAQABYQEAEkxqYXZhL2xhbmcv" +
            "U3RyaW5nOwEABjxpbml0PgEAAygpVgEABENvZGUBAA9MaW5lTnVtYmVyVGFibGUBAANmb28BAA1T" +
            "dGFja01hcFRhYmxlAQAKU291cmNlRmlsZQEADUNsYXNzVG1wLmphdmEMAAoACwwACAAJBwAbDAAc" +
            "AB0BAAZmb28uLi4HAB4MAB8AIAEACENsYXNzVG1wAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEv" +
            "bGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50" +
            "U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgAhAAYABwAAAAEAAQAIAAkA" +
            "AAACAAEACgALAAEADAAAAB0AAQABAAAABSq3AAGxAAAAAQANAAAABgABAAAAAQABAA4ACwABAAwA" +
            "AAA5AAIAAQAAABAqtAACxgALsgADEgS2AAWxAAAAAgANAAAADgADAAAABQAHAAYADwAIAA8AAAAD" +
            "AAEPAAEAEAAAAAIAEQ==";

    /**
     * decode Base64
     */
    public static byte[] decodeBase64(String str) throws Exception {
        byte[] bt = null;
        sun.misc.BASE64Decoder decoder = new sun.misc.BASE64Decoder();
        bt = decoder.decodeBuffer(str);
        return bt;
    }

    public static class MyClassloader extends ClassLoader {
        public Class loadClass(String name) throws ClassNotFoundException {
            if ("ClassTmp".equals(name)) {
                try {
                    byte[] bs = decodeBase64(classContent);
                    return defineClass(name, bs, 0, bs.length);
                } catch (Exception e) {
                    e.printStackTrace();
                    return null;
                }
            } else {
                return super.loadClass(name);
            }
        }
    }
}
