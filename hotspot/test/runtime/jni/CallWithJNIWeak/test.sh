#!/bin/sh

#
#  Copyright (c) 1998, 2019, Oracle and/or its affiliates. All rights reserved.
#  Copyright (c) 2011 SAP AG.  All Rights Reserved.
#  DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
#  This code is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 only, as
#  published by the Free Software Foundation.
#
#  This code is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#  version 2 for more details (a copy is included in the LICENSE file that
#  accompanied this code).
#
#  You should have received a copy of the GNU General Public License version
#  2 along with this work; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
#  Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
#  or visit www.oracle.com if you need additional information or have any
#  questions.
#

## @test test.sh
## @bug 8166188
## @requires vm.opt.ExplicitGCInvokesConcurrent != true
## @summary Test call of native function with JNI weak global ref.
## @run shell test.sh

if [ "${TESTSRC}" = "" ]
then
  TESTSRC=${PWD}
  echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../../test_env.sh

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
    echo "Test passed; only valid for Linux and Solaris"
    exit 0;
    ;;
esac

THIS_DIR=.

cp ${TESTSRC}${FS}*.java ${THIS_DIR}
${TESTJAVA}${FS}bin${FS}javac *.java

$cc_cmd -fPIC -shared -o libCallWithJNIWeak.so \
    -I${TESTJAVA}${FS}include -I${TESTJAVA}${FS}include${FS}linux \
    ${TESTSRC}${FS}CallWithJNIWeak.c

LD_LIBRARY_PATH=${THIS_DIR}
echo   LD_LIBRARY_PATH = ${LD_LIBRARY_PATH}
export LD_LIBRARY_PATH

echo
echo ${TESTJAVA}${FS}bin${FS}java -cp ${THIS_DIR} -Xint CallWithJNIWeak
${TESTJAVA}${FS}bin${FS}java -cp ${THIS_DIR} -Xint CallWithJNIWeak
JAVA_RETVAL=$?

if [ "$JAVA_RETVAL" == "0" ]
then
  echo
  echo ${TESTJAVA}${FS}bin${FS}java -cp ${THIS_DIR} -Xcomp CallWithJNIWeak
  ${TESTJAVA}${FS}bin${FS}java -cp ${THIS_DIR} -Xcomp CallWithJNIWeak

  JAVA_RETVAL=$?
fi

exit $JAVA_RETVAL
