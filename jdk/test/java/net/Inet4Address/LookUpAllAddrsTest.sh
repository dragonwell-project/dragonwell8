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
# @test LookUpAllAddrsTest.sh
# @summary test calling InetAddress.getAllByName() without triggering DNS reverse resolution
# @run shell/timeout=30 LookUpAllAddrsTest.sh
#

# NOTE: this test only works on RPM based Linux distributions

set -x

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
FS=/
JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${TESTJAVA}${FS}bin${FS}javac
TEST_CLASS=T
TEST_SOURCE=${TEST_CLASS}.java

# pre-condition checking
which gdb
if [ $? != 0 ]
then
    echo "Cannot find GDB, please install it!"
    exit 0
fi

which rpm
if [ $? != 0 ]
then
    echo Cannot find tool 'rpm', unsupported platform
    exit 0
fi

GLIBC_VER_MAJOR=$(rpm -qa | sort | uniq | perl -e 'for (<>) { if ($_ =~ /^glibc-(\d+)\.(\d+)\D\S+/ ) { print "$1\n"; exit; } }')
GLIBC_VER_MINOR=$(rpm -qa | sort | uniq | perl -e 'for (<>) { if ($_ =~ /^glibc-(\d+)\.(\d+)\D\S+/ ) { print "$2\n"; exit; } }')

# Prepare class
cat >> $TEST_SOURCE << EOF
import java.net.*;
import java.util.*;

public class T {
    static {
        System.setProperty("sun.net.inetaddr.ttl", "0");
        System.setProperty("sun.net.inetaddr.negative.ttl", "0");
    }
    public static void main(String[] args) throws InterruptedException, UnknownHostException {
        System.out.println("Test begin!");
        for (int i = 0; i < 10; ++i) {
            long t = System.currentTimeMillis();
            InetAddress[] addresses = InetAddress.getAllByName("tbapi.alipay.com");
            System.out.println("COST:" + (System.currentTimeMillis() - t) + " " + Arrays.toString(addresses));
        }
    }
}
EOF

# GDB command file to verify the invocation path
cat >> debug.gdb << EOF
# needed options
handle SIGSEGV nostop pass noprint
handle SIGPIPE nostop pass noprint
set breakpoint pending on

# set up breakpoints to print callflow info
break getaddrinfo
command
    print "#####getaddrinfo"
    continue
end

break gethostbyname_r
command
    print "#####gethostbyname_r"
    continue
end

# start execution and exit
run
quit

EOF

# Do compilation
${JAVAC} -g $TEST_SOURCE
if [ $? != '0' ]
then
	printf "Failed to compile $TEST_SOURCE"
	exit 1
fi

# Run test
if [ $GLIBC_VER_MAJOR -ge 2 ] && [ $GLIBC_VER_MINOR -ge 12 ]
then
    # GLIBC_VER >= 2.12, using getaddrinfo
    if [ "x" == "x`gdb --command=debug.gdb --arg $JAVA $TEST_CLASS 2>&1 | grep '#####getaddrinfo'`" ]
    then
        echo "Failed!"
        exit 1
    fi

    if [ "x" != "x`gdb --command=debug.gdb --arg $JAVA $TEST_CLASS 2>&1 | grep '#####gethostbyname_r'`" ]
    then
        echo "Failed!"
        exit 1
    fi
else
    # GLIBC_VER < 2.12, using gethostbyname_r
    if [ "x" != "x`gdb --command=debug.gdb --arg $JAVA $TEST_CLASS 2>&1 | grep '#####getaddrinfo'`" ]
    then
        echo "Failed!"
        exit 1
    fi

    if [ "x" == "x`gdb --command=debug.gdb --arg $JAVA $TEST_CLASS 2>&1 | grep '#####gethostbyname_r'`" ]
    then
        echo "Failed!"
        exit 1
    fi
fi


