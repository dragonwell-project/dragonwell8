#
# Copyright (c) 2023 Alibaba Group Holding Limited. All Rights Reserved.
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
# @summary test the default value of ReservedCodeCacheSize. It should be 240m
# @run shell TestDefaultCodeCacheSize.sh 
#

${TESTJAVA}/bin/java -XX:+PrintFlagsFinal -version | grep ReservedCodeCacheSize |grep 251658240

if [ "0" == $? ];
then
    echo "ReservedCodeCacheSize is 240m"
    echo "--- Test passed"
else
    echo "ReservedCodeCacheSize is not 240m"
    echo "--- Test failed"
    exit 1
fi
