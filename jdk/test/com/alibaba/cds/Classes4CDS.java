import com.alibaba.util.QuickStart;
import com.alibaba.util.Utils;
import jdk.test.lib.process.ProcessTools;

import java.nio.file.Paths;

public class Classes4CDS {

    // invoke Classes4CDS in the serverless-adapter

    public static final String CLASSES4CDS = "com.alibaba.jvm.cds.Classes4CDS";

    public static ProcessBuilder invokeClasses4CDS(String cds_original_list, String cds_final_list) {
        return ProcessTools.createJavaProcessBuilder(true,
                "-cp",
                Paths.get(Utils.getJDKHome(), "lib", System.getProperty("os.arch"), QuickStart.getServerlessAdapter()).toString(),
                CLASSES4CDS,
                cds_original_list,
                cds_final_list);
    }
}
