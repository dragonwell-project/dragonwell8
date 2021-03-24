/*
 * @test
 * @summary Test MonitorExit nodes of synchronized methods for C1
 * @library /testlibrary /testlibrary/whitebox
 * @build sun.hotspot.WhiteBox
 * @run driver ClassFileInstaller sun.hotspot.WhiteBox sun.hotspot.WhiteBox$WhiteBoxPermission
 * @run main/othervm -XX:TieredStopAtLevel=1 -XX:CompileCommand=compileonly,TestSynchronizedMethodC1Crash::remove -XX:+PrintIR -XX:+PrintDeoptimizationDetails -XX:+IgnoreUnrecognizedVMOptions -Xbootclasspath/a:. -XX:+UnlockDiagnosticVMOptions -XX:+WhiteBoxAPI -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestSynchronizedMethodC1Crash
 */


import sun.hotspot.WhiteBox;

/**
 * Note: this method is imitating `io.vertx.sqlclient.impl.SocketConnectionBase::lambda\$init\$0`,
 *   which can reproduce the crash at
 *   LinearScan::resolve_exception_edge().
 */

public class TestSynchronizedMethodC1Crash {
    private static WhiteBox WB = WhiteBox.getWhiteBox();

    private static TestSynchronizedMethodC1Crash tsmcc = new TestSynchronizedMethodC1Crash();

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
                // WB.deoptimizeAll();
            }
        }).start();
        MultiThreadWispRunner.run(
                "TestSynchronizedMethodC1Crash",
                4,
                4,
                100_000,
                100000,
                () -> trampoline());
    }

    public static int i;
    protected void handleClose(Throwable t) {
        synchronized (this) {
            if (t != null) {
                t.getCause();
            }
        }
    }
    private synchronized void handleException(Throwable t) {
        if (t instanceof NullPointerException) {
            NullPointerException err = (NullPointerException) t;
            t = err.getCause();
        }
        handleClose(t);
    }
    public void mayThrow() {
        if (i++ % 1000 == 1) {
            throw new NullPointerException();
        } else {
            // do nothing
        }
    }
    public void remove() {
        try {
            mayThrow();
        } catch (Exception e) {
            handleException(e);
        }
    }

    private static void trampoline() {
        tsmcc.remove();
    }

}
