/*
 * @test
 * @library /testlibrary
 * @summary Test jstack ttr and container's cfs configuration.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=5 JStackWispControlGroupTest
 */

import com.alibaba.wisp.engine.WispTask;

import java.dyn.CoroutineBase;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.concurrent.ExecutorService;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;
import static com.oracle.java.testlibrary.Asserts.*;

public class JStackWispControlGroupTest {
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    private static Field ctx = null;
    private static Field data = null;
    private static Field ttr = null;
    private static Field cfsPeriod = null;
    private static Field cfsQuota = null;
    private static Field cg = null;
    private static Field cpuLimit = null;
    private static Class<?> WispControlGroupClazz = null;
    private static Class<?> CpuLimitClazz = null;

    static {
        try {
            ctx = WispTask.class.getDeclaredField("ctx");
            ctx.setAccessible(true);
        } catch (NoSuchFieldException e) {
            fail();
        }
        try {
            data = CoroutineBase.class.getDeclaredField("nativeCoroutine");
            data.setAccessible(true);
        } catch (NoSuchFieldException e) {
            fail();
        }
        try {
            ttr = WispTask.class.getDeclaredField("ttr");
            ttr.setAccessible(true);
        } catch (NoSuchFieldException e) {
            fail();
        }
        try {
            WispControlGroupClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup");
            CpuLimitClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup$CpuLimit");
            cg = WispTask.class.getDeclaredField("controlGroup");
            cg.setAccessible(true);
            cpuLimit = WispControlGroupClazz.getDeclaredField("cpuLimit");
            cpuLimit.setAccessible(true);
            cfsPeriod = CpuLimitClazz.getDeclaredField("cfsPeriod");
            cfsPeriod.setAccessible(true);
            cfsQuota = CpuLimitClazz.getDeclaredField("cfsQuota");
            cfsQuota.setAccessible(true);
        } catch (NoSuchFieldException | ClassNotFoundException e) {
            fail();
        }
    }

    private static Map<String, Long> ttrMap = new ConcurrentHashMap<>();
    private static Map<String, Long> cfsPeriodMap = new ConcurrentHashMap<>();
    private static Map<String, Long> cfsQuotaMap = new ConcurrentHashMap<>();

    private static void test() throws Exception {

        Method createMethod = WispControlGroupClazz.getDeclaredMethod("newInstance",
                int.class);
        createMethod.setAccessible(true);
        ExecutorService contain = (ExecutorService) createMethod.invoke(null, 50);

        int wispTaskCount = 200;

        CountDownLatch latch0 = new CountDownLatch(wispTaskCount);
        CountDownLatch latch1 = new CountDownLatch(1);
        for (int i = 0; i < wispTaskCount; i++) {
            contain.submit(() -> {
                try {
                    WispTask current = WEA.getCurrentTask();
                    long coroutine = (Long) data.get(ctx.get(current));
                    String name = "0x" + Long.toHexString(coroutine);
                    ttrMap.put(name, (Long) ttr.get(current));
                    cfsPeriodMap.put(name, (Long) cfsPeriod.get(cpuLimit.get(cg.get(current))));
                    cfsQuotaMap.put(name, (Long) cfsQuota.get(cpuLimit.get(cg.get(current))));
                } catch (IllegalAccessException e) {
                    fail(e.getMessage());
                } finally {
                    try {
                        latch0.countDown();
                        latch1.await();
                    } catch (InterruptedException e) {
                        fail();
                    }
                }
            });
        }

        latch0.await();
        List<String> result = jstack();
        int checkedWisp = 0;
        for (int i = 0; i < result.size(); i++) {
            if (result.get(i).contains("- Coroutine [")) {
                String str = result.get(i);
                String coroutine = matchCoroutine(str);
                long ttrV = Long.parseLong(matchttr(str));
                Long ttrVM = ttrMap.get(coroutine);
                long cfsPeriodV = Long.parseLong(matchPeriod(str));
                long cfsQuotaV = Long.parseLong(matchQuota(str));
                Long cfsVM = ttrMap.get(coroutine);
                if (ttrVM != null && cfsVM != null) {
                    assertEQ(ttrVM.longValue(), ttrV);
                    assertEQ(cfsPeriodMap.get(coroutine).longValue(), cfsPeriodV * 1000);
                    assertEQ(cfsQuotaMap.get(coroutine).longValue(), cfsQuotaV * 1000);
                    checkedWisp++;
                }
            }
        }
        latch1.countDown();
        assertEQ(checkedWisp, wispTaskCount);
    }

    private static String matchCoroutine(String str) {
        Pattern pattern = Pattern.compile(".*\\[(.*)\\].*");
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            return matcher.group(1);
        }
        fail();
        return null;
    }

    private static String matchttr(String str) {
        Pattern pattern = Pattern.compile(".*ttr=(\\d+).*");
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            return matcher.group(1);
        }
        fail();
        return null;
    }

    private static String matchQuota(String str) {
        Pattern pattern = Pattern.compile(".*cg=(\\d+).*");
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            return matcher.group(1);
        }
        fail();
        return null;
    }

    private static String matchPeriod(String str) {
        Pattern pattern = Pattern.compile(".*cg=[0-9]*/(\\d+).*");
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            return matcher.group(1);
        }
        fail();
        return null;
    }

    private static List<String> jstack() throws Exception {
        List<String> statusLines = Files.readAllLines(Paths.get("/proc/self/status"));
        String pidLine = statusLines.stream().filter(l -> l.startsWith("Pid:")).findAny().orElse("1 -1");
        int pid = Integer.valueOf(pidLine.split("\\s+")[1]);

        Process p = Runtime.getRuntime().exec(System.getProperty("java.home") + "/../bin/jstack " + pid);
        List<String> result = new BufferedReader(new InputStreamReader(p.getInputStream())).lines()
                .collect(Collectors.toList());
        return result;
    }

    public static void main(String[] args) throws Exception {
        test();
    }
}
