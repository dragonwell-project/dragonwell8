/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
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
 */

import static jdk.testlibrary.Asserts.*;

/*
 * @test
 * @library /lib/testlibrary
 * @summary test thread as wisp still keep the daemon semantic
 * @requires os.family == "linux"
 * @run main DaemonTest
 */
public class DaemonTest {

    private static final String SHUTDOWN_MSG = "[Run ShutdownHook]";

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            driver(true);  // test daemon will not prevent shutdown
            driver(false); // test non-daemon will prevent shutdown
        } else {
            assert Thread.currentThread().getName().equals("main");
            boolean daemon = Boolean.valueOf(args[0]);

            // start a non-daemon to cover the `--nonDaemonCount == 0` branch
            Thread thread = new Thread(() -> {/**/});
            thread.setDaemon(false);
            thread.start();

            Runtime.getRuntime().addShutdownHook(new Thread(() -> System.out.println(SHUTDOWN_MSG)));
            thread = new Thread(() -> {
                System.out.println("thread started..");
                if (!daemon) {
                    try {
                        Thread.sleep(2000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            });
            thread.setDaemon(daemon);
            thread.start();
            Thread.sleep(1000);
        }
    }

    private static void driver(boolean daemon) throws Exception {
        // we can not use jdk.testlibrary.ProcessTools here, because we need to analyse stdout of a unfinished process
        Process process = new ProcessBuilder(System.getProperty("java.home") + "/bin/java", "-XX:+UnlockExperimentalVMOptions",
                "-XX:+UseWisp2", "-cp", System.getProperty("java.class.path"), DaemonTest.class.getName(), Boolean.toString(daemon)).start();
        Thread.sleep(2000);
        byte[] buffer = new byte[1024];
        int n = process.getInputStream().read(buffer);
        String s = new String(buffer, 0, n);
        assertEQ(daemon, s.contains(SHUTDOWN_MSG));
    }
}
