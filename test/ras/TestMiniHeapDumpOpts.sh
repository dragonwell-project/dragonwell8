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
# @summary Test jmap options related to mini heap dump
# @library /lib/testlibrary
# @run shell/timeout=300 TestMiniHeapDumpOpts.sh
#

set -x

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

JMAP=${TESTJAVA}${FS}bin${FS}jmap
JAVAC=${TESTJAVA}${FS}bin${FS}javac
JAVA=${TESTJAVA}${FS}bin${FS}java
JPS=${TESTJAVA}${FS}bin${FS}jps

# Basic options
if [ ! -f ${JMAP} ]; then
  echo "Cannot find jmap!"
  exit 1
fi

if [ -z "$(${JMAP} -h 2>&1 | grep 'mini         use minidump format (Dragonwell only)')" ]; then
  echo "Cannot find minidump option from 'jmap -h'"
  exit 1
fi

# Test if -dump:mini options works without error
cat > Loop.java <<EOF
public class Loop {
  public static void main(String[] args) {
    while(true);
  }
}
EOF
${JAVAC} Loop.java
if [ $? != 0 ]; then exit 1; fi

${JAVA} -cp . Loop&
PID=$(${JPS} | grep 'Loop' | awk '{print $1}')
if [ $? != 0 ] || [ -z "${PID}" ]; then exit 1; fi
${JMAP} -dump:format=b,mini,file=heap.bin ${PID}
if [ $? != 0 ] || [ ! -f "${PWD}/heap.bin" ]; then exit 1; fi

kill -9 ${PID}
if [ $? != 0 ]; then exit 1; fi

exit 0
