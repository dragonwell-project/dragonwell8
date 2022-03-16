#!/usr/bin/env bash

#
# Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation. Alibaba designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

#
# @test
# @summary test Thread.getStackTrace() in wisp transparentAsync model
# @requires os.family == "linux"
# @run shell TestThreadStackTrace.sh
#

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
# determine platform dependant variables
OS=`uname -s`
case ${OS} in
  Linux)
    FS=/
    ;;
  *)
    exit 1
    ;;
esac

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
TEST_CLASS=TmpThreadStackTrace
TEST_SOURCE=${TEST_CLASS}.java

###################################################################################


cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import com.alibaba.wisp.engine.WispEngine;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import sun.misc.SharedSecrets;
import java.util.concurrent.Executors;
import java.util.concurrent.*;

public class TmpThreadStackTrace {

    public static Thread runningCoroutine;

    public static void foo() throws Exception {
        ExecutorService executor = Executors.newCachedThreadPool();
        executor.execute(() -> {
            try {
                runningCoroutine = Thread.currentThread();
                Thread.sleep(2_000L);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        });
        AtomicBoolean result = new AtomicBoolean(true);
        CountDownLatch done = new CountDownLatch(1);
        Thread mainThread = Thread.currentThread();
        Thread[] ts = new Thread[1];
        ts[0] = new Thread(() -> {
            System.out.println("in managed!");
            try {
                if (ts[0].getStackTrace().length == 0 || mainThread.getStackTrace().length == 0
                        || Thread.currentThread().getStackTrace().length == 0) {
                    result.set(false);
                }
                runningCoroutine.getStackTrace();
                result.set(false);
            } catch (Exception e) {
                if (!(e instanceof UnsupportedOperationException)) {
                    result.set(false);
                }
            } finally {
                done.countDown();
            }
        }, "test-thread");
        ts[0].start();
        done.await();
        System.out.println("in main");
        executor.shutdown();
        if (!result.get()) {
            throw new Error("test failure");
        }
    }

    public static void main(String[] args) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        WispEngine.current().execute(() -> {
            for (int i = 0; i < 5; i++) {
                System.out.println(i);
                Thread.currentThread().getStackTrace();
            }
            latch.countDown();
        });
        latch.await();
        foo();
        System.out.println("done");
        System.exit(0);
    }
}
EOF

# Do compilation
${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE >> /dev/null 2>&1
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE}"
	exit 1
fi

#run
${JAVA} -XX:+PrintSafepointStatistics  -XX:PrintSafepointStatisticsCount=1 -XX:+UnlockExperimentalVMOptions -XX:+UseWisp2 -Dcom.alibaba.wisp.carrierEngines=1 -cp ${TESTCLASSES} ${TEST_CLASS} > output.txt  2>&1
cat output.txt

function assert()
{
    line=`cat output.txt | grep ThreadDump | wc -l`
    echo $line
    if [[ $line -eq "2" ]]; then
        echo "success"
    else
        echo "failure"
        exit -1
    fi
}

assert
