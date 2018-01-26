/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

import com.oracle.java.testlibrary.JDKToolFinder;
import com.oracle.java.testlibrary.OutputAnalyzer;
import com.oracle.java.testlibrary.ProcessTools;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

/*
 * @test
 * @summary
 * @library /testlibrary
 * @run main FinalizerInfoTest
 */
public class FinalizerInfoTest {
    static ReentrantLock lock = new ReentrantLock();
    static volatile int wasInitialized = 0;
    static volatile int wasTrapped = 0;
    static final String cmd = "GC.finalizer_info";
    static final int objectsCount = 1000;

    class MyObject {
        public MyObject() {
            // Make sure object allocation/deallocation is not optimized out
            wasInitialized += 1;
        }

        protected void finalize() {
            // Trap the object in a finalization queue
            wasTrapped += 1;
            lock.lock();
        }
    }

    void run() throws Exception {
        try {
            lock.lock();
            for(int i = 0; i < objectsCount; ++i) {
                new MyObject();
            }
            System.out.println("Objects initialized: " + objectsCount);
            System.gc();

            while(wasTrapped < 1) {
                // Waiting for gc thread.
            }


            String pid = Integer.toString(ProcessTools.getProcessId());
            ProcessBuilder pb = new ProcessBuilder();
            pb.command(new String[] { JDKToolFinder.getJDKTool("jcmd"), pid, cmd});
            OutputAnalyzer output = new OutputAnalyzer(pb.start());
            output.shouldContain("MyObject");
        } finally {
            lock.unlock();
        }
    }

    public static void main(String[] args) throws Exception {
        new  FinalizerInfoTest().run();
    }


}
