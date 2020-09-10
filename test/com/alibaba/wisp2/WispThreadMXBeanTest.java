/*
 * @test
 * @library /lib/testlibrary
 * @summary
 * @requires os.family == "linux"
 * @run main/othervm  -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 WispThreadMXBeanTest
 */
// * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 WispThreadMXBeanTest

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.Arrays;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Phaser;
import java.util.stream.Collectors;
import java.util.stream.LongStream;
import java.util.stream.Stream;

import static jdk.testlibrary.Asserts.assertTrue;


public class WispThreadMXBeanTest {
    private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();
    private static final int THREAD_SIZE = 25;
    private static final Phaser startupCheck = new Phaser(THREAD_SIZE + 1);

    public static void main(String[] args) throws Exception {
        getAllThreadIds();
        dumpAllThreads();
        getThreadInfo();
    }

    // also getThreadCount
    private static void getAllThreadIds() throws Exception {
        long[] startIds = mbean.getAllThreadIds();
        int startCnt = mbean.getThreadCount();
        assertTrue(startIds.length == startCnt);

        long[] newIds = new long[THREAD_SIZE];
        for (int i = 0; i < THREAD_SIZE; i++) {
            Thread t = new Thread(() -> {
                startupCheck.arrive();
                while (true) {
                    sleep();
                }
            });
            newIds[i] = t.getId();
            t.start();
        }
        startupCheck.arriveAndAwaitAdvance();

        long[] endIds = mbean.getAllThreadIds();
        long endCnt = mbean.getThreadCount();

        assertTrue(endIds.length == endCnt);
        assertTrue(endCnt - startCnt == THREAD_SIZE);
        for (int i = 0; i < THREAD_SIZE; i++) {
            final int idx = i;
            assertTrue(Arrays.stream(endIds).anyMatch(e -> e == newIds[idx]));
        }
    }

    private static void dumpAllThreads() {
        long[] ids = mbean.getAllThreadIds();
        long[] newThreadIds = new long[THREAD_SIZE];

        for (int i = 0; i < THREAD_SIZE; i++) {
            Thread t = new Thread(() -> {
                startupCheck.arrive();
                while (true) {
                    sleep();
                }
            });
            t.setName("dumpAll");
            newThreadIds[i] = t.getId();
            t.start();
        }
        startupCheck.arriveAndAwaitAdvance();

        boolean[] lockedMonitor = new boolean[] {false, true};
        boolean[] lockedSyncronizer = new boolean[] {false, true};
        for (boolean lm : lockedMonitor) {
            for (boolean ls : lockedSyncronizer) {
                ThreadInfo[] threadInfosNew = mbean.dumpAllThreads(lm, ls);
                assertTrue(threadInfosNew.length - ids.length >= THREAD_SIZE, " " + threadInfosNew.length + "," + ids.length);
                assertTrue(Arrays.stream(threadInfosNew).allMatch(info -> info.getStackTrace() != null));
                assertTrue(Arrays.stream(threadInfosNew).anyMatch(info -> info.getThreadName().equals("dumpAll")));
                assertTrue(Arrays.stream(threadInfosNew).noneMatch(Objects::isNull));
                for (int i = 0; i < newThreadIds.length; i++) {
                    final int idx = i;
                    assertTrue(Arrays.stream(threadInfosNew).anyMatch(info -> info.getThreadId() == newThreadIds[idx] && Objects.equals(info.getStackTrace()[0].getFileName(), "CoroutineSupport.java")));
                }
            }
        }
    }

    private static void getThreadInfo() throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        Thread thread = new Thread(() -> {
            latch.countDown();
            while (true) { sleep(); }
        });
        thread.start();

        latch.await();
        ThreadInfo[] infos = mbean.getThreadInfo(new long[]{thread.getId()}, Integer.MAX_VALUE);
        StackTraceElement[] stack1 =  thread.getStackTrace();
        StackTraceElement[] stack2 =  infos[0].getStackTrace();
        assertTrue(Arrays.equals(stack1, stack2));
    }



    private static void sleep() {
        try {
            Thread.sleep(20);
        } catch (InterruptedException e) {}
    }
}
