
/*
 * @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary Test isolation of various static fields
 * @library /lib/testlibrary
 * @run main/othervm -XX:+UseG1GC -XX:+MultiTenant -XX:+TenantDataIsolation TestStaticFieldIsolation
 */

import static jdk.testlibrary.Asserts.*;
import java.lang.reflect.Field;
import java.util.ResourceBundle;
import java.util.concurrent.atomic.AtomicInteger;
import javax.management.MBeanServerFactory;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;


public class TestStaticFieldIsolation {

    public static void main(String[] args) throws TenantException {
        TestStaticFieldIsolation test = new TestStaticFieldIsolation();
        test.testIsolation_java_lang_ResourceBundle_cacheList();
        test.testIsolation_java_sql_DriverManager_registeredDrivers();
        test.testIsolation_javax_management_MBeanServerFactory_mBeanServerList();
        test.testIsolation_java_lang_Class_classValueMap();
    }

    private void testIsolation_java_lang_ResourceBundle_cacheList() throws TenantException {
        Runnable task = () -> {
            try {
                ResourceBundle.getBundle("NON_EXIST");
            } catch (Throwable t) { /* ignore */ }
        };
        testStaticFieldIsolation(ResourceBundle.class, task, "cacheList");
    }

    private void testIsolation_javax_management_MBeanServerFactory_mBeanServerList() throws TenantException {
        Runnable task = () -> {
            try {
                MBeanServerFactory.findMBeanServer(null);
            } catch (Throwable t) { /* ignore */ }
        };
        testStaticFieldIsolation(MBeanServerFactory.class, task, "mBeanServerList");
    }

    private void testIsolation_java_sql_DriverManager_registeredDrivers() throws TenantException {
        Runnable task = () -> {
            try {
                java.sql.DriverManager.getDrivers();
            } catch (Throwable t) { /* ignore */ }
        };
        testStaticFieldIsolation(java.sql.DriverManager.class, task, "registeredDrivers");
    }

    class TestClass {
        int val;
    }

    private void testIsolation_java_lang_Class_classValueMap() throws TenantException {
        final AtomicInteger atomicInteger = new AtomicInteger(0);

        ClassValue<TestClass> value = new ClassValue<TestClass>() {
            @Override protected TestClass computeValue(Class<?> type) {
                TestClass obj = new TestClass();
                obj.val = atomicInteger.getAndIncrement();
                return obj;
            }
        };

        TestClass tc = value.get(TestClass.class);
        assertEquals(tc.val, 0);
        TestClass tc2 = value.get(TestClass.class);
        assertTrue(tc == tc2);

        TestClass[] receivers = new TestClass[2];
        TenantContainer tenant = createSimpleTenant();
        tenant.run(() -> {
            receivers[0] = value.get(TestClass.class);
        });
        assertEquals(receivers[0].val, 1);

        tenant.run(() -> {
            receivers[1] = value.get(TestClass.class);
        });
        assertEquals(receivers[1].val, 1);
        assertTrue(receivers[1] == receivers[0]);
        assertTrue(receivers[0] != tc);


    }

    /**
     * test isolation of {@code classKay}'s static field {@code fieldName}
     * @param classKey          The class contains target static field
     * @param tenantTask        Task to trigger the static filed isolation per tenant,
     *                          which is stored in TenantContainer.tenantData
     * @param fieldName         Field name
     * @throws TenantException
     */
    private static void testStaticFieldIsolation(Class classKey, Runnable tenantTask, String fieldName)
            throws TenantException {
        TenantContainer tenant = createSimpleTenant();

        assertNull(tenant.getFieldValue(classKey, fieldName));

        tenant.run(tenantTask);

        Object isolatedFieldValue = tenant.getFieldValue(classKey, fieldName);
        assertNotNull(isolatedFieldValue);

        try {
            Field field = classKey.getDeclaredField(fieldName);
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
}
