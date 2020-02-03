/*
 * @test
 * @library /lib/testlibrary
 * @summary test RCM cpu resource control.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmCpuTest
 */

import java.lang.Long;
import java.lang.reflect.Field;

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;

import java.security.MessageDigest;

import java.util.Collections;
import java.util.concurrent.FutureTask;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import static jdk.testlibrary.Asserts.*;

public class RcmCpuTest {

    private static Callable<Long> taskFactory(int load) {
        return new Callable<Long>() {
            @Override
            public Long call() throws Exception {
                long start = System.currentTimeMillis();
                MessageDigest md5 = MessageDigest.getInstance("MD5");
                int count = load;
                while (--count > 0) {
                    md5.update("hello world!!!".getBytes());
                    if (count % 20 == 0) {
                        Thread.yield();
                    }
                }
                return System.currentTimeMillis() - start;
            }
        };
    }

    public static void main(String[] args) throws Exception {
        Field f = Class.forName("com.alibaba.wisp.engine.WispConfiguration").getDeclaredField("ENABLE_THREAD_AS_WISP");
        f.setAccessible(true);
        boolean isWispEnabled = f.getBoolean(null);
        ResourceContainer rc0 = isWispEnabled ? WispResourceContainerFactory.instance()
        .createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(40))) : null;
        ResourceContainer rc1 = isWispEnabled ? WispResourceContainerFactory.instance()
        .createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80))) : null;

        Callable<Long> task0 = taskFactory(2_000_000);
        Callable<Long> task1 = taskFactory(2_000_000);
        FutureTask<Long> futureTask0 = new FutureTask<Long>(task0);
        FutureTask<Long> futureTask1 = new FutureTask<Long>(task1);
        ExecutorService es = Executors.newFixedThreadPool(4);
        es.submit(() -> {
            rc0.run(futureTask0);
        });
        es.submit(() -> {
            rc1.run(futureTask1);
        });
        Long duration0 = futureTask0.get();
        Long duration1 = futureTask1.get();
        double ratio = (double) duration1.longValue() / duration0.longValue();
        assertLT(Math.abs(ratio - 0.5), 0.1, "deviation is out of reasonable scope");
    }
}