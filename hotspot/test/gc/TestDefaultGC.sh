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
# @test
# @summary test the default gc
# @run shell TestDefaultGC.sh
#


arch=$(uname -m)
if [[ ${arch} == 'aarch64' ]]; then
${TESTJAVA}/bin/java -XX:+PrintFlagsFinal -version | grep UseParallelGC |grep true
else
${TESTJAVA}/bin/java -XX:+PrintFlagsFinal -version | grep UseConcMarkSweepGC |grep true
fi

if [ "0" == $? ];
then
    echo "CMS is Default GC"
    echo "--- Test passed"
else
    echo "CMS is not Default GC"
    echo "--- Test failed"
    exit 1
fi
