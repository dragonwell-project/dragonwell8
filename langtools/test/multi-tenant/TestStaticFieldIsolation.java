
/*
 * @test
 * @summary Test isolation of various static fields
 * @run main/othervm -XX:+MultiTenant -XX:+TenantDataIsolation TestStaticFieldIsolation
 */

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;
import javax.tools.ToolProvider;


public class TestStaticFieldIsolation {

    public static void main(String[] args) throws TenantException {
        TestStaticFieldIsolation test = new TestStaticFieldIsolation();
        test.testIsolation_com_sun_tools_javac_file_ZipFileIndexCache_sharedInstance();
        test.testIsolation_javax_tools_ToolProvider_getSystemJavaCompiler();
    }

    private void testIsolation_com_sun_tools_javac_file_ZipFileIndexCache_sharedInstance() throws TenantException {
        // find the class
        ClassLoader toolsLoader = ToolProvider.getSystemJavaCompiler().getClass().getClassLoader();
        Class clazz[] = new Class[1];
        try {
            clazz[0] = toolsLoader.loadClass("com.sun.tools.javac.file.ZipFileIndexCache");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
            fail();
        } finally {
            assertNotNull(clazz[0]);
        }


        Runnable task = () -> {
            try {
                Method f = clazz[0].getDeclaredMethod("getSharedInstance");
                f.setAccessible(true);
                f.invoke(null);
            } catch (Throwable t) {
                fail();
            }
        };
        testStaticFieldIsolation(clazz[0], task, "sharedInstance");
    }

    private void testIsolation_javax_tools_ToolProvider_getSystemJavaCompiler() throws TenantException {
        Runnable task = () -> {
            try {
                ToolProvider.getSystemJavaCompiler();
            } catch (Throwable t) {
                fail();
            }
        };
        testStaticFieldIsolation(ToolProvider.class, task, "instance");
    }


    /**
     * test isolation of {@code classKay}'s static field {@code fieldName}
     * @param key               The class contains target static field
     * @param tenantTask        Task to trigger the static filed isolation per tenant,
     *                          which is stored in TenantContainer.tenantData
     * @param fieldName         Field name
     * @throws TenantException
     */
    private static <K> void testStaticFieldIsolation(K key, Runnable tenantTask, String fieldName)
            throws TenantException {
        TenantContainer tenant = createSimpleTenant();

        assertNull(tenant.getFieldValue(key, fieldName));

        // run twice to initialize field in ROOT and non-ROOT
        tenant.run(tenantTask);
        tenantTask.run();

        Object isolatedFieldValue = tenant.getFieldValue(key, fieldName);
        assertNotNull(isolatedFieldValue);

        try {
            Class clazz = null;
            if (key instanceof Class) {
                clazz = (Class)key;
            } else {
                clazz = key.getClass();
            }
            Field field = clazz.getDeclaredField(fieldName);
            field.setAccessible(true);
            Object rootValue = field.get(null);

            assertTrue(rootValue != isolatedFieldValue);
        } catch (NoSuchFieldException | IllegalAccessException e) {
            e.printStackTrace();
            fail();
        }

        tenant.destroy();
    }

    // convenient static method to create a simple {@code TenantContainer} object
    private static TenantContainer createSimpleTenant() {
        TenantConfiguration config = new TenantConfiguration(1024, 64 * 1024 * 1024);
        return TenantContainer.create(config);
    }

    private static void assertNull(Object o) {
        if (o != null) {
            throw new RuntimeException("assertNull failed!");
        }
    }

    private static void assertNotNull(Object o) {
        if (o == null) {
            throw new RuntimeException("assertNotNull failed!");
        }
    }

    private static void fail() { throw new RuntimeException("Failed!"); }

    private static void assertTrue(Boolean b) {
        if (!b.booleanValue()) {
            throw new RuntimeException("assertTrue failed!");
        }
    }
}
