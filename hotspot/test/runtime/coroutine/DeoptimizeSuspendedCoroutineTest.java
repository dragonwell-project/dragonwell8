/*
 * Copyright (c) 2026 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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
 * @test DeoptimizeSuspendedCoroutineTest
 * @summary Deoptimize marked methods on parked platform and Wisp coroutine stacks
 * @library /testlibrary /testlibrary/whitebox
 * @build DeoptimizeSuspendedCoroutineTest sun.hotspot.WhiteBox
 * @run main ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -Dtest.compLevel=1 -XX:TieredStopAtLevel=1
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::compiledBody
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::suspendHere
 *      -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *      DeoptimizeSuspendedCoroutineTest
 * @run main/othervm -Dtest.compLevel=4 -XX:-TieredCompilation
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::compiledBody
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::suspendHere
 *      -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *      DeoptimizeSuspendedCoroutineTest
 * @run main/othervm -Dtest.compLevel=1 -XX:TieredStopAtLevel=1
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::compiledBody
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::suspendHere
 *      -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2
 *      -Dcom.alibaba.wisp.carrierEngines=1 -Xbootclasspath/a:.
 *      -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *      DeoptimizeSuspendedCoroutineTest
 * @run main/othervm -Dtest.compLevel=4 -XX:-TieredCompilation
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::compiledBody
 *      -XX:CompileCommand=dontinline,DeoptimizeSuspendedCoroutineTest::suspendHere
 *      -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2
 *      -Dcom.alibaba.wisp.carrierEngines=1 -Xbootclasspath/a:.
 *      -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI
 *      DeoptimizeSuspendedCoroutineTest
 * @requires os.family == "linux"
 */

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;

import sun.hotspot.WhiteBox;
import sun.misc.Unsafe;

public class DeoptimizeSuspendedCoroutineTest {
    private static final int WARMUP_ITERATIONS = 100_000;
    private static final long COMPILATION_TIMEOUT_MILLIS = 10_000;
    private static final long TASK_TIMEOUT_SECONDS = 10;
    private static final boolean USE_WISP = Boolean.getBoolean("com.alibaba.wisp.allThreadAsWisp");

    private static final WhiteBox WB = WhiteBox.getWhiteBox();
    private static final Unsafe UNSAFE = getUnsafe();
    private static final Method COMPILED_BODY = findMethod("compiledBody");
    private static final int[] COMPILED_VALUE = {1};
    private static final CountDownLatch PARK_REACHED = new CountDownLatch(1);
    private static final CountDownLatch CARRIER_RELEASED = new CountDownLatch(1);
    private static final CountDownLatch TASK_COMPLETED = new CountDownLatch(1);

    private static volatile Thread taskThread;
    private static volatile boolean shouldPark;
    private static volatile boolean completed;
    private static volatile int result;
    private static volatile Throwable failure;

    public static void main(String[] args) throws Throwable {
        int compilationLevel = Integer.parseInt(System.getProperty("test.compLevel"));
        System.out.println("mode=" + (USE_WISP ? "wisp" : "platform")
                + ", compilationLevel=" + compilationLevel);

        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            if (compiledBody() != 1) {
                throw new RuntimeException("Unexpected value during warmup");
            }
        }

        WB.deoptimizeMethod(COMPILED_BODY);
        if (!WB.enqueueMethodForCompilation(COMPILED_BODY, compilationLevel)) {
            throw new RuntimeException("Could not enqueue compiledBody at level " + compilationLevel);
        }
        waitUntilCompiled(compilationLevel);

        shouldPark = true;
        Runnable task = () -> {
            taskThread = Thread.currentThread();
            try {
                result = compiledBody();
                completed = true;
            } catch (Throwable throwable) {
                failure = throwable;
            } finally {
                TASK_COMPLETED.countDown();
            }
        };
        new Thread(task, "deoptimize-suspended-test").start();

