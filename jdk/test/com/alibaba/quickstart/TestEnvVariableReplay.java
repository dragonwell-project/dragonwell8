/*
 * @test
 * @summary Test Environment Variable Dump
 * @library /lib /lib/testlibrary
 * @requires os.arch=="amd64" | os.arch=="aarch64"
 * @build TestEnvClass
 * @run driver ClassFileInstaller -jar test.jar TestEnvClass
 * @run main/othervm/timeout=600 TestEnvVariableReplay
 */
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;
import java.io.*;
import java.util.Map;
import static jdk.testlibrary.Asserts.*;

public class TestEnvVariableReplay {

    private static String TESTJAR = "/test.jar";
    private static final String TESTNAME = "TestEnvClass";
    private static final String TESTCLASS = TESTNAME + ".class";

    private static String cachepath = System.getProperty("user.dir");
    private static String newDir = System.getProperty("user.dir");
    private static final String metadata_file = "metadata";
    private static final String origin_home1 = "/origin_home1";
    private static final String origin_home2 = "/origin_home2";
    private static final String new_home1 = "/new_home1";
    private static final String new_home2 = "/new_home2_long";
    private static String classpath = "";

    public static void main(String[] args) throws Exception {
        TestEnvVariableReplay test = new TestEnvVariableReplay();
        cachepath = cachepath + "/testEnvVariableDump";
        newDir = newDir + "/newDir";
        test.makeAllDir();
        test.moveJar(System.getProperty("user.dir") + TESTJAR , newDir + origin_home1 + TESTJAR);
        test.buildOriginClassPath();
        test.runAsTracer();
        test.moveJar(newDir + origin_home1 + TESTJAR, newDir + new_home1 + TESTJAR);
        test.buildNewClassPath();
        test.runAsReplayer();
    }

    private void makeAllDir() {
        mkdir(newDir);
        mkdir(newDir + origin_home1);
        mkdir(newDir + origin_home2);
        mkdir(newDir + new_home1);
        mkdir(newDir + new_home2);
    }

    private void buildOriginClassPath() {
        classpath = cachepath + ":" + newDir + origin_home1 + TESTJAR + ":" + newDir + origin_home2;
    }

    private void buildNewClassPath() {
        classpath = cachepath + ":" + newDir + new_home1 + TESTJAR + ":" + newDir + new_home2;
    }

    private void moveJar(String from, String to) {
        File file = new File(from);
        assertTrue (file.renameTo(new File(to)));
        file.delete();
    }

    private void mkdir(String dir_path){
        try {
            File dir = new File(dir_path);
            dir.mkdir();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void runAsTracer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose", "-Dcom.alibaba.cds.cp.reloc.envs=HOME1,HOME2", "-XX:+IgnoreAppCDSDirCheck", "-cp", classpath, TESTNAME);
        Map<String, String> env = pb.environment();
        env.put("HOME1", newDir + origin_home1);
        env.put("HOME2", newDir + origin_home2);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Running as tracer");
        output.shouldHaveExitValue(0);
    }

    private void runAsReplayer() throws Exception {
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xquickstart:path=" + cachepath, "-Xquickstart:verbose", "-Dcom.alibaba.cds.cp.reloc.envs=HOME1,HOME2", "-XX:+IgnoreAppCDSDirCheck", "-XX:+TraceClassPaths", "-cp", classpath, TESTNAME);
        Map<String, String> env = pb.environment();
        env.put("HOME1", newDir + new_home1);
        env.put("HOME2", newDir + new_home2);
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        output.shouldContain("Running as replayer");
        output.shouldHaveExitValue(0);
    }

}