/*
 * @test
 * @library /lib/testlibrary
 * @build TestEpollArrayClear
 * @summary test RCM TestEpollArrayClearcontrol
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 TestEpollArrayClear
 */

import com.alibaba.wisp.engine.WispTask;
import sun.misc.SharedSecrets;
import sun.misc.WispEngineAccess;
import sun.nio.ch.EPollSelectorProvider;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.channels.Selector;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import static jdk.testlibrary.Asserts.*;

public class TestEpollArrayClear {

    private static volatile int started = 0;
    private static WispEngineAccess WEA = SharedSecrets.getWispEngineAccess();
    private static AtomicReference<WispTask> atomicReference = new AtomicReference<>();


    private static Runnable IO = () -> {
        while (true) {
            started = 1;
            try {
                Selector selector = EPollSelectorProvider.provider().openSelector();
                atomicReference.set(WEA.getCurrentTask());

                Field f = Thread.class.getDeclaredField("inheritedResourceContainer");
                f.setAccessible(true);
                f.set(Thread.currentThread(), null);
                while (true) {
                    selector.select(10000L);
                }
            } catch (Exception exception) {
                exception.printStackTrace();
                continue;
            }
        }
    };

    public static void main(String[] args) throws Exception {
        started = 0;
        Class<?> wispControlGroupClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup");
        Method createMethod = wispControlGroupClazz.getDeclaredMethod("newInstance", int.class);
        createMethod.setAccessible(true);
        ExecutorService cg = (ExecutorService) createMethod.invoke(null, 50);

        cg.execute(IO);
        while (0 == started) {
        }
        Thread.sleep(100);
        cg.shutdown();
        while (!cg.isTerminated()) {
            try {
                cg.awaitTermination(1, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                throw new InternalError(e);
            }
        }

        Field f = WispTask.class.getDeclaredField("epollArray");
        f.setAccessible(true);
        assertTrue(0L ==  (Long) f.get(atomicReference.get()));
    }
}
