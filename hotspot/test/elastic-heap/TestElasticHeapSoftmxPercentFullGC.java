import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.File;
import com.oracle.java.testlibrary.*;

/* @test
 * @summary test elastic-heap setting softmx percent in full gc with RSS check
 * @library /testlibrary /testlibrary/whitebox
 * @build TestElasticHeapSoftmxPercentFullGC
 * @run main/othervm/timeout=200 TestElasticHeapSoftmxPercentFullGC
 */

public class TestElasticHeapSoftmxPercentFullGC {
    public static void main(String[] args) throws Exception {
        ProcessBuilder serverBuilder;
        serverBuilder = ProcessTools.createJavaProcessBuilder("-XX:+UseG1GC",
            "-XX:+G1ElasticHeap",
            "-Xmx1000m", "-Xms1000m", "-XX:G1HeapRegionSize=1m", "-XX:+AlwaysPreTouch",
            "-verbose:gc", "-XX:+PrintGCDetails", "-XX:+PrintGCTimeStamps",
            "-Dtest.jdk=" + System.getProperty("test.jdk"),
            Server.class.getName());
        Process server = serverBuilder.start();
        OutputAnalyzer output = new OutputAnalyzer(server);
        System.out.println(output.getOutput());
        // Should not have regular initial-mark
        output.shouldNotContain("[GC pause (G1 Evacuation Pause) (young) (initial-mark)");
        Asserts.assertTrue(output.getExitValue() == 0);
    }

    private static class Server {
        public static void main(String[] args) throws Exception {
            // Promote 300M into old gen
            Object[] root = new Object[3 * 1024];
            for (int i = 0; i < 3 * 1024; i++) {
                root[i] = new byte[100*1024];
            }

            System.gc();
            int fullRss = getRss();
            System.out.println("Full rss: " + fullRss);
            Asserts.assertTrue(fullRss > 1000 * 1024);

            OutputAnalyzer output = triggerJcmd("GC.elastic_heap", "softmx_percent=60", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 60, uncommitted memory 419430400 B]");
            int rss60 = getRss();
            System.out.println("60% rss: " + rss60);
            Asserts.assertTrue(Math.abs(fullRss - rss60) > 350 * 1024);

            System.gc();

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=80" , "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 80, uncommitted memory 209715200 B]");
            int rss80 = getRss();
            System.out.println("80% rss: " + rss80);
            Asserts.assertTrue(Math.abs(fullRss - rss80) > 150 * 1024);
            Asserts.assertTrue(Math.abs(rss80 - rss60) > 150 * 1024);

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=0", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: inactive]");
            int rss100 = getRss();
            System.out.println("100% rss: " + rss100);
            Asserts.assertTrue(Math.abs(fullRss - rss100) < 100 * 1024);

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=70", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            output.shouldContain("[GC.elastic_heap: softmx percent 70, uncommitted memory 314572800 B]");
            int rss70 = getRss();
            System.out.println("70% rss: " + rss70);
            Asserts.assertTrue(Math.abs(fullRss - rss70) > 250 * 1024);

            output = triggerJcmd("GC.elastic_heap", "softmx_percent=30", "-fullgc=true");
            System.out.println(output.getOutput());
            output.shouldContain("[GC.elastic_heap: in softmx mode]");
            int rssNot30 = getRss();
            System.out.println("not 30% rss: " + rssNot30);
            // Should be between 45% and 55%
            Asserts.assertTrue(Math.abs(fullRss - rssNot30) > 450 * 1024);
            Asserts.assertTrue(Math.abs(fullRss - rssNot30) < 550 * 1024);

        }

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

        private static int getRss() throws Exception {
            String pid = Integer.toString(ProcessTools.getProcessId());
            int rss = 0;
            Process ps = Runtime.getRuntime().exec("cat /proc/"+pid+"/status");
            ps.waitFor();
            BufferedReader br = new BufferedReader(new InputStreamReader(ps.getInputStream()));
            String line;
            while (( line = br.readLine()) != null ) {
                if (line.startsWith("VmRSS:") ) {
                    int numEnd = line.length() - 3;
                    int numBegin = line.lastIndexOf(" ", numEnd - 1) + 1;
                    rss = Integer.parseInt(line.substring(numBegin, numEnd));
                    break;
                }
            }
            return rss;
        }
    }
}
