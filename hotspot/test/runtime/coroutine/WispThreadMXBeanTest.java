/*
 * @test
 * @library /testlibrary
 * @summary
 * @requires os.family == "linux"
 * @run main/othervm -XX:ActiveProcessorCount=8  -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 WispThreadMXBeanTest
 */

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


import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;

public class WispThreadMXBeanTest {
    private static final ThreadMXBean mbean = ManagementFactory.getThreadMXBean();
    private static final int THREAD_SIZE = 25;
    private static final Phaser startupCheck = new Phaser(THREAD_SIZE + 1);
    private static volatile boolean done = false;


    public static void main(String[] args) throws Exception {
        getStack();
    }

    private static void getStack() throws Exception {
        Thread t = new Thread(() -> {
            while (!done) {
                Thread.yield();
            }
        });

        t.start();

        int i = 0;
        Thread.sleep(200);
        for (int it = 0; it < 2000; it++) {
            i+=  t.getStackTrace().length;
        }
        System.out.println(i);

       done = true;
    }
}
