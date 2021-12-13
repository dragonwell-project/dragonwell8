/*
 * @test
 * @library /lib/testlibrary
 * @build RcmCpuTest RcmUtils
 * @summary test RCM cpu resource control.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmCpuTest
 */

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;

import java.security.MessageDigest;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;

import static jdk.testlibrary.Asserts.assertLT;

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
        ResourceContainer rc0 = RcmUtils.createContainer(
                ResourceType.CPU_PERCENT.newConstraint(40));
        ResourceContainer rc1 = RcmUtils.createContainer(
                ResourceType.CPU_PERCENT.newConstraint(80));

        taskFactory(1_000_000).call(); // warm up
        Callable<Long> task0 = taskFactory(2_000_000);
        Callable<Long> task1 = taskFactory(2_000_000);
        FutureTask<Long> futureTask0 = new FutureTask<>(task0);
        FutureTask<Long> futureTask1 = new FutureTask<>(task1);
        ExecutorService es = Executors.newFixedThreadPool(4);
        es.submit(() -> {
            System.out.println("start-0");
            rc0.run(futureTask0);
            System.out.println("done-0");
        });
        es.submit(() -> {
            System.out.println("start-1");
            rc1.run(futureTask1);
            System.out.println("done-1");
        });
        Long duration0 = futureTask0.get();
        Long duration1 = futureTask1.get();
        es.shutdownNow();

        double ratio = (double) duration1 / duration0;
        assertLT(Math.abs(ratio - 0.5), 0.10, "deviation is out of reasonable scope");
    }
}
