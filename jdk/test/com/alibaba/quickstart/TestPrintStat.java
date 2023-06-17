/*
 * @test
 * @summary Test -Xquickstart:printStat option
 * @library /lib
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm/timeout=600 TestPrintStat
 */

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestPrintStat {
    private static String cachepath = System.getProperty("user.dir");
    public static void main(String[] args) throws Exception {
        TestPrintStat test = new TestPrintStat();
        cachepath = cachepath + "/printstat";
        test.runWithoutCache();
        test.runAsTracer();
        test.runAsReplayerWithPrintStat();
    }

    void runWithoutCache() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:printStat" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("There is no cache in " + cachepath);
        output.shouldHaveExitValue(0);
    }

    void runAsTracer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as tracer");
        output.shouldHaveExitValue(0);
    }

    void runAsReplayerWithPrintStat() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:printStat" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Current statistics for cache " + cachepath);
        output.shouldHaveExitValue(0);
    }
}

