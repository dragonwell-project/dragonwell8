#
# Copyright (c) 2023, Alibaba Group Holding Limited. All rights reserved.
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
#

#!/usr/bin/env bash
#
# @test
# @summary test JVMTI API for preparation of Delegating class loader
# @library /test/lib
# @modules jdk.compiler
# @modules java.base/jdk.internal.misc
# @requires os.arch=="amd64" | os.arch=="aarch64"
# @run shell testDelegatingClassLoaderJVMTI.sh
#

set -x

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi

echo "TESTSRC=${TESTSRC}"
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

if [ "x$(uname -m)" = "xaarch64" ];
then
  echo "Test only valid for x86"
  exit 0
fi

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
TEST_CLASS2=ThrowException
TEST_SOURCE2=${TEST_CLASS2}.java

TEST_CLASS=TestDelegatingClassLoader
TEST_SOURCE=${TEST_CLASS}.java

cat > ${TESTCLASSES}${FS}$TEST_SOURCE2 << EOF

public class ThrowException {
    public static void throwError() {
        throw new Error("testing");
    }
}
EOF

###################################################################################
cat > ${TESTCLASSES}${FS}$TEST_SOURCE << EOF
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.nio.file.Paths;

public class TestDelegatingClassLoader {
    private static final String TEST_SRC = System.getProperty("test.src");

    private static final Path SRC_DIR = Paths.get(TEST_SRC, "src");
    private static final Path CLASSES_DIR = Paths.get("classes");
    private static final String THROW_EXCEPTION_CLASS = "ThrowException";

    public static void main(String... args) throws Exception {
        testURLClassLoader("myloader");
        testDelegatingClassLoader();
    }

    public static void testURLClassLoader(String loaderName) throws Exception {
        System.err.println("---- test URLClassLoader name: " + loaderName);

        URL[] urls = new URL[] { CLASSES_DIR.toUri().toURL() };
        ClassLoader parent = ClassLoader.getSystemClassLoader();
        URLClassLoader loader = new URLClassLoader(urls, parent);

        Class<?> c = Class.forName(THROW_EXCEPTION_CLASS, true, loader);
        Method method = c.getMethod("throwError");
        try {
            // invoke ThrowException::throwError
            method.invoke(null);
        } catch (InvocationTargetException x) {
        }
    }

    private static void testDelegatingClassLoader() throws Exception {
        new Thread(() -> {
            try {
                Method m = Thread.class.getDeclaredMethod("nextThreadNum");
                for (int i = 0; i < 100; i++) {
                    m.setAccessible(true);
                    m.invoke(null);
                }
            } catch (ReflectiveOperationException e) {
                throw new Error(e);
            }
        }, "waiter").start();
        Thread.sleep(100);
    }
}
EOF

# Do compilation
${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE2 >> /dev/null 2>&1
if [ $? != '0' ]
then
    ls -la ${TESTCLASSES}${FS}
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE2}"
	exit 1
fi

${JAVAC} -cp ${TESTCLASSES} -d ${TESTCLASSES} ${TESTCLASSES}${FS}$TEST_SOURCE >> /dev/null 2>&1
if [ $? != '0' ]
then
	printf "Failed to compile ${TESTCLASSES}${FS}${TEST_SOURCE}"
	exit 1
fi

g++ -I${TESTJAVA}/include/ -I${TESTJAVA}/include/linux ${TESTSRC}/loadClassAgent.cpp ${TESTSRC}/testClassLoaderJVMTI.cpp -fPIC -shared -o ${TESTCLASSES}/libloadclassagent.so
if [ $? != '0' ]
then
	printf "Failed to generate so file"
	exit 1
fi

export LD_LIBRARY_PATH=.:${TESTCLASSES}:${LD_LIBRARY_PATH}
#run
${JAVA} -cp ${TESTCLASSES} -agentlib:loadclassagent ${TEST_CLASS} > output.txt  2>&1
cat output.txt

line=`cat output.txt | grep "LoadClassAgent::HandleLoadClass" | wc -l`
echo $line
if [ $line -eq "2" ]; then
    echo "success"
else
    echo "failure"
    exit -1
fi
