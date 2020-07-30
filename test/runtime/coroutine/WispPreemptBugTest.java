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
 * @summary verify vm not crash when we're preempted frequently
 * @requires os.family == "linux"
 * @run main/othervm -XX:ActiveProcessorCount=1 -XX:+UseWisp2 WispPreemptBugTest
 */

import java.security.MessageDigest;
import java.util.Date;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class WispPreemptBugTest {

    public static void main(String[] args) throws Exception {
        final int TIMEOUT = 3000;
        final long start = System.currentTimeMillis();
        final byte[] data = new Date().toString().getBytes();
        ExecutorService es = Executors.newCachedThreadPool();
        for (int i = 0; i < 2; i++) {
            es.submit(() -> {
                try {
                    while (System.currentTimeMillis() - start < TIMEOUT) {
                        MessageDigest.getInstance("md5").digest(data);
                    }
                } catch (Throwable t) {
                    t.printStackTrace();
                }
            });
        }
        Thread.sleep(TIMEOUT);
    }
}
