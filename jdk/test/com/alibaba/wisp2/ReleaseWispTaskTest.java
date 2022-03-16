/*
 * @test
 * @library /lib/testlibrary
 * @summary verification of uncached WispTask resource recycled by GC
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=2 ReleaseWispTaskTest
 */

import com.alibaba.wisp.engine.WispEngine;
import com.alibaba.wisp.engine.WispTask;

import java.lang.reflect.Field;
import java.util.Queue;
import java.util.concurrent.CountDownLatch;

import static jdk.testlibrary.Asserts.assertTrue;

public class ReleaseWispTaskTest {
    public static void main(String[] args) throws Exception {
        CountDownLatch latch0 = new CountDownLatch(5000);
        CountDownLatch latch1 = new CountDownLatch(1);

        for (int i = 0; i < 5000; i++) {
            WispEngine.dispatch(new Runnable() {
                @Override
                public void run() {
                    try {
                        latch0.countDown();
                        latch1.await();
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            });
        }
        Field f =  WispEngine.class.getDeclaredField("groupTaskCache");
        f.setAccessible(true);
        Queue<WispTask> queue =  (Queue<WispTask>)f.get(WispEngine.current());
        assertTrue(queue.size() == 0);
        latch0.await();
        latch1.countDown();
        Thread.sleep(1000);
        assertTrue(queue.size() == 21);
    }
}