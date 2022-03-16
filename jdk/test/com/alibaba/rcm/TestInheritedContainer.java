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
 *
 */

/*
 * @test
 * @summary Test ONLY inheritedResourceContainer is inherited from parent
 * @library /lib/testlibrary
 * @run main TestInheritedContainer
 */

import com.alibaba.rcm.ResourceContainer;
import demo.MyResourceFactory;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;

import java.util.Collections;
import java.util.concurrent.CompletableFuture;

import static jdk.testlibrary.Asserts.assertEQ;


public class TestInheritedContainer {
    public static void main(String[] args) throws Exception {
        ResourceContainer rc = MyResourceFactory.INSTANCE.createContainer(Collections.emptyList());
        JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();
        rc.run(() -> {
            try {
                assertEQ(ResourceContainer.root(),
                        CompletableFuture.supplyAsync(() -> JLA.getResourceContainer(Thread.currentThread())).get());
                assertEQ(rc,
                        CompletableFuture.supplyAsync(() -> JLA.getInheritedResourceContainer(Thread.currentThread())).get());
            } catch (Exception e) {
                throw new AssertionError(e);
            }
        });
    }
}
