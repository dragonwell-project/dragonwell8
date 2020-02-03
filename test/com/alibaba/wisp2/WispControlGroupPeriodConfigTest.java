/*
 * @test
 * @library /lib/testlibrary
 * @summary test cfs period configuration
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.controlGroup.cfsPeriod=200000 WispControlGroupPeriodConfigTest
 */

import java.lang.Long;
import java.lang.reflect.Method;
import java.lang.reflect.Field;

import java.security.MessageDigest;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.ExecutionException;

import static jdk.testlibrary.Asserts.*;

public class WispControlGroupPeriodConfigTest {

    public static void main(String[] args) throws Exception {
        int cpuRate = 5;
        Class<?> WispControlGroupClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup");
        Class<?> CpuLimitClazz = Class.forName("com.alibaba.wisp.engine.WispControlGroup$CpuLimit");

        Field fieldCpuLimit = WispControlGroupClazz.getDeclaredField("cpuLimit");
        Field fieldPeriod = CpuLimitClazz.getDeclaredField("cfsPeriod");
        Field fieldQuota = CpuLimitClazz.getDeclaredField("cfsQuota");

        Method createMethod = WispControlGroupClazz.getDeclaredMethod("create", int.class);
        createMethod.setAccessible(true);
        ExecutorService cg = (ExecutorService) createMethod.invoke(null, cpuRate);

        fieldPeriod.setAccessible(true);
        fieldQuota.setAccessible(true);
        fieldCpuLimit.setAccessible(true);
        long cfsPeriod = fieldPeriod.getLong(fieldCpuLimit.get(cg));
        long cfsQuota = fieldQuota.getLong(fieldCpuLimit.get(cg));

        assertTrue(cfsPeriod == 200000 * 1000,
                "expected cfs period value should equal to (200000(Us) * 1000)(ns) configured");
        assertTrue(cfsQuota == 200000 * 1000 * cpuRate / 100,
                "expected cfs Quota value should be cfsPeriod * cpuRate / 100");
    }
}