/*
 * @test
 * @summary Test deoptimization of _monitorexit bytecode for synchronized block
 * @library /testlibrary /testlibrary/whitebox
 * @build sun.hotspot.WhiteBox
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -XX:TieredStopAtLevel=1 -XX:CompileCommand=compileonly,TestMonitorExitDeopt::wrapper -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestMonitorExitDeopt
 * @run main/othervm -XX:-TieredCompilation -XX:CompileCommand=compileonly,TestMonitorExitDeopt::wrapper -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestMonitorExitDeopt
 */

import sun.hotspot.WhiteBox;

public class TestMonitorExitDeopt {
    private static WhiteBox WB = WhiteBox.getWhiteBox();

    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            for (int i = 0; i < 20; i++) {
                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                WB.deoptimizeAll();
            }
        }).start();
        MultiThreadWispRunner.run(
                "TestMonitorExitDeopt",
                4,
                4,
                200_000,
                100000,
                () -> wrapper());
    }

    private volatile static byte WRITE_VAL = 1;

    static final Object O_1 = new Object();
    static final Object O_2 = new Object();
    private static void wrapper() {
        synchronized (O_1) {
            synchronized (O_2) {
                for (int i = 0; i < bs.length; i++) {
                    bs[i] = WRITE_VAL;
                }
            }
            // THIS WILL CAUSE A CRASH WHEN DEOPTIMIZING
        }
    }

    static byte[] bs = new byte[12];
}
