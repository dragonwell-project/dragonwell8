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

package com.alibaba.wisp.engine;

import java.util.List;
import java.util.stream.Stream;

/**
 * Convert Thread.start() to Wisp without changing application's code.
 * <p/>
 * Note that ThreadAsWisp is not wisp core logic, just a wrapper of wisp
 * coroutine create API which is used by jdk threading library, so we can
 * use objectMonitor here.
 */
class ThreadAsWisp {
    private final static String JAVA_LANG_PKG = "package:java.lang";

    private static int nonDaemonCount;
    private static Thread preventShutdownThread;


    /**
     * Try to "start thread" as wisp if all listed condition is satisfied:
     * <p>
     * 1. not in java.lang or blacklisted package/class
     * 2. not a WispEngine internal Thread
     * 3. allThreadAsWisp is true and not match the blacklist
     *
     * @param thread the thread
     * @param target thread's target field
     * @return if condition is satisfied and thread is started as wisp
     */
    static boolean tryStart(Thread thread, Runnable target, long stackSize) {
        if (WispEngine.isEngineThread(thread)) {
            return false;
        }
        if (WispConfiguration.ALL_THREAD_AS_WISP) {
            if (matchList(thread, target, WispConfiguration.getThreadAsWispBlacklist())) {
                return false;
            }
        } else if (!matchList(thread, target, WispConfiguration.getThreadAsWispWhitelist())) {
            return false;
        }

        if (!thread.isDaemon()) {
            synchronized (ThreadAsWisp.class) {
                if (nonDaemonCount++ == 0) { // start a non-daemon thread to prevent jvm exit
                    assert preventShutdownThread == null;
                    preventShutdownThread = new PreventShutdownThread();
                    preventShutdownThread.start();
                }
            }
        }

        // pthread_create always return before new thread started, so we should not wait here
        WispEngine.JLA.setWispAlive(thread, true); // thread.isAlive() should be true
        WispEngine.current().startAsThread(thread, thread.getName(), thread, stackSize);
        return true;
    }

    /**
     * Refer to hotspot JavaThread::exit():
     * <p>
     * 1. Call Thread.exit() to clean up threadGroup
     * 2. Notify threads wait on Thread.join()
     * 3. exit jvm if all non-daemon thread is exited
     *
     * @param thread exited thread
     */
    static void exit(Thread thread) {
        WispEngine.JLA.threadExit(thread);
        synchronized (thread) {
            thread.notifyAll();
        }
        if (!thread.isDaemon()) {
            synchronized (ThreadAsWisp.class) {
                if (--nonDaemonCount == 0) {
                    assert preventShutdownThread != null && !preventShutdownThread.isInterrupted();
                    preventShutdownThread.interrupt();
                    preventShutdownThread = null;
                }
            }
        }
    }

    private static boolean matchList(Thread thread, Runnable target, List<String> list) {
        return Stream.concat(Stream.of(JAVA_LANG_PKG), list.stream())
                .anyMatch(s -> {
                    Class<?> clazz = (target == null ? thread : target).getClass();
                    // Java code could start a thread by passing a `target` Runnable argument
                    // or override Thread.run() method;
                    // if `target` is null, we're processing the override situation, so we need
                    // to check the thread object's class.
                    String[] sp = s.split(":");
                    if (sp.length != 2) {
                        return false;
                    }
                    switch (sp[0]) {
                        case "class":
                            return sp[1].equals(clazz.getName());
                        case "package":
                            Package pkg = clazz.getPackage();
                            String pkgName = pkg == null ? "" : pkg.getName();
                            return sp[1].equals(pkgName);
                        case "name":
                            return wildCardMatch(thread.getName(), sp[1]);
                        default:
                            return false;
                    }
                });
    }

    private static boolean wildCardMatch(String s, String p) {
        return s.matches(p.replace("?", ".?").replace("*", ".*"));
    }


    private static class PreventShutdownThread extends Thread {
        // help us monitor if PreventShutdownThread start/exit too much times.
        // startCount should very close to 1 for most applications
        private static int startCount;

        private PreventShutdownThread() {
            super(WispEngine.DAEMON_THREAD_GROUP, "Wisp-Prevent-Shutdown-" + startCount++);
            setDaemon(false); // the daemon attribute is inherited from parent, set to false explicitly
        }

        @Override
        public synchronized void run() {
            while (true) {
                try {
                    wait();
                } catch (InterruptedException e) {
                    break;
                }
            }
        }
    } // class PreventShutdownThread
}
