#!/bin/bash -xe

# @test JliLaunchTest.sh
# @bug 8238225
# @library /lib
# @build JliLaunchTest
# @run shell JliLaunchTest.sh

OS=`uname -s`
if [ "${OS}" != "Darwin" ]; then
    echo "This is a MacOSX only test"
    exit 0;
fi

gcc_cmd=`which gcc`
if [ "x$gcc_cmd" == "x" ]; then
    echo "WARNING: gcc not found. Cannot execute test." 2>&1
    exit 0;
fi

JDK_TOPDIR="${TESTROOT}/.."

$gcc_cmd -o ${TESTCLASSES}/JliLaunchTest \
    -I${JDK_TOPDIR}/src/share/bin \
    -I${JDK_TOPDIR}/src/share/javavm/export \
    -I${JDK_TOPDIR}/src/macosx/javavm/export \
    -I${JDK_TOPDIR}/src/solaris/bin \
    -L${TESTJAVA}/jre/lib/jli -ljli \
    ${TESTSRC}/exeJliLaunchTest.c

${TESTJAVA}/bin/java -Dtest.jdk=${TESTJAVA} \
    -Dtest.nativepath=${TESTCLASSES} \
    -cp ${TESTCLASSPATH} JliLaunchTest
