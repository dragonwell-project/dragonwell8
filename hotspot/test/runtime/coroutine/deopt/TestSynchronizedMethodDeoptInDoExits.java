/*
 * @test
 * @summary Test UnlockNode in `Parse::do_exits()` for synchronized method
 * @library /testlibrary /testlibrary/whitebox
 * @build sun.hotspot.WhiteBox
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -XX:-TieredCompilation -XX:CompileCommand=compileonly,TestSynchronizedMethodDeoptInDoExits::remove -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodDeoptInDoExits
 * @run main/othervm -XX:TieredStopAtLevel=1 -XX:CompileCommand=compileonly,TestSynchronizedMethodDeoptInDoExits::remove -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodDeoptInDoExits
 */


import sun.hotspot.WhiteBox;

import java.util.concurrent.ConcurrentHashMap;

/**
 * This test won't have problem, only for Testing Parse::do_exits().
 */

public class TestSynchronizedMethodDeoptInDoExits {
    private static WhiteBox WB = WhiteBox.getWhiteBox();

    private static TestSynchronizedMethodDeoptInDoExits tsmdide = new TestSynchronizedMethodDeoptInDoExits();

    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            final long totalMillis = 20 * 500;
            final long sleepMillis = 1;
            for (int i = 0; i < totalMillis / sleepMillis; i++) {
                try {
                    Thread.sleep(sleepMillis);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                // we are deoptimizing frames and do not mark the method not entrant.
                WB.deoptimizeAll();
            }
        }).start();
        MultiThreadWispRunner.run(
                "TestSynchronizedMethodDeoptInDoExits",
                4,
                4,
                100_000,
                100000,
                () -> trampoline());
    }

    private transient volatile ConcurrentHashMap<Object, Object> map = new ConcurrentHashMap<>();

    // this method will go through Parse::do_exits()'s UnlockNode when the `key` is null
    // we can use this guy for further testing
    public synchronized int remove(Object key, int p) {
        if (p == 100) {
            System.err.println("SYNCHRONIZED METHOD RE-EXECUTES FROM BCI 0!");
            System.exit(-100);
        } else {
            p = 100;
        }
        map.remove(key);
        return p;
    }

    private static volatile Object NULL = null;

    private static void trampoline() {
        try {
            tsmdide.remove(NULL, 3);
        } catch (NullPointerException npe) {

        }
    }

}
