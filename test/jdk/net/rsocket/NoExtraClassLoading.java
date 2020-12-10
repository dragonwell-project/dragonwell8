/*
 * @test
 * @summary Test to check if no extra rdma class loaded when rdma enabled
 * @requires (os.family == "linux")
 * @library /lib/testlibrary
 * @run main/othervm NoExtraClassLoading
 */
public class NoExtraClassLoading {
    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            ProcessBuilder pb = jdk.testlibrary.ProcessTools.createJavaProcessBuilder(
                    "-XX:+TraceClassLoading",
                    NoExtraClassLoading.class.getName(), "1");
            jdk.testlibrary.OutputAnalyzer output = new jdk.testlibrary.OutputAnalyzer(pb.start());
            output.shouldNotContain("Rdma");
            output.shouldNotContain("rdma");
            return;
        }
    }
}
