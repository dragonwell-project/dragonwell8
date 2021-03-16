/*
 * @test
 * @summary Test the GC failure from ElasticHeap jcmd due to GC locker
 * @library /testlibrary /testlibrary/whitebox
 * @build TestElasticHeapGCLocker
 * @run main ClassFileInstaller sun.hotspot.WhiteBox
 * @run main/othervm/timeout=100 -Xmx1g -Xms1g TestElasticHeapGCLocker
 */

import java.util.HashMap;
import java.util.Map;
import java.util.zip.Deflater;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;
import sun.hotspot.WhiteBox;

public class TestElasticHeapGCLocker {

    public static void main(String[] args) throws Exception {
        ProcessBuilder pb;
        OutputAnalyzer output;

        pb = ProcessTools.createJavaProcessBuilder(
            "-Xbootclasspath/a:.",
            "-XX:+UnlockDiagnosticVMOptions", "-XX:+WhiteBoxAPI",
            "-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx1000m", "-Xms1000m",
            "-XX:+PrintGCDetails",
            "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            ElasticHeapGCLockerTest.class.getName());

        output = new OutputAnalyzer(pb.start());
        System.out.println(output.getOutput());
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    static class ElasticHeapGCLockerTest {
        private static WhiteBox WB = WhiteBox.getWhiteBox();

        private static OutputAnalyzer triggerJcmd(String arg1, String arg2, String arg3) throws Exception {
            String pid = Integer.toString(ProcessTools.getProcessId());
            JDKToolLauncher jcmd = JDKToolLauncher.create("jcmd")
                                                  .addToolArg(pid);
            jcmd.addToolArg(arg1);
            jcmd.addToolArg(arg2);
            jcmd.addToolArg(arg3);
            ProcessBuilder pb = new ProcessBuilder(jcmd.getCommand());
            return new OutputAnalyzer(pb.start());
        }

        public static void main(String args[]) throws Exception {
            // Promote 300M into old gen
            Object[] root = new Object[3 * 1024];
            for (int i = 0; i < 3 * 1024; i++) {
                root[i] = new byte[100*1024];
            }

            System.gc();

            WB.gcLockCritical();

            OutputAnalyzer output = triggerJcmd("GC.elastic_heap", "softmx_percent=60", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("cannot invoke GC");
            output.shouldContain("[GC.elastic_heap: inactive]");

            WB.gcUnlockCritical();

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=60", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");

            WB.gcLockCritical();

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=0", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("cannot invoke GC");
            output.shouldContain("[GC.elastic_heap: in softmx mode]");

            WB.gcUnlockCritical();

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=0", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: inactive]");

            System.exit(0);
        }
    }
 }
