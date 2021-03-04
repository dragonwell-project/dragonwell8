#!/bin/sh

# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
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

##
## @test Test8233197.sh
## @bug 8233197
## @summary Check that JFR subsystem can be initialized from VMStart JVMTI event
## @compile T.java
## @run shell Test8233197.sh
##

set -x
if [ "${TESTSRC}" = "" ]
then
  TESTSRC=${PWD}
  echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../test_env.sh

# set platform-dependent variables
OS=`uname -s`
case "$OS" in
  Linux)
    gcc_cmd=`which gcc`
    if [ "x$gcc_cmd" == "x" ]; then
        echo "WARNING: gcc not found. Cannot execute test." 2>&1
        exit 0;
    fi
    NULL=/dev/null
    PS=":"
    FS="/"
    ;;
  * )
    echo "Test passed; only valid for Linux"
    exit 0;
    ;;
esac

${TESTJAVA}${FS}bin${FS}java ${TESTVMOPTS} -Xinternalversion > vm_version.out 2>&1

# Bitness:
# Cannot simply look at TESTVMOPTS as -d64 is not
# passed if there is only a 64-bit JVM available.

grep "64-Bit" vm_version.out > ${NULL}
if [ "$?" = "0" ]
then
  COMP_FLAG="-m64"
else
  COMP_FLAG="-m32"
fi


# Architecture:
# Translate uname output to JVM directory name, but permit testing
# 32-bit x86 on an x64 platform.
ARCH=`uname -m`
case "$ARCH" in
  x86_64)
    if [ "$COMP_FLAG" = "-m32" ]; then
      ARCH=i386
    else
      ARCH=amd64
    fi
    ;;
  ppc64)
    if [ "$COMP_FLAG" = "-m32" ]; then
      ARCH=ppc
    else
      ARCH=ppc64
    fi
    ;;
  sparc64)
    if [ "$COMP_FLAG" = "-m32" ]; then
      ARCH=sparc
    else
      ARCH=sparc64
    fi
    ;;
  arm*)
    # 32-bit ARM machine: compiler may not recognise -m32
    COMP_FLAG=""
    ARCH=arm
    ;;
  aarch64)
    # 64-bit arm machine, could be testing 32 or 64-bit:
    if [ "$COMP_FLAG" = "-m32" ]; then
      ARCH=arm
    else
      ARCH=aarch64
    fi
    ;;
  i586)
    ARCH=i386
    ;;
  i686)
    ARCH=i386
    ;;
  # Assuming other ARCH values need no translation
esac


# VM type: need to know server or client
VMTYPE=client
grep Server vm_version.out > ${NULL}
if [ "$?" = "0" ]
then
  VMTYPE=server
fi


LD_LIBRARY_PATH=.:${COMPILEJAVA}/jre/lib/${ARCH}/${VMTYPE}:/usr/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

cp ${TESTSRC}${FS}libJvmtiAgent.c .

# Copy the result of our @compile action:
cp ${TESTCLASSES}${FS}T.class .

echo "Architecture: ${ARCH}"
echo "Compilation flag: ${COMP_FLAG}"
echo "VM type: ${VMTYPE}"

$gcc_cmd -DLINUX ${COMP_FLAG} -Wl, -g -fno-strict-aliasing -fPIC -fno-omit-frame-pointer -W -Wall  -Wno-unused -Wno-parentheses -c -o libJvmtiAgent.o \
    -I${COMPILEJAVA}/include -I${COMPILEJAVA}/include/linux \
    -L${COMPILEJAVA}/jre/lib/${ARCH}/${VMTYPE} \
    libJvmtiAgent.c
$gcc_cmd -shared -o libJvmtiAgent.so libJvmtiAgent.o

"$TESTJAVA/bin/java" $TESTVMOPTS -agentlib:JvmtiAgent -cp $(pwd) T > T.out
exit $?