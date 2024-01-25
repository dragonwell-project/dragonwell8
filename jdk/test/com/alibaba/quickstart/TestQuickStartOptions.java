/*
 * @test
 * @summary Test -Xquickstart options
 * @library /test/lib
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm/timeout=600 TestQuickStartOptions
 */

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class TestQuickStartOptions {
    public static void main(String[] args) throws Exception {
        TestQuickStartOptions test = new TestQuickStartOptions();
        test.verifyPathSetting();
        test.verifyInvalidOptCheck();
    }

    void verifyPathSetting() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=/a/b/c", "-Xquickstart:verbose" + Config.QUICKSTART_FEATURE_COMMA, "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("cache path is set from");
        output.shouldHaveExitValue(0);
    }

    void verifyInvalidOptCheck() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:jwmup,verbose", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Invalid -Xquickstart option");
        output.shouldHaveExitValue(1);
    }
}

