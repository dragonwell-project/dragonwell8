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
# @test TestParGCAllocatorLeak.sh
# @summary test memory leak of G1ParGCAllocator
# @run shell TestParGCAllocatorLeak.sh
#

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
# Adding common setup Variables for running shell tests.
. ${TESTSRC}/../test_env.sh

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
JCMD=${TESTJAVA}${FS}bin${FS}jcmd
TEST_CLASS=TestLeak
TEST_SRC=$TEST_CLASS.java

###################################################################################
cat > $TEST_SRC << EOF
import com.alibaba.tenant.*;
import java.util.*;

class $TEST_CLASS {
  public static void main(String[] args) {
    int nofCpus = Runtime.getRuntime().availableProcessors();
    Thread[] threads = new Thread[nofCpus];
    for (int i = 0; i < nofCpus; ++i) {
      threads[i] = new Thread(()->{
        TenantContainer tenant = TenantContainer.create(new TenantConfiguration().limitHeap(32 * 1024 * 1024));
        try {
          tenant.run(()->{
            while (true) {
              Object o = new byte[1024];
            }
          });
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      });
      threads[i].start();
    }

    Arrays.stream(threads).forEach(t->{
      try {
        t.join();
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    });
  }
}
EOF

# Do compilation
${JAVAC} ${TEST_SRC}
if [ $? != '0' ]
then
	echo "Failed to compile ${TEST_SRC}"
	exit 1
fi

set -x

${JAVA} -cp . -XX:+UseG1GC -XX:+MultiTenant -XX:+TenantHeapIsolation -XX:NativeMemoryTracking=detail -XX:+PrintGCDetails -Xloggc:gc.log -Xmx1g -Xmn32m ${TEST_CLASS} > ${TEST_CLASS}.log 2>&1 &
sleep 5
PID=$(ps ax | grep ${TEST_CLASS} | grep -v grep | awk '{print $1}')
if [ -z "$PID" ] && [ "$(echo $PID | wc -w)" -gt 1 ] ; then
  echo "BAD pid!"
  exit 1
fi

# set up baseline
$JCMD $PID VM.native_memory baseline

# sleep 30s
sleep 30

# check differences of below sections
NMT_SECTIONS=("Internal" "Tenant")

for MEM_SEC in ${NMT_SECTIONS[*]}; do
  DIFF=$($JCMD $PID VM.native_memory summary.diff | grep -A3 ${MEM_SEC} | grep malloc | grep -v grep)
  if [ ! -z "$(echo $DIFF | grep +)" ] && [ -z "$(echo $DIFF | awk '{print $2}' | grep \#)" ]; then
    DIFF=$(echo $DIFF | awk '{print $2}')
    echo "DIFF=$DIFF"
    if [ ! -z "$(echo $DIFF | grep KB)" ]; then
      # only check result if diff >= 1kb
      DIFF_V="$(echo $DIFF | sed -e 's/KB//g' -e 's/+//g' -e 's/-//g')"
      if [ -z $DIFF_V]; then
        echo "Bad diff value $DIFF_V"
        kill -9 $PID
        exit 1
      fi
      if [ $DIFF_V -gt 1024 ]; then
        echo "Diff value is great than 1 MB, maybe leaking!!"
        kill -9 $PID
        exit 1
      fi
    fi

    # just checking
    if [ ! -z "$(echo $DIFF | grep MB)" ] || [ ! -z "$(echo $DIFF | grep GB)" ]; then
      echo "Cannot in MB or GB scale mode"
      kill -9 $PID
      exit 1
    fi
  else
    echo "No significant memory size changed, skipping"
  fi
done

kill -9 $PID
