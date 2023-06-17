import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class TestAppClassLoader {
    public static void main(String... args) throws Exception {
        //  class loader with name
        testAppClassLoader();
    }

    public static void testAppClassLoader() throws Exception {
        ClassLoader loader = TestAppClassLoader.class.getClassLoader();
        Class<?> c = Class.forName("trivial.ThrowException", true, loader);
        Method method = c.getMethod("throwError");
        try {
            // invoke p.ThrowException::throwError
            method.invoke(null);
        } catch (InvocationTargetException x) {
        }
    }
}

