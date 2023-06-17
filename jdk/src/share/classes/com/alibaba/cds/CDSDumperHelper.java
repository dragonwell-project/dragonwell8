package com.alibaba.cds;

import com.alibaba.util.CDSDumpHook;
import com.alibaba.util.QuickStart;
import com.alibaba.util.Utils;

import java.nio.file.Paths;

public class CDSDumperHelper {

    private static String cdsDumper;
    public static void setCdsDumper(String cdsDumper) {
        CDSDumperHelper.cdsDumper = cdsDumper;
    }

    private static String nonNullString(String str) {
        return str == null ? "" : str;
    }

    private static String[] restoreCommandLineOptions(String[] arguments) {
        // if user specifies '-Dcom.alibaba.wisp.threadAsWisp.black=name:process\ reaper\;name:epollEventLoopGroup\*' as the command line:
        // VM will change it to '-Dcom.alibaba.wisp.threadAsWisp.black=name:process reaper;name:epollEventLoopGroup*',
        // in which the escape character is removed. We will concat all of them with a ' ' (space character) because
        // users could use any character inside the command line. So we need to restore the ' ' inside the option back
        // to '\ ': '-Dcom.alibaba.wisp.threadAsWisp.black=name:process\ reaper;name:epollEventLoopGroup*'
        for (int i = 0; i < arguments.length; i++) {
            arguments[i] = arguments[i].replace(" ", "\\ ");
        }
        return arguments;
    }

    public static void invokeCDSDumper() {
        CDSDumpHook.Info info = CDSDumpHook.getInfo();
        boolean verbose = QuickStart.isVerbose();

        String jdkHome = Utils.getJDKHome();
        String vmOptions = getVmOptions();
        String classPath = getClassPath();
        Utils.runProcess(verbose, "[CDSDumper] ", (pb) -> {
                    // clear up agent options because cds dump phase cannot live with java agent in peace
                    String toolOp = Utils.removeAgentOp();
                    if (toolOp != null) {
                        pb.environment().put(Utils.JAVA_TOOL_OPTIONS, toolOp);
                    }
                },
                Paths.get(jdkHome, "bin", "java").toString(),
                "-cp",
                Paths.get(jdkHome, "lib", System.getProperty("os.arch"), QuickStart.getServerlessAdapter()).toString(),
                cdsDumper,                                              // invoke CDSDumper
                QuickStart.cachePath(),                                 // arg[0]: String dirPath
                info.originClassListName,                               // arg[1]: String originClassListName
                info.finalClassListName,                                // arg[2]: String finalClassListName
                Boolean.toString(info.eager),                           // arg[3]: boolean eager
                info.jsaName,                                           // arg[4]: String jsaName
                nonNullString(info.agent),                              // arg[5]: String agent
                Boolean.toString(verbose),                              // arg[6]: boolean verbose
                vmOptions,                                              // arg[7]: String runtimeCommandLine
                classPath                                               // arg[8]: String cp
        );
    }

    private static String getClassPath() {
        if (QuickStart.isDumper()) {
            return QuickStart.getClassPathInProfileStage();
        } else {
            return System.getProperty("java.class.path");
        }
    }

    private static String getVmOptions() {
        if (QuickStart.isDumper()) {
            return Utils.getVMRuntimeArguments(QuickStart.getVmOptionsInProfileStage());
        } else {
            return String.join(" ", restoreCommandLineOptions(Utils.getCommandLine()));
        }
    }
}
