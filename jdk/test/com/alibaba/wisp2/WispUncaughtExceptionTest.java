/*
 * @test
 * @summary Test uncaught exception can be handled by thread's UncaughtExceptionHandler
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2  WispUncaughtExceptionTest
 */

import static jdk.testlibrary.Asserts.assertTrue;

public class WispUncaughtExceptionTest {

    public static void main(String[] args) throws Exception {
        runCase(false);
        // verify that event if the code throws exception in handler,
        // Wisp still works.
        runCase(true);
    }

    private static void runCase(boolean throwInHandler) throws Exception {
        boolean[] hasBennDispatch = new boolean[1];
        Thread thread = new Thread(() -> {
            Thread.currentThread().setUncaughtExceptionHandler((t, ex) -> {
                hasBennDispatch[0] = true;
                if (throwInHandler) {
                    throw new RuntimeException();
                }
            });
            throw new Error();
        });
        thread.start();
        thread.join();

        assertTrue(hasBennDispatch[0]);
    }
}
