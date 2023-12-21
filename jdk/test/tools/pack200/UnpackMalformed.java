/*
 * Copyright Amazon.com Inc. or its affiliates. All Rights Reserved.
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

/*
 * @test
 * @bug 8315135
 * @run main/othervm/timeout=300 -Dcom.sun.java.util.jar.pack.disable.native=false -Xmx8m UnpackMalformed
 */

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.jar.JarOutputStream;
import java.util.jar.Pack200;

@SuppressWarnings("removal")
public class UnpackMalformed {
    public static void main(String[] args) {
        try {
            ByteArrayInputStream in = new ByteArrayInputStream("foobar".getBytes());
            for (int i=0; i < 1_000; i++) {
                try {
                    JarOutputStream out = new JarOutputStream(new ByteArrayOutputStream());
                    Pack200.Unpacker unpacker = Pack200.newUnpacker();
                    unpacker.unpack(in, out);
                } catch (IOException e) {
                }
            }
        } catch (OutOfMemoryError e) {
            System.out.println(e);
            throw e;
        }
    }
}
