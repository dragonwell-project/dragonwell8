import com.alibaba.rcm.Constraint;
import com.alibaba.rcm.ResourceContainer;
import com.alibaba.tenant.TenantConfiguration;
import com.alibaba.tenant.TenantContainer;
import com.alibaba.tenant.TenantContainerFactory;
import com.alibaba.tenant.TenantGlobals;
import com.alibaba.wisp.engine.WispResourceContainerFactory;
import java.lang.reflect.Field;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import static jdk.testlibrary.Asserts.fail;

public class RcmUtils {
    public static final boolean IS_WISP_ENABLED;
    public static final boolean IS_TENANT_ENABLED = TenantGlobals.isTenantEnabled();

    static {
        boolean isWispEnabled = false;
        try {
            Field f = Class.forName("com.alibaba.wisp.engine.WispConfiguration")
                    .getDeclaredField("ENABLE_THREAD_AS_WISP");
            f.setAccessible(true);
            isWispEnabled = f.getBoolean(null);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        IS_WISP_ENABLED = isWispEnabled;
    }

    public static ResourceContainer createContainer(Iterable<Constraint> constraints) {
        if (IS_WISP_ENABLED) {
            return WispResourceContainerFactory.instance()
                    .createContainer(constraints);
        } else if (IS_TENANT_ENABLED) {
            return TenantContainerFactory.instance()
                    .createContainer(constraints);
        } else {
            fail("Unsupported scenarios");
        }
        return null;
    }

    public static ResourceContainer createContainer(Constraint... constraints) {
        return createContainer(Stream.of(constraints).collect(Collectors.toList()));
    }
}
