/*
 * @test
 * @library /testlibrary
 * @summary verification that NMT tracing Wisp Stack Memory statistics
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 -XX:NativeMemoryTracking=summary NmtTracingWispStackTest
 * @run main/othervm -XX:ActiveProcessorCount=4 -XX:NativeMemoryTracking=summary NmtTracingWispStackTest
 */

import java.util.Queue;
import java.util.List;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.locks.ReentrantLock;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.lang.management.ManagementFactory;

import static com.oracle.java.testlibrary.Asserts.*;

public class NmtTracingWispStackTest {
    public static void main(String[] args) throws Exception {
        List<String> result = jcmd();
        int i = 0;
        int flag = 0;
        for (; i < result.size(); i++) {
            if (result.get(i).contains("CoroutineStack") || result.get(i).contains("Coroutine")) {
                flag++;
            }
        }

        List<String> inputArguments = ManagementFactory.getRuntimeMXBean().getInputArguments();
        if (inputArguments.contains("-XX:+UseWisp2")) {
            assertEquals(flag, 2, "NMT trace output contains no CoroutineStack or Coroutine information");
        } else {
            assertEquals(flag, 0, "NMT trace output contains CoroutineStack or Coroutine information");
        }
    }

    private static List<String> jcmd() throws Exception {
        List<String> statusLines = Files.readAllLines(Paths.get("/proc/self/status"));
        String pidLine = statusLines.stream().filter(l -> l.startsWith("Pid:")).findAny().orElse("1 -1");
        int pid = Integer.valueOf(pidLine.split("\\s+")[1]);

        Process p = Runtime.getRuntime()
                .exec(System.getProperty("java.home") + "/../bin/jcmd " + pid + " VM.native_memory");
        List<String> result = new BufferedReader(new InputStreamReader(p.getInputStream())).lines()
                .collect(Collectors.toList());
        return result;
    }
}
