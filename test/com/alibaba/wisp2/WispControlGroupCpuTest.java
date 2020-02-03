/*
 * @test
 * @library /lib/testlibrary
 * @summary test tenant CPU
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.carrierEngines=4 WispControlGroupCpuTest
 */

import java.lang.Long;
import java.lang.reflect.Method;

import java.security.MessageDigest;

import java.util.concurrent.Callable;
import java.util.concurrent.Future;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.CountDownLatch;

import static jdk.testlibrary.Asserts.*;

public class WispControlGroupCpuTest {

    private static String md5Encode(String inStr) throws Exception {
        MessageDigest md5 = null;
        try {
            md5 = MessageDigest.getInstance("MD5");
        } catch (Exception e) {
            System.out.println(e.toString());
            e.printStackTrace();
            return "";
        }

        byte[] byteArray = inStr.getBytes("UTF-8");
        byte[] md5Bytes = md5.digest(byteArray);
        StringBuffer hexValue = new StringBuffer();
        for (int i = 0; i < md5Bytes.length; i++) {
            int val = ((int) md5Bytes[i]) & 0xff;
            if (val < 16) {
                hexValue.append("0");
            }
            hexValue.append(Integer.toHexString(val));
        }
        return hexValue.toString();
    }

    private static Callable<Long> taskFactory(int load) {
        return new Callable<Long>() {
            @Override
            public Long call() throws Exception {
                long start = System.currentTimeMillis();
                int count = load;
                while (--count > 0) {
                    md5Encode("Hello, World");
                    if (count % 30 == 0) {
                        Thread.yield();
                    }
                }
                return System.currentTimeMillis() - start;
            }
        };
    }

    public static void main(String[] args) {
        try {
            Class<?> WispControlGroupClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup");

            Method createMethod1 = WispControlGroupClazz.getDeclaredMethod("create", int.class);
            Method createMethod0 = WispControlGroupClazz.getDeclaredMethod("create", int.class, int.class);

            createMethod0.setAccessible(true);
            createMethod1.setAccessible(true);

            Callable<Long> task0 = taskFactory(2_000_000);
            Callable<Long> task1 = taskFactory(2_000_000);
            Callable<Long> task2 = taskFactory(2_000_000);
            Callable<Long> task3 = taskFactory(2_000_000);

            ExecutorService cg0 = (ExecutorService) createMethod0.invoke(null, 2_000, 10_000);
            ExecutorService cg1 = (ExecutorService) createMethod0.invoke(null, 4_000, 10_000);
            ExecutorService cg2 = (ExecutorService) createMethod1.invoke(null, 40);
            ExecutorService cg3 = (ExecutorService) createMethod1.invoke(null, 80);

            Future<Long> future0 = cg0.submit(task0);
            Future<Long> future1 = cg1.submit(task1);
            Future<Long> future2 = cg2.submit(task2);
            Future<Long> future3 = cg3.submit(task3);

            Long duration0 = future0.get();
            Long duration1 = future1.get();
            Long duration2 = future2.get();
            Long duration3 = future3.get();
            double ratio = (double) duration1.longValue() / duration0.longValue();
            if (!(ratio > 0.40 && ratio < 0.60)) {
                System.out.println(duration0);
                System.out.println(duration1);
                assertTrue(false);
            }
            ratio = (double) duration3.longValue() / duration2.longValue();
            if (!(ratio > 0.40 && ratio < 0.60)) {
                System.out.println(duration2);
                System.out.println(duration3);
                assertTrue(false);
            }
        } catch (Exception e) {
            e.printStackTrace();
            assertTrue(false, e.toString());
        }
    }
}
