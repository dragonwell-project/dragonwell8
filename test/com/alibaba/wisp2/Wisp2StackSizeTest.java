/*
 * @test
 * @library /lib/testlibrary
 * @summary test wisp2 stack_size
 * @requires os.family == "linux"
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 Wisp2StackSizeTest
 */


import static jdk.testlibrary.Asserts.*;

public class Wisp2StackSizeTest {
    public static void main(String[] args) {
        int prevDepth = 0, curDepth = 0;

        for (int i = 128; i <= 1024; i *= 2) {
            for (int j = 0; j < 10; j++) {
                curDepth = Math.max(curDepth, RecTester.tryRec(i * 1024));
            }

            if (prevDepth != 0) {
                // roughly curDepth / prevDepth ~= 2
                assertTrue(Math.abs(curDepth - 2 * prevDepth) < prevDepth, i + " " + curDepth + " " + prevDepth);
            }

            prevDepth = curDepth;
        }
    }
}

class RecTester implements Runnable {
    /**
     * Run recursion on {@link Thread} with given stackSize.
     *
     * @return recursion depth before {@link StackOverflowError}
     */
    public static int tryRec(long stackSize) {
        RecTester r = new RecTester();
        Thread t = new Thread(null, r, "", stackSize);
        t.start();
        try {
            t.join();
        } catch (InterruptedException ignored) {
        }
        return r.depth;
    }

    public int depth = 0;

    @Override
    public void run() {
        try {
            inner();
        } catch (StackOverflowError ignored) {
        }
    }

    private void inner() {
        depth++;
        inner();
    }
}
