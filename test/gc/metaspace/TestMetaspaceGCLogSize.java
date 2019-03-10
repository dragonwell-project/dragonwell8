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

/*
 * @test
 * @summary test the fix of metaspace gc log size
 * @library /testlibrary
 * @build TestMetaspaceGCLogSize
 * @run main/othervm -Xmx1g -Xmn500m -XX:+UseConcMarkSweepGC TestMetaspaceGCLogSize
 *
 */

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryManagerMXBean;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryUsage;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.oracle.java.testlibrary.*;
import static com.oracle.java.testlibrary.Asserts.*;

public class TestMetaspaceGCLogSize {
    public static long used = 0;

    // the metaspace log is [Metaspace: pre_metadata_used->used_byte(reserved byte)]
    // in original gc log, pre_metadata_used is always equal with used_byte because it prints log before cheaning and resizing metaspace
    // after the fix, the used_byte in gc log will be the value after the metaspace cleaned and resized
    public static void main (String[] args) throws Exception {
        if (!Platform.isLinux()) { return; }
        ProcessBuilder pb = ProcessTools.createJavaProcessBuilder("-Xmx1g",
                "-Xmn500m",
                "-XX:+PrintGCDetails",
                "-verbose:gc",
                "-XX:+UseConcMarkSweepGC",
                TestMetaspaceGCLog.class.getName());
        pb.environment().put("LC_ALL", "");
        OutputAnalyzer output = new OutputAnalyzer(pb.start());
        // the top 2-12 logs are the System GC log, the 13'th row is the  metaspace used bytes, and the last is the summary of the heap
        // here only consider the top 13 rows in the stdout
        // the 12'th row is the last system.gc log
        // the 13'th row is the metaspace used size after doing the last system.gc which obtained by the JMX bean
        String stdout = output.getStdout();
        String [] strOut = stdout.split("\n");
        String result = strOut[12];
        assertTrue(strOut[11].contains(result));

        // same test when doing the LC_ALL=C
        pb.environment().put("LC_ALL", "C");
        output = new OutputAnalyzer(pb.start());
        stdout = output.getStdout();
        strOut = stdout.split("\n");
        result = strOut[12];
        assertTrue(strOut[11].contains(result));
    }

    // class for test the metaspace gc log after system.gc
    public static class TestMetaspaceGCLog {
        private static final Map<String, Foo> loadedClasses = new HashMap<>();

        private static int counter = 0;

        protected static MemoryPoolMXBean pool = getMemoryPool();

        protected static void loadNewClasses(int times, boolean keepRefs) {
            for (int i = 0; i < times; i++) {
                try {
                    String jarUrl = "file:" + counter +".jar";
                    counter++;
                    URL[] urls = new URL[]{new URL(jarUrl)};
                    URLClassLoader cl = new URLClassLoader(urls);
                    TestMetaspaceGCLog.Foo foo = (TestMetaspaceGCLog.Foo) Proxy.newProxyInstance(cl,
                            new Class[]{TestMetaspaceGCLog.Foo.class},
                            new TestMetaspaceGCLog.FooInvocationHandler(new TestMetaspaceGCLog.FooBar()));
                    if (keepRefs) {
                        loadedClasses.put(jarUrl, foo);
                    }
                } catch (MalformedURLException e) {
                    e.printStackTrace();
                }
            }
        }

        protected static void cleanLoadedClasses() {
            loadedClasses.clear();
        }

        protected static void printUsedSize() {
            MemoryUsage mu = pool.getUsage();
            System.out.println(mu.getUsed()/1024 + "K");
        }

        protected static MemoryPoolMXBean getMemoryPool () {
            List<MemoryPoolMXBean> pools  = ManagementFactory.getMemoryPoolMXBeans();
            MemoryPoolMXBean mpool = null;
            for (MemoryPoolMXBean pool : pools) {
                if (pool.getName().equals("Metaspace")) {
                    mpool = pool;
                }
            }
            return mpool;
        }

        public static void checkMetaspaceGC() {
            // the "printUsedSize" here is for loaded JMX and stdout class previously
            printUsedSize();
            // the metaspace used size will change a little between System.gc and the MemoryPoolMXBean at the beginning
            // here is a loop to make the metaspace stabilizing
            for (int i = 0; i < 10; i++) {
                loadNewClasses(1000, true);
                cleanLoadedClasses();
                System.gc();
            }
            loadNewClasses(1000, true);
            cleanLoadedClasses();
            System.gc();
            printUsedSize();
        }

        public static void main(String[] args) {
            checkMetaspaceGC();
        }

        public static interface Foo {}

        static public class FooBar implements Foo {}

        static public class FooInvocationHandler implements InvocationHandler {
            private final Foo foo;

            FooInvocationHandler(TestMetaspaceGCLog.Foo foo) {
                this.foo = foo;
            }

            @Override
            public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                return method.invoke(foo, args);
            }
        }
    }
}

