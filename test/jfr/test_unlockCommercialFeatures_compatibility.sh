#!/usr/bin/env bash
#
# @test
# @summary Test 'jcmd VM.unlock_commercial_features'
# @run shell/timeout=500 test_unlockCommercialFeatures_compatibility.sh
#

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../test_env.sh

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
JCMD=${TESTJAVA}${FS}bin${FS}jcmd
JPS=${TESTJAVA}${FS}bin${FS}jps

# A simple testcase used to invoke JVM
TEST_CLASS=Test_$(date +%Y%m%d%H%M%S)
TEST_SOURCE=$TEST_CLASS.java

cat > $TEST_SOURCE << EOF
public class ${TEST_CLASS} {
    public static void main(String[] args) throws Exception{
        // keep Java process running
        while (true) { Thread.sleep(1000); }
    }
}
EOF

# compile the test class
$JAVAC $TEST_SOURCE
if [ $? != '0' ]; then
    echo "Failed to compile ${TEST_SOURCE}"
    exit 1
fi

${JAVA} -XX:+EnableJFR -cp . ${TEST_CLASS}&

PID=$(${JPS} | grep ${TEST_CLASS} | awk '{print $1}')
if [ $? != 0 ] || [ -z "${PID}" ]; then exit 1; fi

${JCMD} ${PID} VM.unlock_commercial_features
if [ $? != 0 ]; then exit 1; fi

kill -9 ${PID}
exit 0
