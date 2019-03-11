#
# Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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
# @summary Test 'jmap -dump:mini'
# @run shell/timeout=500 TestMiniDump.sh
#

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../../test_env.sh

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
JMAP=${TESTJAVA}${FS}bin${FS}jmap
JCMD=${TESTJAVA}${FS}bin${FS}jcmd
JPS=${TESTJAVA}${FS}bin${FS}jps
# A simple testcase used to invoke JVM
TEST_CLASS=Loop_$(date +%Y%m%d%H%M%S)
TEST_SOURCE=$TEST_CLASS.java

cat > $TEST_SOURCE << EOF
public class ${TEST_CLASS} {
    public static void main(String[] args) {
        // allocate 1G temp objects
        for (int i = 0; i < 1024 * 1024; ++i) {
            Object o = new byte[1024];
        }
        // keep Java process running
        while (true);
    }
}
EOF

# compile the test class
$JAVAC $TEST_SOURCE
if [ $? != '0' ]; then
    echo "Failed to compile Foo.java"
    exit 1
fi

${JAVA} -cp . -Xmx4g -Xms4g -Xmn2g ${TEST_CLASS}&

# wait child java process to allocate memory
sleep 5

PID=$(${JPS} | grep ${TEST_CLASS} | awk '{print $1}')
if [ $? != 0 ] || [ -z "${PID}" ]; then exit 1; fi

# full dump must be > 1G
FULL_DUMP_SIZE_THRESHOLD=$(( 1024 * 1024 * 1024))
# mini dump must be < 30MB
MINI_DUMP_SIZE_THRESHOLD=$(( 30 * 1024 * 1024))

# Test of full heap dump
DUMP="full_heap.bin"
${JMAP} -dump:format=b,file=${DUMP} ${PID}
if [ $? != 0 ] || [ ! -f "${PWD}/${DUMP}" ]; then exit 1; fi

SIZE=$(ls -l | grep ${DUMP} | awk '{print $5}')
if [ $? != 0 ] || [ ${SIZE} -le "${FULL_DUMP_SIZE_THRESHOLD}" ]; then
  echo "Full heap dump is too small"
  exit 1
fi

# full heap dump from jcmd
DUMP="full_heap2.bin"
${JCMD} ${PID} GC.heap_dump -all ${DUMP}
if [ $? != 0 ] || [ ! -f "${PWD}/${DUMP}" ]; then exit 1; fi

SIZE=$(ls -l | grep ${DUMP} | awk '{print $5}')
if [ $? != 0 ] || [ ${SIZE} -lt "${FULL_DUMP_SIZE_THRESHOLDL}" ]; then
  echo "Full heap dump is too small"
  exit 1
fi

# Test of mini heap dump
DUMP="mini_heap.bin"
${JMAP} -dump:format=b,mini,file=${DUMP} ${PID}
if [ $? != 0 ] || [ ! -f "${PWD}/${DUMP}" ]; then exit 1; fi
SIZE=$(ls -l | grep ${DUMP} | awk '{print $5}')
if [ $? != 0 ] || [ ${SIZE} -ge "${MINI_DUMP_SIZE_THRESHOLD}" ]; then
  echo "Mini heap dump is too large"
  exit 1
fi

# minidump from jcmd
DUMP="mini_heap2.bin"
${JCMD} ${PID} GC.heap_dump -all -mini ${DUMP}
if [ $? != 0 ] || [ ! -f "${PWD}/${DUMP}" ]; then exit 1; fi
SIZE=$(ls -l | grep ${DUMP} | awk '{print $5}')
if [ $? != 0 ] || [ ${SIZE} -ge "${MINI_DUMP_SIZE_THRESHOLD}" ]; then
  echo "Mini heap dump is too large"
  exit 1
fi

# clean up
rm -f *.bin
if [ $? != 0 ]; then exit 1; fi
kill -9 ${PID}
if [ $? != 0 ]; then exit 1; fi

exit 0
