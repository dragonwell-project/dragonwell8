/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8365058
 * @summary Check basic correctness of de-serialization
 * @library /lib/testlibrary
 * @run junit SerializationTest
 */

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.concurrent.CopyOnWriteArraySet;
import java.util.stream.Stream;

import jdk.testlibrary.Utils;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

public class SerializationTest {

    // Ensure basic serialization round trip correctness
    @Test
    public void test() {
        roundTripTest().forEach((expected) -> {
            byte[] bytes = ser(expected);
            Object actual = deSer(bytes);
            assertEquals(CopyOnWriteArraySet.class, actual.getClass());
            assertEquals(expected, actual);
        });
    }

    private static Stream<CopyOnWriteArraySet<?>> roundTripTest() {
        return Stream.of(
                new CopyOnWriteArraySet<>(),
                new CopyOnWriteArraySet<>(Utils.listOf(1, 2, 3)),
                new CopyOnWriteArraySet<>(Utils.setOf("Foo", "Bar", "Baz"))
        );
    }

    private static byte[] ser(Object obj) {
        try (ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
             ObjectOutputStream oos = new ObjectOutputStream(byteArrayOutputStream)) {
            oos.writeObject(obj);
            return byteArrayOutputStream.toByteArray();
        } catch (Exception e) {
            fail("Unexpected error during serialization");
            return null;
        }
    }

    private static Object deSer(byte[] bytes) {
        try (ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(bytes);
             ObjectInputStream ois = new ObjectInputStream(byteArrayInputStream)) {
            return ois.readObject();
        } catch (Exception e) {
            fail("Unexpected error during de-serialization");
            return null;
        }
    }
}
