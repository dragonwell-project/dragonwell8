/*
 * @test
 * @library /lib/testlibrary
 * @build RcmUpdateTest RcmUtils
 * @summary test RCM updating API.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmUpdateTest
 */

import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.security.MessageDigest;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;
import java.util.function.Consumer;

import static jdk.testlibrary.Asserts.*;
import static jdk.testlibrary.Asserts.fail;

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

        rc0.getConstraints().forEach(constraint -> {
            if (constraint.getResourceType() == ResourceType.CPU_PERCENT) {
                assertTrue(constraint.getValues()[0] == 40);
                return;
            }
            fail("unexpected resource type");
        });

        rc0.updateConstraint(ResourceType.CPU_PERCENT.newConstraint(80));
        rc0.getConstraints().forEach(constraint -> {
            if (constraint.getResourceType() == ResourceType.CPU_PERCENT) {
                assertTrue(constraint.getValues()[0] == 80);
                return;
            }
            fail("unexpected resource type");
        });
    }
}
