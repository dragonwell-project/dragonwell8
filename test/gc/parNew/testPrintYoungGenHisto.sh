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
# @summary Test for the young generation histogram once after a parnew gc 
# @build TestPrintYoungGenHistoAfterParNewGC
# @run shell testPrintYoungGenHisto.sh

TMP1=tmp_$$

# testing set the VM flag PrintYoungGenHistoAfterParNewGC on the fly by using vm tools(jinfo -flag)
printYongGenHistoTest() {
    echo "Tetsting setting the PrintYoungGenHistoAfterParNewGC flag by using the vm tools(jinfo -flag)"
    ${TESTJAVA}/bin/java ${TESTVMOPTS}  -Xmx1g -Xmn500m -verbose:gc -XX:+UseConcMarkSweepGC -XX:+PrintGCDetails \
                         -cp ${TESTCLASSES} TestPrintYoungGenHistoAfterParNewGC  >${TMP1} 2>&1 &
    PID=$(ps aux | grep TestPrintYoungGenHistoAfterParNewGC | grep -v "grep" | awk '{print $2}')
    echo "hello"
    echo $PID
    ${TESTJAVA}/bin/jinfo -flag +PrintYoungGenHistoAfterParNewGC $PID
    sleep 1s
    cat ${TMP1}
    RESULT=$(cat ${TMP1} | grep -o 'Total' | wc -l)
    if [ $RESULT -gt 0 ]
    then echo "--- passed as expected "
    else
        echo "--- failed"
        exit 1
    fi

    kill -9 $PID
}

printYongGenHistoTest


rm ${TMP1}
