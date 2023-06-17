import java.net.URL;
import java.net.URLClassLoader;
import java.lang.reflect.Field;

import sun.misc.URLClassPath;

public class MyWebAppClassLoader extends URLClassLoader {
    private final URLClassPath myucp;
    public MyWebAppClassLoader(URL[] urls, ClassLoader parent) {
        super(new URL[0], parent);
        this.myucp = new URLClassPath(urls, null);
    }

    @Override
    public Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            Class<URLClassLoader> clazz = URLClassLoader.class;
            Field field = clazz.getDeclaredField("ucp");
            field.setAccessible(true);
            field.set(this, myucp);
        } catch (Exception e) {
            e.printStackTrace();
        }
        return super.findClass(name);
    }
}
