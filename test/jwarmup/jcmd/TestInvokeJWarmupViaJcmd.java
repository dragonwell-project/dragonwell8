/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

import com.oracle.java.testlibrary.*;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.lang.reflect.Field;
import static com.oracle.java.testlibrary.Asserts.assertTrue;

/*
 * @test TestInvokeJWarmupViaJcmd
 * @library /testlibrary
 * @build TestInvokeJWarmupViaJcmd
 * @run main TestInvokeJWarmupViaJcmd
 * @run main/othervm TestInvokeJWarmupViaJcmd
 * @summary test invoking JWarmup APIs via jcmd
 */
public class TestInvokeJWarmupViaJcmd {
    public static void main(String[] args) throws Exception {
        Recording.run();
        CheckNotifyStartup.run();
        CheckCompilationSuccess.run();
        CheckCompilationFail.run();
        CheckDeopt.run();
    }
}

class Main {
    public static void main(String[] args) throws Exception {
        Main main = new Main();
        for (int i = 0; i < 20000; i++) {
            main.foo();
        }
        // Waiting for recording or jcmd.
        Thread.sleep(12000);
        System.out.println("done!");
    }

    public void foo() {}
}

/**
 * JWarmup command
 */
class JCMDArg {
    public static final String notify = "-notify";
    public static final String check = "-check";
    public static final String deopt = "-deopt";

}

class ProgramArg {
    public static final String Recording = "Recording";
    public static final String NotifyStartup = "NotifyStartup";
    public static final String CheckCompilationSuccess = "CheckCompilationSuccess";
    public static final String CheckCompilationFail = "CheckCompilationFail";
    public static final String Deopt = "Deopt";
}

/**
 * JVM options
 */
class JVMArg {
    public static final List<String> Recording = new ArrayList(Arrays.asList(
            "-XX:-ClassUnloading",
            "-XX:-CMSClassUnloadingEnabled",
            "-XX:-ClassUnloadingWithConcurrentMark",
            "-XX:CompilationWarmUpLogfile=jwarmup.log",
            "-XX:+CompilationWarmUpRecording",
            "-XX:CompilationWarmUpRecordTime=10"
    ));

    public static final List<String> Running = new ArrayList(Arrays.asList(
            "-XX:+CompilationWarmUp",
            "-XX:+CompilationWarmUpExplicitDeopt",
            "-XX:-TieredCompilation",
            "-XX:CompilationWarmUpLogfile=jwarmup.log",
            "-XX:CompilationWarmUpDeoptTime=0"
    ));
}

class Recording {
    public static void run() throws Exception {
        System.out.println("Test Jwarmup recording.");
        ProcessBuilderFactory.create(ProgramArg.Recording, JVMArg.Recording).start();
        Thread.sleep(15000);
        File logFile = new File("./jwarmup.log");
        assertTrue(logFile.exists() && logFile.isFile());
    }
}

class CheckNotifyStartup {
    public static void run() throws Exception {
        System.out.println("Test JWarmup notifyStartup.");
        ProcessBuilder processBuilder = ProcessBuilderFactory.create(ProgramArg.NotifyStartup, JVMArg.Running);
        Process p = processBuilder.start();
        Thread.sleep(5000);
        String pid = PID.get(p);
        assert pid != null;
        JcmdCaller.callJcmd(ProgramArg.NotifyStartup, pid);
    }
}

class CheckCompilationSuccess {
    public static void run() throws Exception {
        System.out.println("Test JWarmup checkCompilation.");
        ProcessBuilder processBuilder = ProcessBuilderFactory.create(ProgramArg.CheckCompilationSuccess, JVMArg.Running);
        Process p = processBuilder.start();
        Thread.sleep(5000);
        String pid = PID.get(p);
        assert pid != null;
        JcmdCaller.callJcmd(ProgramArg.CheckCompilationSuccess, pid);
    }
}

/**
 * Excluding compiling com.alibaba.jwarmup.JWarmUp.dummy() to test a unfinished compilation task.
 */
