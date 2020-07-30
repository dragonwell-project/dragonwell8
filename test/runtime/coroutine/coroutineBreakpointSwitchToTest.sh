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

#
# @test
# @library /testlibrary
# @compile SimpleWispTest.java
#
# @summary test Coroutine SwitchTo() crash problem
# @requires os.family == "linux"
# @run shell coroutineBreakpointSwitchToTest.sh
#

OS=`uname -s`
case "$OS" in
  AIX | Darwin | Linux | SunOS )
    FS="/"
    ;;
  Windows_* )
    FS="\\"
    ;;
  CYGWIN_* )
    FS="/"
    ;;
  * )
    echo "Unrecognized system!"
    exit 1;
    ;;
esac

JAVA=${TESTJAVA}${FS}bin${FS}java

export LD_LIBRARY_PATH=.:${TESTJAVA}${FS}jre${FS}lib${FS}amd64${FS}server:${FS}usr${FS}lib:${LD_LIBRARY_PATH}

gcc_cmd=`which gcc`
if [ "x${gcc_cmd}" = "x" ]; then
    echo "WARNING: gcc not found. Cannot execute test." 2>&1
    exit 0
fi

gcc -DLINUX -fPIC -shared -o libtest.so \
    -I${COMPILEJAVA}/include -I${COMPILEJAVA}/include/linux \
    ${TESTSRC}/coroutineBreakpointSwitchToTest.c

${JAVA} -agentpath:libtest.so -XX:-UseBiasedLocking -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.transparentAsync=true -cp ${TESTCLASSES} SimpleWispTest

exit $?
