/*
 * @test
 * @summary Test the flow to determine tracer or replayer
 * @library /lib
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm/timeout=600 TestDeterminingTracerOrReplayer
 */

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestDeterminingTracerOrReplayer {
    private static String cachepath = System.getProperty("user.dir");
    public static void main(String[] args) throws Exception {
        TestDeterminingTracerOrReplayer test = new TestDeterminingTracerOrReplayer();
        cachepath = cachepath + "/determine";
        test.verifyDetermine();
        test.verifyDestroy();
    }

    void verifyDetermine() throws Exception {
        destroyCache();
        runAsTracer();
        runAsReplayer();
    }

    void runAsTracer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as tracer");
        output.shouldHaveExitValue(0);
    }

    void runAsReplayer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as replayer");
        output.shouldHaveExitValue(0);
    }

    void destroyCache() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:destroy", "-Xquickstart:path=" + cachepath, "-Xquickstart:verbose", "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("destroy the cache folder");
        output.shouldHaveExitValue(0);
    }


    void verifyDestroy() throws Exception {
        runAsReplayer();
        destroyCache();
        runAsTracer();
    }
}

