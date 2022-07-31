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
# @summary test JWarmUp deoptimize will skip methods compiled by JIT
# @run shell TestNotDeoptJITMethod.sh

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
TEST_CLASS=TmpTestNotDeoptJITMethod
TEST_SOURCE=${TEST_CLASS}.java

###################################################################################
cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import java.lang.reflect.Method;
import java.util.*;
import com.alibaba.jwarmup.*;

public class TmpTestNotDeoptJITMethod {
  public static String[] aa = new String[0];
  public static List<String> ls = new ArrayList<String>();
  public String foo(int mode) throws Exception {
    for (int i = 0; i < 12000; i++) {
      foo2(aa, mode, i);
    }
    ls.add("x");
    return ls.get(0);
  }

  public void foo2(String[] a, int mode, int index) throws Exception {
    String s = "aa";
    if (mode ==  1 && index == 10000) {
      a[index]=s; // out of range
    }
    if (ls.size() > 100 && a.length < 100) {
      ls.clear();
    } else {
      ls.add(s);
    }
  }

  public static void doBiz(int mode){
    TmpTestNotDeoptJITMethod a = new TmpTestNotDeoptJITMethod();
    try{
      a.foo(mode);
    } catch (Exception ex){
      ex.printStackTrace();
    }
    try{
      // re-warmup foo to let jit compile it
      a.foo(0);
    } catch (Exception ex){
      ex.printStackTrace();
    }
    try{
      Thread.sleep(10000);
      a.foo(0);
    } catch (Exception ex){
      ex.printStackTrace();
    }
    System.out.println("process is done!");
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("recording")) {
      doBiz(0);
    } else if (args[0].equals("compilation")) {
      JWarmUp.notifyApplicationStartUpIsDone();
      while (!JWarmUp.checkIfCompilationIsComplete()) {
        Thread.sleep(1000);
      }
      System.out.println("the JWarmUp Compilation is Done, notify jvm to deoptimize warmup methods");
      new Thread(()-> doBiz(1)).start();
      Thread.sleep(2000);
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
${JAVA} -XX:-TieredCompilation -XX:+CompilationWarmUpRecording -XX:-ClassUnloading -XX:-UseSharedSpaces -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpRecordTime=10 -XX:CompilationWarmUpLogfile=./jitwarmup.log -XX:CompileThreshold=3000 -cp ${TESTCLASSES} ${TEST_CLASS} recording > output_record.txt  2>&1
sleep 1
${JAVA} -XX:-TieredCompilation -XX:+CompilationWarmUp -XX:-UseSharedSpaces -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -Xbatch -XX:CompilationWarmUpLogfile=./jitwarmup.log -XX:+CompilationWarmUpExplicitDeopt -XX:CompilationWarmUpDeoptTime=1200 -XX:-UseOnStackReplacement -cp ${TESTCLASSES} ${TEST_CLASS} compilation > output.txt  2>&1

check_output()
{
  #TmpTestNotDeoptJITMethod.foo2 should be re-compiled by jit
  skip_messages=`grep "skip deoptimize TmpTestNotDeoptJITMethod.foo2" output.txt|wc -l`
  if [ 1 -ne $skip_messages ] ; then
    exit 1
  fi

  exit 0
}

check_output

