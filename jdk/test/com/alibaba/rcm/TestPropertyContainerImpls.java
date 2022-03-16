/*
 * @test
 * @summary Test ResourceType.SYSTEM_PROPERTY for tenant and wisp implementations
 * @library /lib/testlibrary
 * @run main/othervm -Dcom.alibaba.resourceContainer.propertyIsolation=true -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 TestPropertyContainerImpls wisp
 * @run main/othervm -Dcom.alibaba.resourceContainer.propertyIsolation=true -XX:+MultiTenant TestPropertyContainerImpls tenant
 */

import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceContainerFactory;
import com.alibaba.tenant.TenantContainerFactory;
import com.alibaba.wisp.engine.WispResourceContainerFactory;

import java.util.Collections;

import static jdk.testlibrary.Asserts.assertEQ;

public class TestPropertyContainerImpls {

    private final static String KEY = "rc.type";

    public static void main(String[] args) {
        ResourceContainerFactory factory = args[0].contains("wisp") ?
                WispResourceContainerFactory.instance() :
                TenantContainerFactory.instance();

        ResourceContainer rc = factory.createContainer(Collections.emptyList());

        // Isolation
        rc.run(() -> System.setProperty(KEY, "propRC"));
        assertEQ(System.getProperty(KEY), null);
        rc.run(() -> assertEQ(System.getProperty(KEY), "propRC"));
    }
}
