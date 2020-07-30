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
 * @summary Test WispCarrier's multi-task schedule
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.wisp.transparentWispSwitch=true ExecutionTest
 */


import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * test the coroutine execute engine
 */
public class ExecutionTest {

    static int finishCnt = 0;

    public static void main(String[] args) {
        Callable handler = () -> {
            /**
             * transform from:
             * if ((nodeA() + nodeB()).equals("A:done\nB1:done\nB2:done\n"))
             *     throw new Error("result error");
             */
            FutureTask<String> futureA = new FutureTask<>(ExecutionTest::nodeA);
            FutureTask<String> futureB = new FutureTask<>(ExecutionTest::nodeB);
            WispEngine.current().execute(futureA);
            WispEngine.current().execute(futureB);
            String result;
            try {
                result = futureA.get() + futureB.get();
            } catch (Exception e) {
                throw new Error("exception during task execution");
            }

            if (!result.equals("A:done\nB1:done\nB2:done\n"))
                throw new Error("result error");

            finishCnt++;
            return null;
        };

        /**
         * <pre>
         * WispCarrier.local().createTask(() -> {
         *     while (client = accept()) {
         *         WispCarrier.local().createTask(()->handler(client), "client handler");
         *     }
         * }, "accepter");
         *
         * a web server can using a accepter {@link WispTask}  create handler for every request
         * we don't have request, create 3 handler manually
         *
         * every handler is a tree root
         *         handler         handler        handler
         *         /    \           /   \          /   \
         *        A     B          A    B         A    B
         *             / \             / \            / \
         *            B1 B2           B1 B2          B1 B2
         * </pre>
         *
         * look into a particular tree:
         * B1 and B2 is created when nodeB is executed after nodeA blocked
         * business code drive the node create ..
         *
         * then B1 block; B2 executed and block
         * then the engine block on pump about 100ms..
         *
         *
         * 3 tree execute concurrently
         *
         */
        for (int i = 0; i < 3; i++) {
            WispEngine.current().execute(() -> {
                try {
                    handler.call();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            });
        }

        SharedSecrets.getWispEngineAccess().sleep(200);
        // the 3 tree should finish after 100ms+, but the switch need warm up, give more time..
        if (finishCnt != 3) throw new Error("not finished");
    }

    static String nodeA() {
        // node A could also be a function call after B created
        return slowReq("A");
    }

    static String nodeB() {
        /*
          transform from:
          return slowReq("B1") + slowReq("B2");
         */
        FutureTask<String> future1 = new FutureTask<>(() -> slowReq("B1"));
        FutureTask<String> future2 = new FutureTask<>(() -> slowReq("B2"));
        WispEngine.current().execute(future1);
        WispEngine.current().execute(future2);

        try {
            return future1.get() + future2.get();
        } catch (Exception e) {
            throw new Error("exception during task execution");
        }
    }

    // mimic an IO call
    static String slowReq(String arg) {
        SharedSecrets.getWispEngineAccess().sleep(100); // only block current coroutine
        return arg + ":done\n";
    }
}
