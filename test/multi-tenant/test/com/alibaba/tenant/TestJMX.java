package test.com.alibaba.tenant;
import static jdk.testlibrary.Asserts.*;
import java.lang.management.ManagementFactory;
import java.util.Set;

import javax.management.MBeanInfo;
import javax.management.MBeanServer;
import javax.management.MBeanServerInvocationHandler;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.StandardMBean;
import org.junit.Test;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantException;

/* @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary JMX related unit tests
 * @library /lib/testlibrary
 * @compile TestJMX.java
 * @run junit/othervm/timeout=300 -XX:+MultiTenant -XX:+TenantDataIsolation -XX:+TenantHeapThrottling -XX:+UseG1GC
 *                                -XX:+TenantCpuThrottling -Xmx600m  -Xms600m  test.com.alibaba.tenant.TestJMX
 */
public class TestJMX {
    public static interface MXBean {
        public String getName();
    }

    public class MXBeanImpl implements MXBean {
        public String getName() {
             return "test";
        }
     }

    private void registerAndVerifyMBean(MBeanServer mbs) {
        try {
            ObjectName myInfoObj = new ObjectName("com.alibaba.tenant.mxbean:type=MyTest");
            MXBeanImpl myMXBean = new MXBeanImpl();
            StandardMBean smb = new StandardMBean(myMXBean, MXBean.class);
            mbs.registerMBean(smb, myInfoObj);

            assertTrue(mbs.isRegistered(myInfoObj));

            //call the method of MXBean
            MXBean mbean =
                    (MXBean)MBeanServerInvocationHandler.newProxyInstance(
                         mbs,new ObjectName("com.alibaba.tenant.mxbean:type=MyTest"), MXBean.class, true);
            assertTrue("test".equals(mbean.getName()));

            Set<ObjectInstance> instances = mbs.queryMBeans(new ObjectName("com.alibaba.tenant.mxbean:type=MyTest"), null);
            ObjectInstance instance = (ObjectInstance) instances.toArray()[0];
            assertTrue(myMXBean.getClass().getName().equals(instance.getClassName()));

            MBeanInfo info = mbs.getMBeanInfo(myInfoObj);
            assertTrue(myMXBean.getClass().getName().equals(info.getClassName()));
        } catch (Exception e) {
            e.printStackTrace();
            fail();
        }
    }

    @Test
    public void testMBeanServerIsolation() {
        //verify in root.
        registerAndVerifyMBean(ManagementFactory.getPlatformMBeanServer());

        TenantConfiguration tconfig   = new TenantConfiguration(1024, 100 * 1024 * 1024);
        final TenantContainer tenant  = TenantContainer.create(tconfig);
        final TenantContainer tenant2 = TenantContainer.create(tconfig);

        try {
            tenant.run(() -> {
                //verify in the tenant 1.
                registerAndVerifyMBean(ManagementFactory.getPlatformMBeanServer());
            });
        } catch (TenantException e) {
            e.printStackTrace();
           fail();
        }

        try {
            tenant2.run(() -> {
                //verify in the tenant 1.
                registerAndVerifyMBean(ManagementFactory.getPlatformMBeanServer());
            });
        } catch (TenantException e) {
            e.printStackTrace();
            fail();
        }
    }

    public static void main(String[] args) throws Exception {
        new TestJMX().testMBeanServerIsolation();
    }
}
