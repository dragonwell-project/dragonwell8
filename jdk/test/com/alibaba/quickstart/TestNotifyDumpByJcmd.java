/*
 * @test
 * @modules java.base/sun.security.action
 * @summary Test dumping using jcmd
 * @library /lib
 * @build TestDump
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run driver ClassFileInstaller -jar test-notifyDumpByJcmd.jar TestDump TestDump$Policy TestDump$ClassLoadingPolicy TestDump$WatcherThread
 * @run main/othervm/timeout=600 TestNotifyDumpByJcmd
 */

import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import sun.security.action.GetPropertyAction;

import java.io.File;
import java.security.AccessController;

public class TestNotifyDumpByJcmd {

    private static final String TESTJAR = "./test-notifyDumpByJcmd.jar";
    private static final String TESTCLASS = "TestDump";

    public static void main(String[] args) throws Exception {
        String dir = AccessController.doPrivileged(new GetPropertyAction("test.classes"));
        destroyCache(dir);
        TestNotifyDumpByJcmd.verifyPathSetting(dir);
        new File(dir).delete();
    }

    static void verifyPathSetting(String parentDir) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                "-Xquickstart:path=" + parentDir + "/quickstartcache",
                "-Xquickstart:verbose",
                // In sleeping condition there is no classloading happens,
                // we will consider it as the start-up finish
                "-DcheckIntervalMS=" + TestDump.SLEEP_MILLIS,
                "-DtestJcmd=true",
                "-cp",
                TESTJAR,
                TESTCLASS);
        pb.redirectErrorStream(true);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println("[Child Output] " + output.getOutput());
        output.shouldContain(TestDump.ANCHOR);
        output.shouldHaveExitValue(0);
    }

    static void destroyCache(String parentDir) throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder(
                "-Xquickstart:destroy",
                "-Xquickstart:path=" + parentDir + "/quickstartcache",
                "-Xquickstart:verbose", "-version");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("destroy the cache folder");
        output.shouldHaveExitValue(0);
    }

}
