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
 * @bug 8195160
 * @summary RdmaSockets API Exceptions
 * @run testng Exceptions
 */

import java.net.ProtocolFamily;
import org.testng.annotations.Test;
import static jdk.net.RdmaSockets.*;
import static org.testng.Assert.*;

public class Exceptions {

    static final Class<NullPointerException> NPE = NullPointerException.class;
    static final Class<UnsupportedOperationException> UOE = UnsupportedOperationException.class;

    @Test
    public void testNull() {
        assertThrows(NPE, () -> openSocket(null));
        assertThrows(NPE, () -> openServerSocket(null));
        assertThrows(NPE, () -> openSocketChannel(null));
        assertThrows(NPE, () -> openServerSocketChannel(null));
    }

    static class UnsupportedFamily implements ProtocolFamily {
        @Override public String name() { return null; }
    }

    @Test
    public void testBadFamily() {
        assertThrows(UOE, () -> openSocket(new UnsupportedFamily()));
        assertThrows(UOE, () -> openServerSocket(new UnsupportedFamily()));
        assertThrows(UOE, () -> openSocketChannel(new UnsupportedFamily()));
        assertThrows(UOE, () -> openServerSocketChannel(new UnsupportedFamily()));
    }
}
