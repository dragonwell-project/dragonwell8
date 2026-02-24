#!/bin/sh

# Copyright (c) 2012, 2025, Oracle and/or its affiliates. All rights reserved.
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

# @test DynamicCodeGeneratedTest.sh
# @bug 8212155
# @summary Test concurrent enabling and posting of DynamicCodeGenerated events.
# @run shell DynamicCodeGeneratedTest.sh

if [ "$TESTSRC" = "" ]
then TESTSRC=.
fi

if [ "$TESTJAVA" = "" ]
then
  PARENT=$(dirname $(which java))
  TESTJAVA=$(dirname $PARENT)
  echo "TESTJAVA not set, selecting " $TESTJAVA
  echo "If this is incorrect, try setting the variable manually."
fi

# set platform-dependent variables
OS=$(uname -s)
case "$OS" in
  Linux )
    ARCHFLAG=
    $TESTJAVA/bin/java -version 2>&1 | grep '64-Bit' > /dev/null
    if [ $? -eq '0' ]
    then
        ARCH="amd64"
    else
        ARCH="i386"
        ARCHFLAG="-m32 -march=i386"
    fi
    ;;
  * )
    echo "Test passed, unrecognized system."
    exit 0;
    ;;
esac

echo "OS-ARCH is" linux-$ARCH
$TESTJAVA/jre/bin/java -fullversion 2>&1

which gcc >/dev/null 2>&1
if [ "$?" -ne '0' ]
then
    echo "No C compiler found. Test passed."
    exit 0
fi

JAVA=$TESTJAVA/jre/bin/java
JAVAC=$TESTJAVA/bin/javac
JAVAH=$TESTJAVA/bin/javah

$JAVAC -d . $TESTSRC/DynamicCodeGenerated.java
$JAVAH -jni -classpath . -d . DynamicCodeGenerated

gcc --shared $ARCHFLAG -fPIC -O -I$TESTJAVA/include -I$TESTJAVA/include/linux -I. -o libDynamicCodeGenerated.so $TESTSRC/libDynamicCodeGenerated.cpp

LD_LIBRARY_PATH=. $JAVA -classpath . -agentlib:DynamicCodeGenerated DynamicCodeGenerated

exit $?
