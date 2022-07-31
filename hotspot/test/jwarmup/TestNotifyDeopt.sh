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
# @summary test notifyJVMDeoptWarmUpMethods API
# @run shell TestNotifyDeopt.sh

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
TEST_CLASS=TmpTestNotifyDeopt
TEST_SOURCE=${TEST_CLASS}.java

###################################################################################
cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import java.lang.reflect.Method;
import java.util.*;
import com.alibaba.jwarmup.*;

public class TmpTestNotifyDeopt {
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

  public static void doBiz() {
    try {
      TmpTestNotifyDeopt a = new TmpTestNotifyDeopt();
      a.foo();
      Thread.sleep(10000);
      a.foo();
      System.out.println("process is done!");
    } catch( Exception ex ){
      ex.printStackTrace();
      System.exit(-1);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("recording")) {
      doBiz();
    } else if (args[0].equals("compilation")) {
      JWarmUp.notifyApplicationStartUpIsDone();
      while (!JWarmUp.checkIfCompilationIsComplete()) {
        Thread.sleep(1000);
      }
      System.out.println("the JWarmUp Compilation is Done, notify jvm to deoptimize warmup methods");
      new Thread(()->doBiz());
      JWarmUp.notifyJVMDeoptWarmUpMethods();
      Thread.sleep(6000);
      System.gc();
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
${JAVA} -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUpRecording -XX:-ClassUnloading -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpRecordTime=10 -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_CLASS} recording > output.txt  2>&1
sleep 1
${JAVA} -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUp -XX:+PrintCompilationWarmUpDetail -XX:CompileThreshold=500000 -XX:CompilationWarmUpLogfile=./jitwarmup.log -XX:+CompilationWarmUpExplicitDeopt -XX:CompilationWarmUpDeoptTime=1200 -cp ${TESTCLASSES} ${TEST_CLASS} compilation > output.txt  2>&1

check_output()
{
  # check warning for conflict CompilationWarmUpDeoptTime
  deopt_time_warning=`grep "WARNING : CompilationWarmUpDeoptTime is unused" output.txt|wc -l`
  if [ $deopt_time_warning -ne 1 ]; then
    echo "err by deopt time $deopt_time_warning"
    exit -1
  fi

  # check warmup method is deoptimized
  deopt_method_messages=`grep "WARNING : deoptimize warmup method" output.txt|wc -l`
  if [ $deopt_method_messages -eq 0 ]; then
    echo "err by deopt messages $deopt_method_messages"
    exit -1
  fi

  # check every deoptimized method is compiled by warmup
  warmup_methods=`grep "preload method" output.txt|awk '{print $4}'`
  deopt_methods=`grep "WARNING : deoptimize warmup method" output.txt|awk '{print $7}'`
  echo "warmup:$warmup_methods"
  echo "deopt:$deopt_methods"
  for m in $deopt_methods
  do
    found=0
    for wm in $warmup_methods
    do
      if [ "${m}" = "${wm}" ]; then
        found=1
      fi
    done
    if [ $found -ne 1 ]; then
      echo "not found $m"
      exit 1
    fi
  done
  exit 0
}

check_output

