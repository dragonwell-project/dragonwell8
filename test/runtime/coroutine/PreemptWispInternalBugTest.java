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
 * @summary Verify wisp internal logic can not be preempted
 * @library /testlibrary
 * @run main PreemptWispInternalBugTest
 */

import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.HashMap;
import java.util.concurrent.FutureTask;
import static com.oracle.java.testlibrary.Asserts.*;


public class PreemptWispInternalBugTest {
	private static String[] tasks = new String[] {"toString", "addTimer"};

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
	        for (int i = 0; i < tasks.length; i++) {
		        ProcessBuilder pb = com.oracle.java.testlibrary.ProcessTools.createJavaProcessBuilder(
				        "-XX:+UseWisp2", "-XX:+UnlockDiagnosticVMOptions", "-XX:+VerboseWisp", "-XX:-Inline",
				        PreemptWispInternalBugTest.class.getName(), tasks[i]);
		        com.oracle.java.testlibrary.OutputAnalyzer output = new  com.oracle.java.testlibrary.OutputAnalyzer(pb.start());
		        output.shouldContain("[WISP] preempt was blocked, because wisp internal method on the stack");
	        }
            return;
        }

        FutureTask future = getTask(args[0]);
        WispEngine.dispatch(future);
        future.get();
    }

	private static FutureTask toStringTask() {
		return new FutureTask<>(() -> {
			long start = System.currentTimeMillis();
			Object task = SharedSecrets.getWispEngineAccess().getCurrentTask();
			while (System.currentTimeMillis() - start < 1000) {
				for (int i = 0; i < 100; i++) {
                    task.toString();
				}
			}
			return null;
		});
	}

	private static FutureTask addTimerTask() {
		return new FutureTask<>(() -> {
			long start = System.currentTimeMillis();
			while (System.currentTimeMillis() - start < 2000) {
				for (int i = 0; i < 100; i++) {
					SharedSecrets.getWispEngineAccess().addTimer(System.nanoTime() + 1008611);
					SharedSecrets.getWispEngineAccess().cancelTimer();
				}
			}
			return null;
		});
	}

	private static FutureTask getTask(String taskName) {
        switch (taskName) {
	            case "toString":
		        return toStringTask();
                    case "addTimer":
		        return addTimerTask();
	            default:
		        throw new IllegalArgumentException();
	    }
	}
}
