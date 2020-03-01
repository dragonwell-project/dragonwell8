#!/usr/bin/env bash
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
# @test TestTenantJVMOptions
# @summary Test the dependencies of multi-tenant JVM options
# @run shell TestTenantJVMOptions.sh
#

if [ "${TESTSRC}" = "" ]
then
    TESTSRC=${PWD}
    echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../test_env.sh

JAVA=${TESTJAVA}${FS}bin${FS}java

set -x

# if $FROM is enabled, $TO should be enabled automatically
function check_dependency_bool_bool() {
  FROM=$1
  TO="$(echo $2 | sed 's/-XX:+//g')"
  if [ -z "$(${JAVA} ${FROM} -XX:+PrintFlagsFinal -version 2>&1 | grep ${TO} | grep '= true')" ]; then
    echo "check_dependency_bool_bool failed: $1 --> $2"
    exit 1
  fi
}

function check_dependency_bool_bool_false() {
  FROM=$1
  TO="$(echo $2 | sed 's/-XX:+//g')"
  if [ -z "$(${JAVA} ${FROM} -XX:+PrintFlagsFinal -version 2>&1 | grep ${TO} | grep '= false')" ]; then
    echo "check_dependency_bool_bool failed: $1 --> $2"
    exit 1
  fi
}

check_dependency_bool_bool '-XX:+UseG1GC -XX:+TenantHeapIsolation' '-XX:+MultiTenant'
check_dependency_bool_bool '-XX:+UseG1GC -XX:+TenantHeapThrottling' '-XX:+MultiTenant'
check_dependency_bool_bool '-XX:+UseG1GC -XX:+TenantHeapThrottling' '-XX:+TenantHeapIsolation'
check_dependency_bool_bool '-XX:+UseG1GC -XX:+TenantHeapIsolation -XX:+UseTLAB' '-XX:+UsePerTenantTLAB'

# check if provided jvm arguments is invalid
function assert_invalid_jvm_options() {
  JVM_ARGS=$1
  CMD="${JAVA} ${JVM_ARGS} -version"
  OUT=$(${CMD} 2>&1)
  if [ 0 -eq $? ]; then
    echo "Expected invalid JVM arguments: ${JVM_ARGS}"
    exit 1
  fi
}

assert_invalid_jvm_options '-XX:+TenantHeapIsolation'
assert_invalid_jvm_options '-XX:+TenantHeapIsolation -XX:+UseConcMarkSweepGC'
assert_invalid_jvm_options '-XX:+TenantHeapThrottling'
assert_invalid_jvm_options '-XX:+UseG1GC -XX:+TenantHeapIsolation -XX:-MultiTenant'
assert_invalid_jvm_options '-XX:+UseG1GC -XX:+TenantHeapThrottling -XX:-TenantHeapIsolation'
assert_invalid_jvm_options '-XX:+UseG1GC -XX:+UsePerTenantTLAB -XX:+TenantHeapThrottling -XX:-UseTLAB'
assert_invalid_jvm_options '-XX:+UseG1GC -XX:+UsePerTenantTLAB -XX:+UseTLAB -XX:-TenantHeapThrottling'
