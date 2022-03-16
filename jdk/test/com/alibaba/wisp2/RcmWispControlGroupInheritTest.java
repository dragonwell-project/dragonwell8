/*
 * @test
 * @library /lib/testlibrary
 * @summary test wisp control group inheritance feature.
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmWispControlGroupInheritTest
 */

import java.lang.Long;
import java.lang.reflect.Field;

import sun.misc.SharedSecrets;

import java.util.Collections;
import java.util.concurrent.*;

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;
import com.alibaba.rcm.internal.AbstractResourceContainer;
import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispResourceContainerFactory;

import static jdk.testlibrary.Asserts.*;

public class RcmWispControlGroupInheritTest {

    private static Callable<Long> taskFactory(int load, ResourceContainer rc) {
        return new Callable<Long>() {
            @Override
            public Long call() throws Exception {
                int count = load;
                while (--count > 0) {
                    ;
                }
                Class<?> wispTaskClazz = Class.forName("com.alibaba.wisp.engine.WispTask");
                Field field = wispTaskClazz.getDeclaredField("controlGroup");
                field.setAccessible(true);
                Object container = field.get((SharedSecrets.getWispEngineAccess().getCurrentTask()));

                boolean findOuterObj = false;
                Field[] fields = rc.getClass().getDeclaredFields();
                for (Field fieldx : fields) {
                    if (fieldx != null && fieldx.getType() == container.getClass()
                            && fieldx.getName().startsWith("this$")) {
                        fieldx.setAccessible(true);
                        Object outerObj = fieldx.get(rc);
                        assertTrue(container == outerObj, "should own the same container as its parent wisp!!!");
                        findOuterObj = true;
                        break;
                    }
                }
                assertTrue(findOuterObj, "outer WispControlGroup was not found in giving container");

                Class<?> threadClazz = Class.forName("java.lang.Thread");
                Field fieldCon = threadClazz.getDeclaredField("resourceContainer");
                fieldCon.setAccessible(true);
                Object threadContainer = fieldCon.get(Thread.currentThread());
                assertTrue(threadContainer == (Object) rc, "should own the same container as its parent thread!!!");

                long i = 0;
                return Long.valueOf(i);
            }
        };
    }

    public static void main(String[] args) throws Exception {
        ResourceContainer rc0 = WispResourceContainerFactory.instance()
                .createContainer((Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(40))));
        Callable<Long> task0 = taskFactory(10, rc0);
        FutureTask<Long> futureTask0 = new FutureTask<Long>(task0);
        Runnable command0 = () -> {
            new Thread(futureTask0).start();
        };
        WispEngine.dispatch(() -> {
            AbstractResourceContainer.root().run(() -> rc0.run(command0));
        });
        futureTask0.get();
    }
}