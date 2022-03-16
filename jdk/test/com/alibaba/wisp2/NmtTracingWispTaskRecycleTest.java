/*
 * @test
 * @library /lib/testlibrary
 * @summary verification of uncached WispTask resource recycled by JVM
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 -XX:NativeMemoryTracking=summary NmtTracingWispTaskRecycleTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.locks.ReentrantLock;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import static jdk.testlibrary.Asserts.assertTrue;

public class NmtTracingWispTaskRecycleTest {

    public static void main(final String[] args) throws Exception {

        int loop = 0;
        final int count = 300;
        long initCoroutineStack = 0;
        long endCoroutineStack = 0;

        jcmd();

        // step one: create enough wisp task to fill carrier local cache and global
        // cache.
        // step two: create more 200 wisp task.
        for (; loop < 2; loop++) {
            final CountDownLatch latch0 = new CountDownLatch(count);
            final CountDownLatch latch1 = new CountDownLatch(1);
            for (int i = 0; i < count; i++) {
                WispEngine.dispatch(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            latch0.countDown();
                            latch1.await();
                        } catch (final Exception e) {
                            e.printStackTrace();
                        }
                    }
                });
            }
            latch0.await();
            latch1.countDown();
            // make sure that all 200 wisp task exit.
            Thread.sleep(1000);
            // record wisp task stack size at this point.
            if (loop == 0) {
                initCoroutineStack = parseResult(jcmd());
            } else if (loop == 1) {
                endCoroutineStack = parseResult(jcmd());
            }
        }

        System.out.println(initCoroutineStack);
        System.out.println(endCoroutineStack);

        // step three: compare Coroutine stack size, it should never grow and all be
        // released by jvm.
        assertTrue(initCoroutineStack == endCoroutineStack,
                "uncached wisp task's memory was not released as expected.");
    }

    private static List<String> jcmd() throws Exception {
        final List<String> statusLines = Files.readAllLines(Paths.get("/proc/self/status"));
        final String pidLine = statusLines.stream().filter(l -> l.startsWith("Pid:")).findAny().orElse("1 -1");
        final int pid = Integer.valueOf(pidLine.split("\\s+")[1]);

        final Process p = Runtime.getRuntime()
                .exec(System.getProperty("java.home") + "/../bin/jcmd " + pid + " VM.native_memory");
        final List<String> result = new BufferedReader(new InputStreamReader(p.getInputStream())).lines()
                .collect(Collectors.toList());
        return result;
    }

    private static long parseResult(final List<String> result) {
        int i = 0;
        for (; i < result.size(); i++) {
            if (result.get(i).contains("CoroutineStack")) {
                return matchCoroutineStack(result.get(i));
            }
        }
        throw new RuntimeException("ShouldNotReachHere");
    }

    static final Pattern pattern = Pattern.compile(".*CoroutineStack \\(reserved=(\\d+).*");
    private static long matchCoroutineStack(final String str) {
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            System.out.println(str);
            return Long.parseLong(matcher.group(1));
        }
        throw new RuntimeException("ShouldNotReachHere");
    }
}