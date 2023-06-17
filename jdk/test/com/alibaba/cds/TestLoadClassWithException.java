import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.concurrent.CountDownLatch;
import java.math.*;
import java.util.*;

public class TestLoadClassWithException {
    static CountDownLatch finishLatch;
    static URLClassLoader loader;

    public static void main(String... args) throws Exception {
        Path CLASSES_DIR = Paths.get("./testSimple.jar");
        String classPath2 = "newpath/testSimple.jar";
        Path CLASSES_DIR_2 = Paths.get(classPath2);
        finishLatch = new CountDownLatch(2);
        //  class loader with name
        URL[] urls = new URL[] { CLASSES_DIR.toUri().toURL(), CLASSES_DIR_2.toUri().toURL() };
        ClassLoader parent = ClassLoader.getSystemClassLoader();
        loader = new URLClassLoader(urls, parent);
        registerURLClassLoader(loader, "myloader");

        new Thread() {
            public void run() {
                try {
                    Class<?> c = Class.forName("trivial.TestSimple", true, loader);
                } catch (Exception e) {
                    e.printStackTrace(System.out);
                }
                finishLatch.countDown();
            }
        }.start();

        new Thread() {
            public void run() {
                try {
                    Class<?> c = Class.forName("TestSimpleWispUsage", true, loader);
                } catch (Exception e) {
                    e.printStackTrace(System.out);
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

    public static void registerURLClassLoader(URLClassLoader loader, String loaderName) throws Exception {
        try {
            Method m = Class.forName("com.alibaba.util.Utils").getDeclaredMethod("registerClassLoader",
                                     ClassLoader.class, String.class);
            m.setAccessible(true);
            m.invoke(null, loader, loaderName);
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }

}

