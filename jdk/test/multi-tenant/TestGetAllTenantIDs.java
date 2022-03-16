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
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary Test data structure integrity while access TenantContainer.tenantContainerMap concurrently
 * @library /lib/testlibrary
 * @build TestGetAllTenantIDs
 * @run main/othervm/timeout=120 -XX:+MultiTenant -Xmx1g  -Xms1g TestGetAllTenantIDs
 */

import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.stream.IntStream;
import java.util.stream.Stream;
import static jdk.testlibrary.Asserts.fail;

public class TestGetAllTenantIDs {
    public static void main(String[] args) {
        int parallel = Runtime.getRuntime().availableProcessors();
        TenantConfiguration config = new TenantConfiguration();
        List<TenantContainer> tenants = new LinkedList<>();
        IntStream.range(0, parallel)
                .forEach(i -> tenants.add(TenantContainer.create(config)));

        CountDownLatch startTesting = new CountDownLatch(1);
        AtomicBoolean terminated = new AtomicBoolean(false);
        Thread[] getterThreads = new Thread[parallel];
        Thread[] setterThreads = new Thread[parallel];
        Thread[][] allThreads = { getterThreads, setterThreads };
        IntStream.range(0, parallel)
                .forEach(i-> {
                    getterThreads[i] = new Thread(()->{
                        try { startTesting.await(); } catch(InterruptedException e) { fail(); }
                        int len = 0;
                        while (!terminated.get()) {
                            len = TenantContainer.getAllTenantIds().size();
                        }
                    });
                    getterThreads[i].start();
                    setterThreads[i] = new Thread(()->{
                        try { startTesting.await(); } catch(InterruptedException e) { fail(); }
                        while (!terminated.get()) {
                            TenantContainer tenant = TenantContainer.create(config);
                        }
                    });
                    setterThreads[i].start();
                });
        startTesting.countDown();

        try {
            Thread.sleep(15 * 1000);
        } catch (InterruptedException e) {
            fail();
        }

        terminated.set(true);

        for (Thread[] threads : allThreads) {
            Stream.of(threads)
                    .forEach(t -> {
                        try {
                            t.join();
                        } catch (Exception e) {
                            fail();
                        }
                    });
        }
    }
}