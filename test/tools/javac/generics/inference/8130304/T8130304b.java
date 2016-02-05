/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @bug 8130304
 * @summary Inference: NodeNotFoundException thrown with deep generic method call chain
 * @compile T8130304b.java
 */
class T8130304b {

    void test() {
        TestZ r = null;
        Crazy<String, String> x = r.m3("X");
        r.m1(r.m4(r.m2(x, r.m3("a")), t -> "x"), r.m3("a"));
    }

    interface Crazy<A, B> { }

    interface TestZ {
        public <A, B> Crazy<A, B> m1(Crazy<A, ? extends B>... d);
        public <A, B, C> Crazy<A, Crazy<B, C>> m2(Crazy<A, B> e, Crazy<A, C> f);
        public <A> Crazy<A, A> m3(A g);
        public <A, B, C> Crazy<A, C> m4(Crazy<A, B> h, java.util.function.Function<B, C> i);
    }
}
