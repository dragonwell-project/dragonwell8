/*
 * @test
 * @summary Test Environment Variable Dump
 * @library /lib /lib/testlibrary
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @run main/othervm/timeout=600 TestEnvVariableDump
 */
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import java.io.*;
import java.util.Map;
import static jdk.testlibrary.Asserts.*;

public class TestEnvVariableDump {
    private static String cachepath = System.getProperty("user.dir");
    private static final String metadata_file = "metadata";
    private static final String hadoop_home = "/hadoop_home";
    private static final String hadoop_common_home = "/hadoop_common_home";

    public static void main(String[] args) throws Exception {
        TestEnvVariableDump test = new TestEnvVariableDump();
        cachepath = cachepath + "/testEnvVariableDump";
        test.runAsTracer();
        test.checkMetadataFile(cachepath + "/" + metadata_file);
    }

    private void runAsTracer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose", "-Dcom.alibaba.cds.cp.reloc.envs=PWD,HADOOP_HOME,HADOOP_COMMON_HOME", "-XX:+IgnoreAppCDSDirCheck", "-version");
        Map<String, String> env = pb.environment();
        env.put("PWD", cachepath);
        env.put("HADOOP_HOME", cachepath + hadoop_home);
        env.put("HADOOP_COMMON_HOME", cachepath + hadoop_common_home);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        output.shouldContain("Running as tracer");
        output.shouldHaveExitValue(0);
    }

    private void checkMetadataFile(String filepath) {
        boolean pwd_checked = false;
        boolean hadoop_home_checked = false;
        boolean hadoop_common_home_checked = false;
        try {
            BufferedReader in = new BufferedReader(new FileReader(filepath));
            String str;
            while ((str = in.readLine()) != null) {
                if(str.contains("ENV.PWD")) {
                    pwd_checked = str.contains(cachepath);
                }
                if(str.contains("ENV.HADOOP_HOME")) {
                    hadoop_home_checked = str.contains(cachepath + hadoop_home);
                }
                if(str.contains("ENV.HADOOP_COMMON_HOME")) {
                    hadoop_common_home_checked = str.contains(cachepath + hadoop_common_home);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        assertTrue(pwd_checked && hadoop_home_checked && hadoop_common_home_checked);
    }
}