#!/bin/sh

#
# Copyright (c) 2019, Red Hat, Inc. All rights reserved.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
#

## @test test.sh
## @bug 8178870
## @summary test instrumentation.retransformClasses
## @run shell test.sh

if [ "${TESTSRC}" = "" ]
then
  TESTSRC=${PWD}
  echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../test_env.sh

LIB_SRC=${TESTSRC}/../../testlibrary/

# set platform-dependent variables
OS=`uname -s`
echo "Testing on " $OS
case "$OS" in
  Linux)
    cc_cmd=`which gcc`
    if [ "x$cc_cmd" == "x" ]; then
        echo "WARNING: gcc not found. Cannot execute test." 2>&1
        exit 0;
    fi
    ;;
  Solaris)
    cc_cmd=`which cc`
    if [ "x$cc_cmd" == "x" ]; then
        echo "WARNING: cc not found. Cannot execute test." 2>&1
        exit 0;
    fi
    ;;
  *)
    echo "Test passed. Only on Linux and Solaris"
    exit 0;
    ;;
esac

THIS_DIR=.

cp ${TESTSRC}/RedefineDoubleDelete.java ${THIS_DIR}
mkdir -p ${THIS_DIR}/classes
${TESTJAVA}/bin/javac -sourcepath  ${LIB_SRC} -d ${THIS_DIR}/classes ${LIB_SRC}RedefineClassHelper.java
${TESTJAVA}/bin/javac -cp .:${THIS_DIR}/classes:${TESTJAVA}/lib/tools.jar -d ${THIS_DIR} RedefineDoubleDelete.java

$cc_cmd -fPIC -shared -o ${THIS_DIR}${FS}libRedefineDoubleDelete.so \
    -I${TESTJAVA}/include -I${TESTJAVA}/include/linux \
    ${TESTSRC}/libRedefineDoubleDelete.c

LD_LIBRARY_PATH=${THIS_DIR}
echo   LD_LIBRARY_PATH = ${LD_LIBRARY_PATH}
export LD_LIBRARY_PATH

# Install redefineagent.jar
${TESTJAVA}/bin${FS}java -cp ${THIS_DIR}/classes RedefineClassHelper

echo
echo ${TESTJAVA}/bin/java -agentlib:RedefineDoubleDelete RedefineDoubleDelete
${TESTJAVA}/bin/java -cp .:${THIS_DIR}${FS}classes -javaagent:redefineagent.jar -agentlib:RedefineDoubleDelete RedefineDoubleDelete

