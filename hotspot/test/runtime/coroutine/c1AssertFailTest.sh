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

## @test
##
## @summary test c1 assertion failure when UseDirectUnpark is enabled (please run it in slowdebug ver.)
## @requires os.family == "linux"
## @run shell c1AssertFailTest.sh

set -x
version=`${TESTJAVA}/bin/java -version 2>&1`
fastdebug=`echo ${version} | grep "fastdebug"`
# fastdebug mode
if [ "$fastdebug" != "" ];then
    echo "fastdebug mode"
    exit 0
fi
debug=`echo ${version} | grep "debug"`
# release mode
if [ "$debug" = "" ];then
    echo "release mode"
    exit 0
fi
# slowdebug mode
time ${TESTJAVA}/bin/java -Xcomp -XX:TieredStopAtLevel=1 -XX:-UseBiasedLocking -XX:+UnlockExperimentalVMOptions -XX:+EnableCoroutine -XX:+UseWispMonitor -Dcom.alibaba.transparentAsync=true &

PID=$!

sleep 2
ls -d /proc/$PID || exit 1 # process exited, fail

kill -KILL $PID

exit 0
