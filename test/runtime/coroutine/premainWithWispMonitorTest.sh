#!/bin/sh

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

# see PremainWithWispMonitorTest.java

AGENT=PremainWithWispMonitorTest

if [ "${TESTSRC}" = "" ]
then
  echo "TESTSRC not set.  Test cannot execute.  Failed."
  exit 1
fi
echo "TESTSRC=${TESTSRC}"

if [ "${TESTJAVA}" = "" ]
then
  echo "TESTJAVA not set.  Test cannot execute.  Failed."
  exit 1
fi
echo "TESTJAVA=${TESTJAVA}"


cp ${TESTSRC}/${AGENT}.java .
${TESTJAVA}/bin/javac ${TESTJAVACOPTS} ${TESTTOOLVMOPTS} PremainWithWispMonitorTest.java

echo "Manifest-Version: 1.0"    >  ${AGENT}.mf
echo Premain-Class: ${AGENT} >> ${AGENT}.mf
shift
while [ $# != 0 ] ; do
  echo $1 >> ${AGENT}.mf
  shift
done

${TESTJAVA}/bin/jar ${TESTTOOLVMOPTS} cvfm ${AGENT}.jar ${AGENT}.mf ${AGENT}*.class

${TESTJAVA}/bin/java -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -Dcom.alibaba.transparentAsync=true -XX:+UseWispMonitor \
        -javaagent:PremainWithWispMonitorTest.jar PremainWithWispMonitorTest
