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
 * @summary Test builder and update constraint
 * @library /lib/testlibrary
 * @run main TestConfiguration
 */

import demo.MyResourceContainer;
import demo.MyResourceFactory;
import demo.MyResourceType;

import java.util.Arrays;
import java.util.*;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;

import static jdk.testlibrary.Asserts.assertTrue;

public class TestConfiguration {
    public static void main(String[] args) {

        MyResourceContainer mc = (MyResourceContainer) MyResourceFactory.INSTANCE.createContainer(Arrays.asList(
                MyResourceType.MY_RESOURCE1.newConstraint(),
                MyResourceType.MY_RESOURCE2.newConstraint()));

        assertTrue(iterator2Stream(mc.operations.iterator()).collect(Collectors.toSet())
                .equals(new HashSet<>(Arrays.asList("update " + MyResourceType.MY_RESOURCE1.toString(),
                        "update " + MyResourceType.MY_RESOURCE2.toString()))));

        mc.updateConstraint(MyResourceType.MY_RESOURCE2.newConstraint());

        assertTrue(iterator2Stream(mc.operations.iterator()).collect(Collectors.toSet())
                .equals(new HashSet<>(Arrays.asList("update " + MyResourceType.MY_RESOURCE1.toString(),
                        "update " + MyResourceType.MY_RESOURCE2.toString(),
                        "update " + MyResourceType.MY_RESOURCE2.toString()))));
    }

    private static <T> Stream<T> iterator2Stream(Iterator<T> iterator) {
        return StreamSupport.stream(
                Spliterators.spliteratorUnknownSize(iterator, Spliterator.ORDERED),
                false);
    }
}
