#!/bin/sh
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

# @test
# @summary test TieredCompilation in JWarmUp recording phase
# @run shell TestTieredCompilationInRecording.sh

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
TEST_CLASS=TmpTestTieredCompilationInRecording
TEST_SOURCE=${TEST_CLASS}.java

###################################################################################
cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import java.lang.reflect.Method;
import java.util.*;
import com.alibaba.jwarmup.*;

public class TmpTestTieredCompilationInRecording {
  public static String[] aa = new String[0];
  public static List<String> ls = new ArrayList<String>();
  public String foo() {
    for (int i = 0; i < 12000; i++) {
      foo2(aa);
    }
    ls.add("x");
    return ls.get(0);
  }

  public void foo2(String[] a) {
    String s = "aa";
    if (ls.size() > 100 && a.length < 100) {
      ls.clear();
    } else {
      ls.add(s);
    }
  }

  public static void doBiz() throws Exception {
    TmpTestTieredCompilationInRecording a = new TmpTestTieredCompilationInRecording();
    a.foo();
    Thread.sleep(1000);
    a.foo();
    a.foo();
    Thread.sleep(1000);
    a.foo();
    Thread.sleep(1000);
    a.foo();
    Thread.sleep(10000);
    a.foo();
    System.out.println("process is done!");
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("collection")) {
      doBiz();
    } else if (args[0].equals("startup")) {
      JWarmUp.notifyApplicationStartUpIsDone();
      if (!JWarmUp.checkIfCompilationIsComplete()) {
        Thread.sleep(1000);
      }
      System.out.println("the JWarmUp Compilation is Done");
      System.out.println("Test Done");
    }
  }
}
EOF

# Do compilation
${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE >> /dev/null 2>&1
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE}"
	exit 1
fi

#run Recording Model
${JAVA} -XX:+CompilationWarmUpRecording -XX:-ClassUnloading -XX:-UseSharedSpaces -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpRecordTime=10 -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_CLASS} collection > output.txt.recording  2>&1
sleep 1
${JAVA} -XX:-TieredCompilation -XX:+CompilationWarmUp -XX:-UseSharedSpaces -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_CLASS} startup > output.txt  2>&1

function assert()
{
  i=0
  has_foo2=0
  while read line
  do
    i=$(($i+1))
    echo $i
    echo $line
    if [[ $line =~ "foo2" ]]; then
      has_foo2=$i
    fi
  done < output.txt
  if [[ $has_foo2 != 0 ]]; then
    exit 0
  else
    exit -1
  fi
}

assert

