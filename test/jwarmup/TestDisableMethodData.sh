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
# @summary disable MethodData in compilation made by JWarmUp
# @run shell TestDisableMethodData.sh

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
TEST_CLASS=TmpTestDisableMethodData
TEST_SOURCE=${TEST_CLASS}.java

###################################################################################
cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import java.lang.reflect.Method;
import java.util.*;
import com.alibaba.jwarmup.*;

public class TmpTestDisableMethodData {
  public static class TmpBase {
    public void bar() { }
  }
  public static class TmpA extends TmpBase {
    public static long a = 0;
    @Override
    public void bar() { a++; }
  }
  public static class TmpB extends TmpBase {
    public static long b = 0;
    @Override
    public void bar() { b++; }
  }
  public static String[] aa = new String[0];
  public static List<String> ls = new ArrayList<String>();
  public String foo() {
    for (int i = 0; i < 22000; i++) {
      foo2(new TmpA());
    }
    foo2(new TmpB());
    ls.add("x");
    return ls.get(0);
  }

  public void foo2(TmpBase t) {
    if (t instanceof TmpB) {
      return;
    }
    String s = "aa";
    if (ls.size() > 2000) {
      ls.clear();
    } else {
      ls.add(s);
    }
  }

  public static void doBiz() throws Exception {
    TmpTestDisableMethodData a = new TmpTestDisableMethodData();
    a.foo();
    Thread.sleep(10000);
    a.foo();
    System.out.println("process is done!");
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("recording")) {
      doBiz();
    } else if (args[0].equals("compilation")) {
      // invoke foo2 once
      TmpTestDisableMethodData a = new TmpTestDisableMethodData();
      for (int i = 0; i < 9687; i++) {
        a.foo2(new TmpB());
      }
      System.gc();
      JWarmUp.notifyApplicationStartUpIsDone();
      while (!JWarmUp.checkIfCompilationIsComplete()) {
        Thread.sleep(1000);
      }
      // Here, a.foo2 will be compiled by JWarmup but without MethodData info.
      // and, wait for the deoptimization made by JWarmup.
      for (int i = 0; i < 5; i++) {
        Thread.sleep(4000);
        System.gc();
      }
      for (int i = 0; i < 19687; i++) {
        // Here, methods already be deoptimized by JWarmup.
        a.foo2(new TmpB());
      }
      Thread.sleep(1000);
      // a.foo2 will be compiled by C2 in optimal performance.
      System.gc();
      System.out.println("re-compilation done by normal C2");
      // deoptimization should occur as it is made by normal C2 compilation
      doBiz();
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
${JAVA} -XX:-Inline -XX:CompileCommand=exclude,*.foo -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUpRecording -XX:-ClassUnloading -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpRecordTime=10 -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_CLASS} recording > output.txt  2>&1
cat output.txt
echo "----------------------------------"
sleep 1
${JAVA} -XX:-Inline -XX:CompilationWarmUpDeoptTime=3 -XX:CompileCommand=exclude,*.foo -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUp -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_CLASS} compilation > output.txt  2>&1
cat output.txt

function assert()
{
  i=0
  notify_line_no=0
  while read line
  do
    i=$(($i+1))
    echo $i
    echo $line
    if [[ $line =~ "made not entrant" ]]; then
      deopt_line_no=$i
    fi
    if [[ $line =~ "re-compilation" ]]; then
      recom_line_no=$i
    fi
  done < output.txt
  if [[ $deopt_line_no > $recom_line_no ]]; then
    echo "deoptimization happens after normal c2 compilation, it is OK."
    exit 0
  else
    echo "deopt_line_no is $deopt_line_no"
    echo "recom_line_no is $recom_line_no"
    exit -1
  fi
}

assert

