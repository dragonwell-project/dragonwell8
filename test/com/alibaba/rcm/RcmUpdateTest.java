/*
 * @test
 * @library /lib/testlibrary
 * @build RcmUpdateTest RcmUtils
 * @summary test RCM updating API.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmUpdateTest
 */

import com.alibaba.rcm.RcmUtils;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.security.MessageDigest;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;

import static jdk.testlibrary.Asserts.assertLT;

public class RcmUpdateTest {

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
        ResourceContainer rc0 = RcmUtils.createContainer(ResourceType.CPU_PERCENT.newConstraint(40));

        Callable<Long> task0 = taskFactory(2_000_000);
        FutureTask<Long> futureTask0 = new FutureTask<Long>(task0);
        ExecutorService es = Executors.newFixedThreadPool(4);
        es.submit(() -> {
            rc0.run(futureTask0);
        });
        Long duration0 = futureTask0.get();

        rc0.updateConstraint(ResourceType.CPU_PERCENT.newConstraint(80));
        Callable<Long> task1 = taskFactory(2_000_000);
        FutureTask<Long> futureTask1 = new FutureTask<Long>(task1);
        es.submit(() -> {
            AbstractResourceContainer.root().run(() -> {
                rc0.run(futureTask1);
            });
        });
        Long duration1 = futureTask1.get();
        es.shutdownNow();

        double ratio = (double) duration1.longValue() / duration0.longValue();
        assertLT(Math.abs(ratio - 0.5), 0.1, "deviation is out of reasonable scope:"
                + duration0.longValue() + "/" + duration1.longValue());
    }
}