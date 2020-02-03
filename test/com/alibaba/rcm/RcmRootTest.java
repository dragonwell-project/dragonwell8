/*
 * @test
 * @library /lib/testlibrary
 * @summary test RCM root API.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.carrierEngines=4 RcmRootTest
 */

import sun.misc.SharedSecrets;
import java.lang.reflect.Field;

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;

import java.util.Collections;
import java.util.concurrent.FutureTask;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import static jdk.testlibrary.Asserts.*;

public class RcmRootTest {

    public static void main(String[] args) throws Exception {
        Field f = Class.forName("com.alibaba.wisp.engine.WispConfiguration").getDeclaredField("ENABLE_THREAD_AS_WISP");
        f.setAccessible(true);
        boolean isWispEnabled = f.getBoolean(null);
        ResourceContainer rc0 = isWispEnabled ? WispResourceContainerFactory.instance().createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80))) : null;
        ResourceContainer rc1 = isWispEnabled ? WispResourceContainerFactory.instance().createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80))) : null;

        FutureTask future0 = new FutureTask<>(() -> {
            boolean isException = false;
            try {
                ResourceContainer threadContainer = SharedSecrets.getJavaLangAccess()
                        .getResourceContainer(Thread.currentThread());
                assertTrue(threadContainer == rc0, "should own the same container as its parent thread!!!");
                rc1.run(() -> {

                });
            } catch (IllegalStateException e) {
                isException = true;
            } finally {
                assertTrue(isException, "an expected IllegalStateException should happen");
            }
            return null;
        });

        FutureTask future1 = new FutureTask<>(() -> {
            boolean isException = false;
            try {
                ResourceContainer threadContainer = SharedSecrets.getJavaLangAccess()
                        .getResourceContainer(Thread.currentThread());
                assertTrue(threadContainer == rc0, "should own the same container as its parent thread!!!");
                AbstractResourceContainer.root().run(() -> {
                    rc1.run(() -> {
                        try {
                            ResourceContainer threadContainer1 = SharedSecrets.getJavaLangAccess()
                                    .getResourceContainer(Thread.currentThread());
                            assertTrue(threadContainer1 == rc1,
                                    "should own the same container as its parent thread!!!");
                        } catch (Exception e) {
                            assertTrue(false, "unexpected reflection exception happen");
                        }
                    });
                });
            } catch (IllegalStateException e) {
                isException = true;
            } finally {
                assertFalse(isException, "an IllegalStateException should never happen");
            }
            return null;
        });
        ExecutorService es = Executors.newFixedThreadPool(4);
        es.submit(() -> {
            AbstractResourceContainer.root().run(() -> rc0.run(future0));
        });
        es.submit(() -> {
            AbstractResourceContainer.root().run(() -> rc0.run(future1));
        });
        future0.get();
        future1.get();
    }
}