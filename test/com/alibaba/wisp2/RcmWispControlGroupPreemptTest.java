/*
 * @test
 * @library /lib/testlibrary
 * @summary test an inherated wisp could be preempted in a dead loop.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmWispControlGroupPreemptTest
 */

import java.lang.reflect.Field;

import sun.misc.SharedSecrets;

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;
import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispResourceContainerFactory;

import java.util.concurrent.*;
import java.util.Collections;

import static jdk.testlibrary.Asserts.*;

public class RcmWispControlGroupPreemptTest {

    private static final int TIME_OUT = 2500;

    private static FutureTask getFutureTask() {
        return new FutureTask<>(() -> {
            long start = System.currentTimeMillis();
            while (System.currentTimeMillis() - start < TIME_OUT) {
                int count = 500000;
                while (count > 0) {
                    count--;
                }
            }
            Class<?> wispTaskClazz = Class.forName("com.alibaba.wisp.engine.WispTask");
            Field field = wispTaskClazz.getDeclaredField("preemptCount");
            field.setAccessible(true);
            int preemptCount = field.getInt(SharedSecrets.getWispEngineAccess().getCurrentTask());
            assertTrue(preemptCount > 0, "preempt doesn't happen in the wispTask container!!!");
            return null;
        });
    }

    public static void main(String[] args) throws Exception {

        FutureTask future0 = getFutureTask();
        FutureTask future1 = getFutureTask();

        ResourceContainer rc0 = WispResourceContainerFactory.instance().createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80)));
        WispEngine.dispatch(() -> {
            AbstractResourceContainer.root().run(() -> rc0.run(future0));
        });
        future0.get();
        ResourceContainer rc1 = WispResourceContainerFactory.instance().createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80)));
        WispEngine.dispatch(() -> {
            AbstractResourceContainer.root().run(() -> rc1.run(() -> {
                new Thread(future1).start();
            }));
        });
        future1.get();
    }
}