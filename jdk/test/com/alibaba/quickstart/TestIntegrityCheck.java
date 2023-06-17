/*
 * @test
 * @summary Test Integrity Check
 * @library /lib
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm/timeout=600 TestIntegrityCheck
 */

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestIntegrityCheck {
    private static String cachepath = System.getProperty("user.dir");
    public static void main(String[] args) throws Exception {
        TestIntegrityCheck test = new TestIntegrityCheck();
        cachepath = cachepath + "/integrityCheck";
        test.verifyIntegrity();
        test.verifyImageEnvChange();
    }

    void verifyIntegrity() throws Exception {
        runAsTracer();
        runAsReplayer();
    }

    void verifyImageEnvChange() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose,containerImageEnv=pouchid" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        pb.environment().put("pouchid", "123456");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Container image isn't the same");
        output.shouldHaveExitValue(0);
    }

    void runAsTracer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose,containerImageEnv=pouchid" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as tracer");
        output.shouldHaveExitValue(0);
    }

    void runAsReplayer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose,containerImageEnv=pouchid" + Config.QUICKSTART_FEATURE_COMMA, "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as replayer");
        output.shouldHaveExitValue(0);
    }

    void verifyOptionChange() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose,containerImageEnv=pouchid" + Config.QUICKSTART_FEATURE_COMMA, "-esa", "-XX:+IgnoreAppCDSDirCheck", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("JVM option count isn't the same");
        output.shouldHaveExitValue(0);
    }
}

