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

/* @test
 * @summary unit tests for steal in concurrent situation
 * @run junit/othervm/timeout=300 -XX:+EnableCoroutine ConcurrentStealTest
 */

import org.junit.Test;

import java.dyn.Coroutine;
import java.dyn.CoroutineSupport;
import java.util.Arrays;
import java.util.Comparator;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;

import static org.junit.Assert.*;

public class ConcurrentStealTest {

    @Test
    public void stealRunning() throws Exception {
        Coroutine[] coro = new Coroutine[1];

        CountDownLatch cdl = new CountDownLatch(1);

        Thread t = new Thread(() -> {
            coro[0] = new Coroutine(() -> {
                try {
                    cdl.countDown();
                    Thread.sleep(10000000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            });
            Coroutine.yieldTo(coro[0]);
        }, "stealRunning");
        t.setDaemon(true);
        t.start();

        cdl.await();

        Thread.sleep(500);

        assertEquals(coro[0].steal(false), Coroutine.StealResult.FAIL_BY_STATUS);
    }

    final static int THREADS = Math.min(Runtime.getRuntime().availableProcessors(), 8);
    final static int CORO_PER_TH = 20;
    final static int TIMEOUT = 10;

    @Test
    public void randomSteal() throws Exception {
        CoroutineAdaptor[] coro = new CoroutineAdaptor[THREADS * CORO_PER_TH];
        Stat[] cnt = new Stat[THREADS];
        StealWorker[] stealWorkers = new StealWorker[THREADS];

        AtomicInteger sync = new AtomicInteger();

        for (int th = 0; th < THREADS; th++) {
            cnt[th] = new Stat();
            stealWorkers[th] = new StealWorker(coro, cnt , sync, th, stealWorkers);
            Thread t = new Thread(stealWorkers[th], "randomSteal-" + th);
            t.setDaemon(true);
            t.start();
        }

        while (sync.get() != THREADS) {
        }

        long start = System.nanoTime();
        while (System.nanoTime() - start < TimeUnit.SECONDS.toNanos(TIMEOUT)) {
        }

        for (int i = 0; i < cnt.length; i++) {
            cnt[i].stealCnt /= TIMEOUT;
            cnt[i].yieldCnt /= TIMEOUT;
            cnt[i].stealFailedCnt /= TIMEOUT;
        }
        System.out.println(Arrays.toString(cnt));
    }

    class StealWorker implements Runnable {
        CoroutineAdaptor[] coro;
        Coroutine threadCoro;
        public ConcurrentSkipListSet<CoroutineAdaptor> stealables = new ConcurrentSkipListSet<>(new Comparator<CoroutineAdaptor>() {
            @Override
            public int compare(CoroutineAdaptor o1, CoroutineAdaptor o2) {
                return  Long.compare(o1.id, o2.id);
            }
        });
        Stat[] cnt;
        StealWorker[] workers;
        AtomicInteger sync;
        int cth;

        StealWorker(CoroutineAdaptor[] coro, Stat[] cnt, AtomicInteger sync, int cth, StealWorker[] workers) {
            this.coro = coro;
            this.cnt = cnt;
            this.sync = sync;
            this.cth = cth;
            this.workers = workers;
        }

        @Override
        public void run() {
            threadCoro =  Thread.currentThread().getCoroutineSupport().threadCoroutine();

            for (int i = 0; i < CORO_PER_TH; i++) {
                coro[CORO_PER_TH * cth + i] = new CoroutineAdaptor(() -> {
                    while (true) {
                        Coroutine.yieldTo(threadCoro);
                        cnt[cth].yieldCnt++;
                    }
                });
            }
            for (int i = 0; i < CORO_PER_TH; i++) {
                CoroutineAdaptor coroutineAdaptor =  new CoroutineAdaptor(() ->{
                    while (true) {
                        yield();
                    }
                });
                Coroutine.yieldTo(coroutineAdaptor);
                stealables.add(coroutineAdaptor);
            }


            sync.incrementAndGet();
            while (sync.get() != THREADS) {
            }

            runRandom(System.nanoTime(), (cth + 1) % THREADS);
        }


        void yield() {
            Coroutine.yieldTo(coro[CORO_PER_TH * cth +  ThreadLocalRandom.current().nextInt(CORO_PER_TH)]);
        }

        void runRandom(long start, int nxt) {
            while (System.nanoTime() - start < TimeUnit.SECONDS.toNanos(TIMEOUT)) {
                for (int i = 0; i < 2; i ++) {
                    if(i != 1) {
                        yield();
                        cnt[cth].yieldCnt++;
                    } else {
                        CoroutineAdaptor target = workers[nxt].stealables.pollFirst();
                        if (target != null && target.steal(true) == Coroutine.StealResult.SUCCESS) {
                            stealables.add(target);
                            cnt[cth].stealCnt++;
                        } else {
                            if (target != null)
                                workers[nxt].stealables.add(target);
                            cnt[cth].stealFailedCnt++;
                        }
                    }
                }
            }
        }
    }

    static AtomicInteger seq = new AtomicInteger(0);

    class CoroutineAdaptor extends Coroutine {
        long id;
        CoroutineAdaptor(Runnable runnable) {
            super(runnable);
            id = seq.addAndGet(1);
        }
    }

    class Stat {
        int stealCnt;
        int yieldCnt;
        int stealFailedCnt;

        @Override
        public String toString() {
            return "\n < steal Cnt " + stealCnt + ">, < yieldCnt" + yieldCnt + "> , < stealFailedCnt " + stealFailedCnt + " > ";
        }
    }
}
