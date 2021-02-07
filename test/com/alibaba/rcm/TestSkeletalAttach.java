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
 * @summary Test skeletal implementation in AbstractResourceContainer
 * @library /lib/testlibrary
 * @run main TestSkeletalAttach
 */

import com.alibaba.rcm.ResourceContainer;
import demo.MyResourceContainer;
import demo.MyResourceFactory;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import static jdk.testlibrary.Asserts.assertTrue;


public class TestSkeletalAttach {
    public static void main(String[] args) {
        MyResourceContainer rc = (MyResourceContainer) MyResourceFactory.INSTANCE
                .createContainer(Collections.emptyList());
        rc.run(() -> {
            assertListEQ(Collections.singletonList("attach"), rc.operations);
        });
        assertListEQ(Arrays.asList("attach", "detach"), rc.operations);

        rc.operations.clear();
        rc.run(() -> {
            rc.run(() -> {
                assertListEQ(Collections.singletonList("attach"), rc.operations);
            });
        });
        assertListEQ(Arrays.asList("attach", "detach"), rc.operations);

        rc.operations.clear();
        rc.run(() -> {
            assertListEQ(Collections.singletonList("attach"), rc.operations);
            ResourceContainer.root().run(() -> {
                assertListEQ(Arrays.asList("attach", "detach"), rc.operations);
            });
        });
        assertTrue(Arrays.asList("attach", "detach", "attach", "detach").equals(rc.operations));

        rc.operations.clear();
        rc.run(() -> {
            assertListEQ(Collections.singletonList("attach"), rc.operations);
            ResourceContainer.root().run(() -> {
                rc.run(() -> {
                    assertListEQ(Arrays.asList("attach", "detach", "attach"), rc.operations);
                });
            });
        });
        assertTrue(Arrays.asList("attach", "detach", "attach", "detach", "attach", "detach").equals(rc.operations));
    }

    private static void assertListEQ(List<String> lhs, List<String> rhs) {
        assertTrue(lhs.equals(rhs), "expect " + lhs + " equals to " + rhs);
    }
}
