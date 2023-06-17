import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.CountDownLatch;
import java.math.*;
import java.util.*;

public class TestClassLoaderInParallel {
    private static final Path CLASSES_DIR = Paths.get(System.getProperty("test.classes"));
    static CountDownLatch finishLatch;

    public static void main(String... args) throws Exception {
        finishLatch = new CountDownLatch(2);
        //  class loader with name
        testURLClassLoader("myloader");

        //Start a class that has a method running
        new Thread() {
            public void run() {
                try {
                    testURLClassLoader("myloader2");
                } catch (Exception e) {
                }

                for (long i = -10; i < 0; i++) {
                    for (int j = -5; j < 5; j++) {
                        try {
                            BigDecimal.valueOf(i, j);
                        } catch (ArithmeticException e) {
                            ; // Expected
                        }
                    }
                }

                finishLatch.countDown();
            }
        }.start();

        new Thread() {
            public void run() {
                try {
                    testURLClassLoader("myloader3");
                } catch (Exception e) {
                }
                try {
                    // the code for japanese zero
                    BigInteger b1 = new BigInteger("\uff10");
                    System.err.println(b1.toString());

                    // Japanese 1010
                    BigInteger b2 = new BigInteger("\uff11\uff10\uff11\uff10");
                    System.err.println(b2.toString());
                }
                catch (ArrayIndexOutOfBoundsException e) {
                    throw new RuntimeException(
                            "BigInteger is not accepting unicode initializers.");
                }
                finishLatch.countDown();
            }
        }.start();

        try {
            finishLatch.await();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static void testURLClassLoader(String loaderName) throws Exception {
        URL[] urls = new URL[] { CLASSES_DIR.toUri().toURL() };
        ClassLoader parent = ClassLoader.getSystemClassLoader();
        URLClassLoader loader = new URLClassLoader(urls, parent);

        try {
            Method m = Class.forName("com.alibaba.util.Utils").getDeclaredMethod("registerClassLoader",
                                     ClassLoader.class, String.class);
            m.setAccessible(true);
            m.invoke(null, loader, loaderName);
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }

        Class<?> c = Class.forName("TestSimple", true, loader);
    }

}

