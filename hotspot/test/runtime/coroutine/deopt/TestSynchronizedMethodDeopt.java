/*
 * @test
 * @summary Test deoptimization of _return bytecode for synchronized method
 * @library /testlibrary /testlibrary/whitebox
 * @build sun.hotspot.WhiteBox
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -XX:-TieredCompilation -XX:CompileCommand=compileonly,TestSynchronizedMethodDeopt::syncMethodWrite -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodDeopt
 * @run main/othervm -XX:TieredStopAtLevel=1 -XX:CompileCommand=compileonly,TestSynchronizedMethodDeopt::syncMethodWrite -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodDeopt
 */

import sun.hotspot.WhiteBox;

/**
 * This test can reproduce two problems:
 * 1. C1 deopt failed at _monitorexit during _return
 * 2. C2 deopt fastdebug assertion fail:
 *    assert(false) failed: Non-balanced monitor enter/exit! Likely JNI locking
 *    It shows that _monitorenter executes once and _monitorexit executes twice
 *    So C2 will throw an IllegalMonitorStateException in release mode.
 */

public class TestSynchronizedMethodDeopt {
    private static WhiteBox WB = WhiteBox.getWhiteBox();

    public static void main(String[] args) throws Exception {
        new Thread(() -> {
            for (int i = 0; i < 20; i++) {
                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                // we are deoptimizing frames including `syncMethodWrite` continuously,
                // and do not mark the method not entrant.
                WB.deoptimizeAll();
            }
        }).start();
        MultiThreadWispRunner.run(
                "TestSynchronizedMethodDeopt",
                4,
                4,
                200_000,
                100000,
                () -> syncMethodWrite(3));
    }

    private volatile static byte WRITE_VAL = 1;

    private static synchronized int syncMethodWrite(int p) {
        // THIS METHOD'S _MONITOREXIT WILL CRASH

        // In C2's code, we will set the SynchronizationEntryBCI to -1
        // So if we deopt at monitorexit inside _return bytecode,
        // the interpreter will restart the whole method from bci == 0.
        // In C1's code, if a method isn't a inlined method, the bci has
        // been set to the _return's bci instead of SynchronizationEntryBCI,
        // so it's a UB and will crash.
        // In theory I think C2 also has problem because the reexecution of the
        // whole method but I have no evidence. so I add a bomb to prove it's true:

        // =====================================================================
        //                          THE SPECIAL BOMB
        // =====================================================================
        // The syncMethodWrite() will write things, and the `p` will be changed
        // when entering into the method. If syncMethodWrite() get deopted and
        // re-executed in C2, we will detect the value change of `p`.
        // so we will step on this bomb and the test fails.
        if (p == 100) {
            System.err.println("SYNCHRONIZED METHOD RE-EXECUTES FROM BCI 0!");
            // In coroutine this guy will be eaten by wisp. So it is telling you AN ERROR
            // IS HAPPENING NOW only from code level.
//            throw new Error("synchronized method deopted at monitorexit in _return, " +
//                            "and the whole method reexecute from the bci == 0!");
            System.exit(-100);
        } else {
            p = 100;
        }

        for (int i = 0; i < bs.length; i++) {
            bs[i] = WRITE_VAL;
        }

        return p;
    }

    static byte[] bs = new byte[12];
}
