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
# @summary test issue 9780156
# @run shell issue9780156.sh

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
JAR=${TESTJAVA}${FS}bin${FS}jar
TEST_MAIN_CLASS=TmpTestMain
TEST_MAIN_SOURCE=${TEST_MAIN_CLASS}.java

TEST_CLASS_A=TmpClassA
TEST_SOURCE_A=${TEST_CLASS_A}.java

TEST_CLASS_B=TmpClassB
TEST_SOURCE_B=${TEST_CLASS_B}.java

JARNAME=testjar.jar

###################################################################################

cat > ${TESTCLASSES}${FS}$TEST_SOURCE_A << EOF
import java.util.*;
public class TmpClassA {
  static {
    System.out.println("init TmpClassA");
  }

  public static void fooA() {
    System.out.println(TmpClassB.class);
  }

  TmpClassB bb;
}
EOF

cat > ${TESTCLASSES}${FS}$TEST_SOURCE_B << EOF
public class TmpClassB {
  static {
    System.out.println("init TmpClassB");
  }
}

EOF

cat > ${TESTCLASSES}${FS}$TEST_MAIN_SOURCE << EOF
import java.lang.reflect.Method;
import java.util.*;
import java.io.*;
import java.net.*;

public class TmpTestMain {
  public static class TmpClassloader extends URLClassLoader {
    public TmpClassloader(URL[] urls) {
      super(urls);
    }

    public TmpClassloader(String dir) throws MalformedURLException {
      super(new URL[] { new URL("file:" + basePath + File.separatorChar + dir + File.separatorChar + jarName) });
    }

    public static String basePath = "${TESTCLASSES}";

    public static String jarName = "${JARNAME}";
  }

  public static void foo(String dir) throws Exception {
    TmpClassloader tcl = new TmpClassloader(dir);
    Class c1 = tcl.loadClass("TmpClassA");
    c1.newInstance();
    Class c2 = tcl.loadClass("TmpClassB");
    c2.newInstance();
  }

  public static void bar(String dir) throws Exception {
    TmpClassloader tcl = new TmpClassloader(dir);
    Class c1 = tcl.loadClass("TmpClassA");
    c1.newInstance();
  }

  public static void main(String[] args) throws Exception {
    try {
      foo("dir1");
      bar("dir2");
      Class c = Class.forName("com.alibaba.jwarmup.JWarmUp");
      Method m = c.getMethod("notifyApplicationStartUpIsDone");
      Method m2 = c.getMethod("checkIfCompilationIsComplete");
      m.invoke(null);
      while ((Boolean) m2.invoke(null) == false) {
        Thread.sleep(1000);
      }
      System.out.println("the JWarmUp Compilation is Done");
      System.out.println("Test Done");
      foo("dir2");
      Thread.sleep(6000);
    } catch (Exception e) {
      e.printStackTrace();
    }
  }
}
EOF

# Do compilation
${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_MAIN_SOURCE
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_MAIN_SOURCE}"
	exit 1
fi

${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE_A
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE_A}"
	exit 1
fi

${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE_B
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE_B}"
	exit 1
fi

rm -f ${JARNAME}

cp ${TESTCLASSES}${FS}${TEST_CLASS_A}.class .
cp ${TESTCLASSES}${FS}${TEST_CLASS_B}.class .

rm ${TESTCLASSES}${FS}${TEST_CLASS_A}.class
rm ${TESTCLASSES}${FS}${TEST_CLASS_B}.class

${JAR} cvf ${JARNAME} ${TEST_CLASS_A}.class ${TEST_CLASS_B}.class

mkdir -p ${TESTCLASSES}${FS}dir1${FS}
mkdir -p ${TESTCLASSES}${FS}dir2${FS}
cp ${JARNAME} ${TESTCLASSES}${FS}dir1${FS}
cp ${JARNAME} ${TESTCLASSES}${FS}dir2${FS}


#run Recording Model
${JAVA} -verbose:class -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUpRecording -XX:-ClassUnloading -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpRecordTime=5 -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_MAIN_CLASS} > output.txt  2>&1
sleep 1
${JAVA} -verbose:class -XX:-TieredCompilation -XX:-UseSharedSpaces -XX:+CompilationWarmUp -XX:+PrintCompilation -XX:+PrintCompilationWarmUpDetail -XX:CompilationWarmUpLogfile=./jitwarmup.log -cp ${TESTCLASSES} ${TEST_MAIN_CLASS} > output.txt  2>&1

function assert()
{
  i=0
  while read line
  do
    echo $line
    if [[ $line =~ "Loaded TmpClassB" ]]; then
      i=$(($i+1))
      echo $i
    fi
  done < output.txt
  if [[ $i == 3 ]]; then
    exit 0
  else
    exit -1
  fi
}

assert

