/*
 * @test
 * @library /testlibrary
 * @summary Test jstack control group id
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=5 JStackWispContainerIdTest
 */

import com.alibaba.rcm.internal.AbstractResourceContainer;
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

import java.io.FileWriter;

public class JStackWispContainerIdTest {
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();

    private static Field ctx = null;
    private static Field data = null;
    private static Field abstractResourceContainer = null;
    private static Field id = null;
    private static Class<?> WispControlGroupClazz = null;
    private static Class<?> AbstractResourceContainerClazz = null;
    private static Class<?> ThreadClazz = null;

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
            WispControlGroupClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup");
            AbstractResourceContainerClazz = Class.forName("com.alibaba.rcm.internal.AbstractResourceContainer");
            ThreadClazz = Class.forName("java.lang.Thread");
            abstractResourceContainer = ThreadClazz.getDeclaredField("resourceContainer");
            abstractResourceContainer.setAccessible(true);
            id = AbstractResourceContainerClazz.getDeclaredField("id");
            id.setAccessible(true);
        } catch (NoSuchFieldException | ClassNotFoundException e) {
            fail();
        }
    }

    private static Map<String, Long> idMap = new ConcurrentHashMap<>();

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
                    idMap.put(name, (Long) id.get(abstractResourceContainer.get(Thread.currentThread())));
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
                long idV = Long.parseLong(matchId(str));
                Long idVM = idMap.get(coroutine);
                if (idVM != null) {
                    assertEQ(idVM.longValue(), idV);
                    checkedWisp++;
                }
            }
        }
        latch1.countDown();
        assertEQ(checkedWisp, wispTaskCount);
    }

    private static String matchId(String str) {
        Pattern pattern = Pattern.compile(".*containerId=(\\d+).*");
        Matcher matcher = pattern.matcher(str);
        if (matcher.find()) {
            return matcher.group(1);
        }
        fail();
        return null;
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
