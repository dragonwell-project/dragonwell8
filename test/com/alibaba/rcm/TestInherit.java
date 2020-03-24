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
 * @summary Test resource container is inherited from parent
 * @library /lib/testlibrary
 * @run main TestInherit
 */

import com.alibaba.rcm.ResourceContainer;
import demo.RCInheritedThreadFactory;
import demo.MyResourceFactory;

import java.util.Collections;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;

import static jdk.testlibrary.Asserts.assertEQ;

public class TestInherit {
    public static void main(String[] args) {
        assertEQ(ResourceContainer.current(), ResourceContainer.root());
        ResourceContainer rc = MyResourceFactory.INSTANCE.createContainer(Collections.emptyList());
        rc.run(() -> {
            assertEQ(ResourceContainer.current(), rc);
            FutureTask<ResourceContainer> f1 = new FutureTask<>(ResourceContainer::current);
            RCInheritedThreadFactory.INSTANCE.newThread(f1).start();
            assertEQ(get(f1), rc);
            FutureTask<ResourceContainer> f2 = new FutureTask<>(ResourceContainer::current);
            ResourceContainer.root().run(() -> RCInheritedThreadFactory.INSTANCE.newThread(f2).start());
            assertEQ(get(f2), ResourceContainer.root());
        });

        // thread is bound to it's parent thread(the thread called Thread::init())
        // not dependent on where Thread::start() called.
        FutureTask<ResourceContainer> f3 = new FutureTask<>(ResourceContainer::current);
        Thread t3 = RCInheritedThreadFactory.INSTANCE.newThread(f3);
        // Thread::init is called in root()
        rc.run(t3::start);
        assertEQ(get(f3), ResourceContainer.root());

        FutureTask<ResourceContainer> f4 = new FutureTask<>(ResourceContainer::current);
        FutureTask<Thread> newThread = new FutureTask<>(() -> RCInheritedThreadFactory.INSTANCE.newThread(f4));
        // Thread::init is called in container
        rc.run(newThread);
        get(newThread).start();
        assertEQ(get(f4), rc);
    }

    private static <V> V get(Future<V> f) {
        try {
            return f.get();
        } catch (InterruptedException | ExecutionException e) {
            throw new InternalError(e);
        }
    }
}
