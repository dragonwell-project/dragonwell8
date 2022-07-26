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
 * @library /lib/testlibrary
 * @summary verify canceled timers are removed ASAP
 * @requires os.family == "linux"
 * @run main/othervm/manual -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 Wisp2TimerRemoveTest
 * @run main/othervm/manual -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.highPrecisionTimer=true  Wisp2TimerRemoveTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.util.concurrent.*;

import static jdk.testlibrary.Asserts.*;

public class Wisp2TimerRemoveTest {

    public static void main(String[] args) throws Exception {
        BlockingQueue<Integer> q1 = new ArrayBlockingQueue<>(1);
        BlockingQueue<Integer> q2 = new ArrayBlockingQueue<>(1);
        ExecutorService es = Executors.newCachedThreadPool();
        CountDownLatch latch = new CountDownLatch(2);

        final int WORKERS = Runtime.getRuntime().availableProcessors();

        for (int i = 0; i < WORKERS; i++) {
            es.submit(() -> {
                TimeUnit.MINUTES.sleep(10);
                return null;
            });
        }

        es.submit(() -> pingpong(q1, q2, latch));
        es.submit(() -> pingpong(q2, q1, latch));
        q1.offer(1);
        latch.await();

        for (int i = 0; i < WORKERS * 10; i++) { // iterate all carriers
            Integer ql = es.submit(() -> {
//                Object carrier = new FieldAccessor
                Thread thread = Thread.currentThread();
                TimeUnit.MILLISECONDS.sleep(1);
                assertEQ(Thread.currentThread(), thread);
                return new FieldAccessor(SharedSecrets.getJavaLangAccess().getWispTask(thread))
                        .access("carrier")
                        .access("worker")
                        .access("timerManager")
                        .access("queue")
                        .getInt("size");
            }).get();
            assertLessThanOrEqual(ql, WORKERS);
        }
    }

    private static Void pingpong(BlockingQueue<Integer> pingQ,
                                 BlockingQueue<Integer> pongQ,
                                 CountDownLatch latch) throws Exception {
        for (int i = 0; i < 100000; i++) {
            pingQ.poll(1, TimeUnit.HOURS);
            pongQ.offer(1);
        }
        stealInfo();
        latch.countDown();
        return null;
    }

    private static void stealInfo() throws ReflectiveOperationException {
        int stealCount = new FieldAccessor(SharedSecrets.getWispEngineAccess().getCurrentTask())
                .getInt("stealCount");
        System.out.println("stealCount = " + stealCount);
    }

    static class FieldAccessor {
        final Object obj;

        FieldAccessor(Object o) {
            this.obj = o;
        }

        FieldAccessor access(String fieldName) throws ReflectiveOperationException {
            Field field = obj.getClass().getDeclaredField(fieldName);
            field.setAccessible(true);
            return new FieldAccessor(field.get(obj));
        }

        int getInt(String fieldName) throws ReflectiveOperationException {
            Field field = obj.getClass().getDeclaredField(fieldName);
            field.setAccessible(true);
            return field.getInt(obj);
        }
    }
}