class CheckCompilationFail {
    public static void run() throws Exception {
        System.out.println("Test JWarmup checkCompilation.");
        List<String> jvmArgs = new ArrayList<>();
        jvmArgs.addAll(JVMArg.Running);
        jvmArgs.add("-XX:CompileCommand=exclude,com/alibaba/jwarmup/JWarmUp,dummy");
        ProcessBuilder processBuilder = ProcessBuilderFactory.create(ProgramArg.CheckCompilationFail, jvmArgs);
        Process p = processBuilder.start();
        String pid = PID.get(p);
        assert pid != null;
        Thread.sleep(5000);
        JcmdCaller.callJcmd(ProgramArg.CheckCompilationFail, pid);
    }
}

class CheckDeopt {
    public static void run() throws Exception {
        System.out.println("Test JWarmup de-optimization.");
        ProcessBuilder processBuilder =  ProcessBuilderFactory.create(ProgramArg.Deopt, JVMArg.Running);
        Process p = processBuilder.start();
        Thread.sleep(5000);
        JcmdCaller.callJcmd(ProgramArg.Deopt, PID.get(p));
    }
}

class ProcessBuilderFactory {
    public static ProcessBuilder create(String programArg, final List<String> jvmArgs) throws Exception {
        assert jvmArgs != null && jvmArgs.size() != 0;
        List<String> _jvmArgs = new ArrayList<>(jvmArgs);
        _jvmArgs.addAll(Arrays.asList(
                Main.class.getName(),
                programArg
        ));
        return ProcessTools.createJavaProcessBuilder(_jvmArgs.stream().toArray(String[]::new));
    }
}

class JcmdCaller {
    public static void callJcmd(String arg, String pid) throws Exception {
        if (ProgramArg.Recording.equals(arg)) {
            return;
        } else if (ProgramArg.NotifyStartup.equals(arg)) {
            ProcessBuilder processBuilder = notifyStartup(pid);
            OutputAnalyzer output = new OutputAnalyzer(processBuilder.start());
            output.shouldContain("Command executed successfully");
        } else if (ProgramArg.CheckCompilationSuccess.equals(arg)) {
            ProcessBuilder processBuilder = checkCompilation(pid);
            OutputAnalyzer output = new OutputAnalyzer(processBuilder.start());
            output.shouldContain("Last compilation task is completed.");
        } else if (ProgramArg.CheckCompilationFail.equals(arg)) {
            ProcessBuilder processBuilder = checkCompilation(pid);
            OutputAnalyzer output = new OutputAnalyzer(processBuilder.start());
            output.shouldContain("Last compilation task is not completed.");
        } else if (ProgramArg.Deopt.equals(arg)) {
            ProcessBuilder processBuilder = deopt(pid);
            OutputAnalyzer output = new OutputAnalyzer(processBuilder.start());
            output.shouldContain("Command executed successfully");
        } else {
            throw new RuntimeException(String.format("Unrecognized argument: %s", arg));
        }
    }

    private static ProcessBuilder notifyStartup(String pid) {
        JDKToolLauncher notify = JDKToolLauncher.create("jcmd")
                .addToolArg(pid)
                .addToolArg("JWarmup")
                .addToolArg(JCMDArg.notify);
        return new ProcessBuilder(notify.getCommand());
    }

    private static ProcessBuilder checkCompilation(String pid) throws Exception {
        // Invoke notifyStartup() before checkCompilation().
        ProcessBuilder processBuilder = notifyStartup(pid);
        Process p = processBuilder.start();
        p.waitFor();
        JDKToolLauncher check = JDKToolLauncher.create("jcmd")
                .addToolArg(pid)
                .addToolArg("JWarmup")
                .addToolArg(JCMDArg.check);
        return new ProcessBuilder(check.getCommand());
    }

    private static ProcessBuilder deopt(String pid) {
        JDKToolLauncher deopt = JDKToolLauncher.create("jcmd")
                .addToolArg(pid)
                .addToolArg("JWarmup")
                .addToolArg(JCMDArg.deopt);
        return new ProcessBuilder(deopt.getCommand());
    }
}

class PID {
    public static String get(Process p) throws Exception {
        if ("java.lang.UNIXProcess".equals(p.getClass().getName())) {
            Field f = p.getClass().getDeclaredField("pid");
            f.setAccessible(true);
            long pid = f.getLong(p);
            f.setAccessible(false);
            return Long.toString(pid);
        } else {
            throw new RuntimeException("Unable to obtain pid.");
        }
    }
}