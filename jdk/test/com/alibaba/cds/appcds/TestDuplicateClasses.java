import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.nio.file.Paths;

public class TestDuplicateClasses {
    private static final Path CLASSES_DIR = Paths.get(System.getProperty("test.classes"));

    private static void registerClassLoader(ClassLoader loader, String loaderName) throws Exception {
        try {
            Method m = Class.forName("com.alibaba.util.Utils").getDeclaredMethod("registerClassLoader",
                    ClassLoader.class, String.class);
            m.setAccessible(true);
            m.invoke(null, loader, loaderName);
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    public static void main(String... args) throws Exception {
        //  class loader with name
        testURLClassLoader();
    }

    public static void testURLClassLoader() throws Exception {
        URL[] urls = new URL[] { CLASSES_DIR.toUri().toURL() };
        ClassLoader parent = ClassLoader.getSystemClassLoader();
        URLClassLoader a = new URLClassLoader(urls, null);
        URLClassLoader b = new URLClassLoader(urls, null);

        try {
            // test dup classes
            Class.forName("TestSimple", true, a);
            Class.forName("TestSimple", true, b);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }


}

