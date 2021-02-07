/*
 * @test
 * @library /lib/testlibrary
 * @summary test RcmMXBeanTest
 * @run main/othervm  -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -XX:ActiveProcessorCount=4 RcmMXBeanTest
 */

import com.alibaba.management.ResourceContainerMXBean;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.rcm.ResourceType;

import javax.management.MBeanServer;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.util.Collections;
import com.alibaba.wisp.engine.WispResourceContainerFactory;

import static jdk.testlibrary.Asserts.*;

public class RcmMXBeanTest {
    static ResourceContainerMXBean resourceContainerMXBean;

    public static void main(String[] args) throws Exception {
        MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
        try {
            resourceContainerMXBean = ManagementFactory.newPlatformMXBeanProxy(mbs,
                    "com.alibaba.management:type=ResourceContainer", ResourceContainerMXBean.class);
        } catch (IOException e) {
            e.printStackTrace();
        }

        for (int i = 0; i < 3; i++) {
            ResourceContainer rc1 = WispResourceContainerFactory.instance()
                    .createContainer(Collections.singletonList(ResourceType.CPU_PERCENT.newConstraint(80)));

            rc1.run(() -> {
                new Thread(() -> {
                    while (true) {
                        Thread.yield();
                    }
                }).start();
            });
            Thread.sleep(1000);
        }

        assertTrue(resourceContainerMXBean.getAllContainerIds().size() == 4);


        for (long id : resourceContainerMXBean.getAllContainerIds()) {
            if (id != 0) {
                assertTrue(resourceContainerMXBean.getConstraintsById(id).size() == 1);
                assertEQ(resourceContainerMXBean.getConstraintsById(id).get(0), 80L);
                assertGreaterThan(resourceContainerMXBean.getCPUResourceConsumedAmount(id), 0L);
            } else {
                assertTrue(resourceContainerMXBean.getConstraintsById(id).isEmpty());
            }
        }
    }
}
