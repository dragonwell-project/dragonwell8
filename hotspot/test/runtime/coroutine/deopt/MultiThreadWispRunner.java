import com.alibaba.wisp.engine.WispEngine;
import sun.misc.SharedSecrets;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.stream.IntStream;

public class MultiThreadWispRunner {

    public static void run(String name, int threadCount, int coroCount, int runCount, int outputCount, Runnable runnable) throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(threadCount * coroCount);
        AtomicInteger id = new AtomicInteger();
        ExecutorService es = Executors.newCachedThreadPool(r -> {
            Thread t = new Thread(r);
            t.setName(name + "_" + id.getAndIncrement());
            return t;
        });
        IntStream.range(0, threadCount).forEach(ign -> es.execute(() -> {
            AtomicInteger n = new AtomicInteger();
            for (int i = 0; i < coroCount; i++) {
                WispEngine.dispatch(() -> {
                    while (n.get() < runCount) {
                        runnable.run();
                        if (n.incrementAndGet() % outputCount == 0) {
                            System.out.println(SharedSecrets.getJavaLangAccess().currentThread0().getName() + "/" + n.get() / 1000 +"k");
                        }
                    }
                    latch.countDown();
                });
            }
        }));
        latch.await();
    }

}
