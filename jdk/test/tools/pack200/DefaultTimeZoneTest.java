/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.TimeZone;
import java.util.concurrent.CountDownLatch;
import java.util.jar.JarFile;
import java.util.jar.JarOutputStream;
import java.util.jar.Pack200;

/*
 * @test
 * @bug 8066985
 * @summary multithreading packing/unpacking files can result in Timezone set to UTC
 * @compile -XDignore.symbol.file Utils.java DefaultTimeZoneTest.java
 * @run main/othervm DefaultTimeZoneTest
 * @author mcherkas
 */


public class DefaultTimeZoneTest {

    private static final TimeZone tz = TimeZone.getTimeZone("Europe/Moscow");
    private static final int INIT_THREAD_COUNT = Math.min(4, Runtime.getRuntime().availableProcessors());
    private static final int MAX_THREAD_COUNT = 4 * INIT_THREAD_COUNT;
    private static final long MINUTE = 60 * 1000;

    private static class NoOpOutputStream extends OutputStream {
        @Override
        public void write(int b) throws IOException {
            // no op
        }
    }

    static class PackAction implements Runnable {
        private Pack200.Packer packer = Pack200.newPacker();
        private JarFile jarFile;

        PackAction() throws IOException {
            jarFile = new JarFile(new File("golden.jar"));
        }

        @Override
        public void run() {
            try {
                packer.pack(jarFile, new NoOpOutputStream());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    static class UnpackAction implements Runnable {
        private Pack200.Unpacker unpacker = Pack200.newUnpacker();
        private JarOutputStream jos;
        private File packedJar = new File("golden.pack");

        UnpackAction() throws IOException {
            jos = new JarOutputStream(new NoOpOutputStream());
        }

        @Override
        public void run() {
            try {
                unpacker.unpack(packedJar, jos);
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                try {
                    jos.close();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        }
    };

    public static void test(final Class<? extends Runnable> runnableClass) throws InterruptedException {
        for (int i = INIT_THREAD_COUNT; i <= MAX_THREAD_COUNT; i*=2) {
            final CountDownLatch startLatch = new CountDownLatch(i);
            final CountDownLatch doneLatch = new CountDownLatch(i);
            for (int j = 0; j < i; j++) {
                new Thread() {
                    @Override
                    public void run() {
                        try {
                            Runnable r = runnableClass.newInstance();
                            startLatch.countDown();
                            startLatch.await();
                            r.run();
                        } catch (Exception e) {
                            throw new RuntimeException(e);
                        } finally {
                            doneLatch.countDown();
                        }
                    }
                }.start();
            }
            doneLatch.await();

            if(!TimeZone.getDefault().equals(tz)) {
                throw new RuntimeException("FAIL: default time zone was changed");
            }
        }
    }

    public static void main(String args[]) throws IOException, InterruptedException {
        TimeZone.setDefault(tz);

        // make a local copy of our test file
        File srcFile = Utils.locateJar("golden.jar");
        final File goldenFile = new File("golden.jar");
        Utils.copyFile(srcFile, goldenFile);

        // created packed file
        final JarFile goldenJarFile = new JarFile(goldenFile);
        final File packFile = new File("golden.pack");
        Utils.pack(goldenJarFile, packFile);

        // before test let's unpack golden pack to warm up
        // a native unpacker. That allow us to avoid JDK-8080438
        UnpackAction unpackAction = new UnpackAction();
        unpackAction.run();

        long startTime = System.currentTimeMillis();
        while(System.currentTimeMillis() - startTime < MINUTE) {
            // test packer
            test(PackAction.class);

            // test unpacker
            test(UnpackAction.class);
        }

        Utils.cleanup();
    }
}