        if (!PARK_REACHED.await(TASK_TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
            throw new RuntimeException("Test task did not reach park inside compiledBody");
        }
        if (USE_WISP) {
            new Thread(CARRIER_RELEASED::countDown, "carrier-release-probe").start();
            if (!CARRIER_RELEASED.await(TASK_TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
                throw new RuntimeException("Wisp task did not release its carrier after park");
            }
        } else {
            waitUntilPlatformThreadParked();
        }
        if (completed || failure != null) {
            throw new RuntimeException("Test task completed before deoptimization", failure);
        }

        setCompiledValue(new int[]{2});
        int deoptimizedMethods = WB.deoptimizeMethod(COMPILED_BODY);
        if (deoptimizedMethods == 0) {
            throw new RuntimeException("WhiteBox did not mark compiledBody for deoptimization");
        }
        if (WB.isMethodCompiled(COMPILED_BODY)) {
            throw new RuntimeException("WhiteBox did not deoptimize compiledBody");
        }

        shouldPark = false;
        LockSupport.unpark(taskThread);
        if (!TASK_COMPLETED.await(TASK_TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
            throw new RuntimeException("Test task did not complete after unpark");
        }

        if (failure != null) {
            throw new RuntimeException("Test task failed", failure);
        }
        if (!completed) {
            throw new RuntimeException("Test task did not complete after unpark");
        }
        if (result != 2) {
            throw new RuntimeException("Suspended compiled frame used stale static-final value: " + result);
        }
    }

    private static int compiledBody() {
        suspendHere();
        return COMPILED_VALUE[0];
    }

    private static void suspendHere() {
        if (shouldPark) {
            PARK_REACHED.countDown();
            while (shouldPark) {
                LockSupport.park();
            }
        }
    }

    private static void waitUntilCompiled(int expectedLevel) throws Exception {
        long deadline = System.currentTimeMillis() + COMPILATION_TIMEOUT_MILLIS;
        while (!WB.isMethodCompiled(COMPILED_BODY) && System.currentTimeMillis() < deadline) {
            Thread.sleep(10);
        }
        if (!WB.isMethodCompiled(COMPILED_BODY)) {
            throw new RuntimeException("compiledBody was not compiled at level " + expectedLevel);
        }
        int actualLevel = WB.getMethodCompilationLevel(COMPILED_BODY);
        if (actualLevel != expectedLevel) {
            throw new RuntimeException("Expected compilation level " + expectedLevel + ", got " + actualLevel);
        }
    }

    private static void waitUntilPlatformThreadParked() throws Exception {
        long deadline = System.currentTimeMillis() + TimeUnit.SECONDS.toMillis(TASK_TIMEOUT_SECONDS);
        while (taskThread.getState() != Thread.State.WAITING && System.currentTimeMillis() < deadline) {
            Thread.sleep(10);
        }
        if (taskThread.getState() != Thread.State.WAITING) {
            throw new RuntimeException("Platform thread did not park");
        }
    }

    private static Method findMethod(String name) {
        try {
            return DeoptimizeSuspendedCoroutineTest.class.getDeclaredMethod(name);
        } catch (ReflectiveOperationException exception) {
            throw new ExceptionInInitializerError(exception);
        }
    }

    private static Unsafe getUnsafe() {
        try {
            Field field = Unsafe.class.getDeclaredField("theUnsafe");
            field.setAccessible(true);
            return (Unsafe) field.get(null);
        } catch (ReflectiveOperationException exception) {
            throw new ExceptionInInitializerError(exception);
        }
    }

    private static void setCompiledValue(int[] value) {
        try {
            Field field = DeoptimizeSuspendedCoroutineTest.class.getDeclaredField("COMPILED_VALUE");
            Object base = UNSAFE.staticFieldBase(field);
            long offset = UNSAFE.staticFieldOffset(field);
            UNSAFE.putObjectVolatile(base, offset, value);
        } catch (ReflectiveOperationException exception) {
            throw new RuntimeException(exception);
        }
    }
}
